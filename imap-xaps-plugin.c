/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Stefan Arentz <stefan@arentz.ca>
 * Copyright (c) 2017 Frederik Schwan <frederik dot schwan at linux dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <config.h>
#include <lib.h>
#include <ostream.h>
#include <ostream-unix.h>

#include "str.h"
#include "strescape.h"
#include "imap-common.h"
#include "mail-storage-private.h"


#ifdef DOVECOT_ABI_VERSION
const char *xapplepushservice_plugin_version = DOVECOT_ABI_VERSION;
#else
const char *xapplepushservice_plugin_version = DOVECOT_VERSION;
#endif


static struct module *imap_xaps_module;
static imap_client_created_func_t *next_hook_client_created;


/**
 * Quote and escape a string. Not sure if this deals correctly with
 * unicode in mailbox names.
 */

static void xaps_str_append_quoted(string_t *dest, const char *str) {
    str_append_c(dest, '"');
    str_append(dest, str_escape(str));
    str_append_c(dest, '"');
}


/**
 * Send a registration request to the daemon, which will do all the
 * hard work.
 */

static int xaps_register(const char *socket_path, const char *aps_account_id, const char *aps_device_token,
                         const char *aps_subtopic, const char *dovecot_username,
                         const struct imap_arg *dovecot_mailboxes, string_t *aps_topic) {
    int ret = -1;

    /*
     * Construct our request.
     */

    string_t *req = t_str_new(1024);
    str_append(req, "REGISTER");
    str_append(req, " aps-account-id=");
    xaps_str_append_quoted(req, aps_account_id);
    str_append(req, "\taps-device-token=");
    xaps_str_append_quoted(req, aps_device_token);
    str_append(req, "\taps-subtopic=");
    xaps_str_append_quoted(req, aps_subtopic);
    str_append(req, "\tdovecot-username=");
    xaps_str_append_quoted(req, dovecot_username);
    str_append(req, "");

    if (dovecot_mailboxes == NULL) {
        str_append(req, "\tdovecot-mailboxes=(\"INBOX\")");
    } else {
        str_append(req, "\tdovecot-mailboxes=(");
        int next = 0;
        for (; !IMAP_ARG_IS_EOL(dovecot_mailboxes); dovecot_mailboxes++) {
            const char *mailbox;
            if (!imap_arg_get_astring(&dovecot_mailboxes[0], &mailbox)) {
                return -1;
            }
            if (next) {
                str_append(req, ",");
            }
            xaps_str_append_quoted(req, mailbox);
            next = 1;
        }
        str_append(req, ")");
    }
    str_append(req, "\r\n");

    /*
     * Send the request to our daemon over a unix domain socket. The
     * protocol is very simple line based. We use an alarm to make sure
     * this request does not hang.
     */

    int fd = net_connect_unix(socket_path);
    if (fd == -1) {
        i_error("net_connect_unix(%s) failed: %m", socket_path);
        return -1;
    }

    net_set_nonblock(fd, FALSE);

    alarm(2);
    struct ostream *ostream = o_stream_create_unix(fd, (size_t)-1);
    o_stream_cork(ostream);
    o_stream_nsend(ostream, str_data(req), str_len(req));
    o_stream_uncork(ostream);
    {
        if (o_stream_flush(ostream) < 1) {
            i_error("write(%s) failed: %m", socket_path);
            ret = -1;
        } else {
            char res[1024];
            ret = net_receive(fd, res, sizeof(res) - 1);
            if (ret < 0) {
                i_error("read(%s) failed: %m", socket_path);
            } else {
                res[ret] = '\0';
                if (strncmp(res, "OK ", 3) == 0) {
                    char *tmp;
                    /* Remove whitespace the end. We expect \r\n. TODO: Looks shady. Is there a dovecot library function for this? */
                    str_append(aps_topic, strtok_r(&res[3], "\r\n", &tmp));
                    ret = 0;
                }
            }
        }
    }
    o_stream_destroy(&ostream);
    alarm(0);

    net_disconnect(fd);

    return ret;
}


/**
 * Command handler for the XAPPLEPUSHSERVICE command. The command is
 * used by iOS clients to register for push notifications.
 *
 * We receive a list of key value pairs from the client, with the
 * following keys:
 *
 *  aps-version      - always set to "2"
 *  aps-account-id   - a unique id the iOS device has associated with this account
 *  api-device-token - the APS device token
 *  api-subtopic     - always set to "com.apple.mobilemail"
 *  mailboxes        - list of mailboxes to send notifications for
 *
 * For example:
 *
 *  XAPPLEPUSHSERVICE aps-version 2 aps-account-id 0715A26B-CA09-4730-A419-793000CA982E
 *    aps-device-token 2918390218931890821908309283098109381029309829018310983092892829
 *    aps-subtopic com.apple.mobilemail mailboxes (INBOX Notes)
 *
 * To minimize the work that needs to be done inside the IMAP client,
 * we only parse and validate the parameters and then simply push all
 * of this to the supporting daemon that will record the mapping
 * between the account and the iOS client.
 */

static bool cmd_xapplepushservice(struct client_command_context *cmd) {
    /*
     * Parse arguments. We expect four key value pairs. We only take
     * those that we understand for version 2 of this extension.
     *
     * TODO: We are ignoring the mailboxes parameter for now and just
     * default to INBOX always.
     */

    const struct imap_arg *args;
    const char *arg_key, *arg_val;
    const char *aps_version = NULL, *aps_account_id = NULL, *aps_device_token = NULL, *aps_subtopic = NULL;

    if (!client_read_args(cmd, 0, 0, &args)) {
        client_send_command_error(cmd, "Invalid arguments.");
        return FALSE;
    }

    for (int i = 0; i < 4; i++) {
        if (!imap_arg_get_astring(&args[i * 2 + 0], &arg_key)) {
            client_send_command_error(cmd, "Invalid arguments.");
            return FALSE;
        }

        if (!imap_arg_get_astring(&args[i * 2 + 1], &arg_val)) {
            client_send_command_error(cmd, "Invalid arguments.");
            return FALSE;
        }

        if (strcasecmp(arg_key, "aps-version") == 0) {
            aps_version = arg_val;
        } else if (strcasecmp(arg_key, "aps-account-id") == 0) {
            aps_account_id = arg_val;
        } else if (strcasecmp(arg_key, "aps-device-token") == 0) {
            aps_device_token = arg_val;
        } else if (strcasecmp(arg_key, "aps-subtopic") == 0) {
            aps_subtopic = arg_val;
        }
    }

    /*
     * Check if this is a version we expect
     */

    if (!aps_version || (strcmp(aps_version, "1") != 0 && strcmp(aps_version, "2") != 0)) {
        client_send_command_error(cmd, "Unknown aps-version.");
        return FALSE;
    }

    /*
     * If this is version 2 then also need to grab the mailboxes, which
     * is a list of mailbox names.
     */

    const struct imap_arg *mailboxes = NULL;

    if (strcmp(aps_version, "2") == 0) {
        if (!imap_arg_get_astring(&args[8], &arg_key)) {
            client_send_command_error(cmd, "Invalid arguments.");
            return FALSE;
        }

        if (strcmp(arg_key, "mailboxes") != 0) {
            client_send_command_error(cmd, "Invalid arguments. (Expected mailboxes)");
            return FALSE;
        }

        if (!imap_arg_get_list(&args[9], &mailboxes)) {
            client_send_command_error(cmd, "Invalid arguments.");
            return FALSE;
        }
    }

    /*
     * Check if all of the parameters are there.
     */

    if (!aps_account_id || strlen(aps_account_id) == 0) {
        client_send_command_error(cmd, "Incomplete or empty aps-account-id parameter.");
        return FALSE;
    }

    if (!aps_device_token || strlen(aps_device_token) == 0) {
        client_send_command_error(cmd, "Incomplete or empty aps-device-token parameter.");
        return FALSE;
    }

    if (!aps_subtopic || strlen(aps_subtopic) == 0) {
        client_send_command_error(cmd, "Incomplete or empty aps-subtopic parameter.");
        return FALSE;
    }

    /*
     * Forward to the helper daemon. The helper will return the
     * aps-topic, which in reality is the subject of the certificate. I
     * think it is only used to make sure that the binding between the
     * client and the APS server and the IMAP server stays current.
     */

    struct client *client = cmd->client;
    struct mail_user *user = client->user;

    const char *socket_path = mail_user_plugin_getenv(user, "xaps_socket");
    if (socket_path == NULL) {
        socket_path = "/var/run/dovecot/xapsd.sock";
    }

    string_t *aps_topic = t_str_new(0);

    if (xaps_register(socket_path, aps_account_id, aps_device_token, aps_subtopic, user->username, mailboxes,
                      aps_topic) != 0) {
        client_send_command_error(cmd, "Registration failed.");
        return FALSE;
    }

    /*
     * Return success. We assume that aps_version and aps_topic do not
     * contain anything that needs to be escaped.
     */

    client_send_line(cmd->client,
                     t_strdup_printf("* XAPPLEPUSHSERVICE aps-version \"%s\" aps-topic \"%s\"", aps_version,
                                     str_c(aps_topic)));
    client_send_tagline(cmd, "OK XAPPLEPUSHSERVICE Registration successful.");

    return TRUE;
}


/**
 * This hook is called when a client has connected but before the
 * capability string has been sent. We simply add XAPPLEPUSHSERVICE to
 * the capabilities. This will trigger the usage of the
 * XAPPLEPUSHSERVICE command by iOS clients.
 */

static void xaps_client_created(struct client **client) {
    if (mail_user_is_plugin_loaded((*client)->user, imap_xaps_module)) {
        str_append((*client)->capability_string, " XAPPLEPUSHSERVICE");
    }

    if (next_hook_client_created != NULL) {
        next_hook_client_created(client);
    }
}


/**
 * This plugin method is called when the plugin is globally
 * initialized. We register a new command, XAPPLEPUSHSERVICE, and also
 * setup the client_created hook so that we can modify the
 * capabilities string.
 */

void imap_xaps_plugin_init(struct module *module) {
    command_register("XAPPLEPUSHSERVICE", cmd_xapplepushservice, 0);

    imap_xaps_module = module;
    next_hook_client_created = imap_client_created_hook_set(xaps_client_created);
}


/**
 * This plugin method is called when the plugin is globally
 * deinitialized. We unregister our command and remove the
 * client_created hook.
 */

void imap_xaps_plugin_deinit(void) {
    imap_client_created_hook_set(next_hook_client_created);

    command_unregister("XAPPLEPUSHSERVICE");
}


/**
 * This plugin only makes sense in the context of IMAP.
 */

const char imap_xaps_plugin_binary_dependency[] = "imap";
