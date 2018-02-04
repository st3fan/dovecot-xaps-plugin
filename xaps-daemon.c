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
#include <net.h>
#if (DOVECOT_VERSION_MINOR >= 3u)
#include <ostream-unix.h>
#include <ostream.h>
#endif
#include <unistd.h>
#include <push-notification-drivers.h>
#include <imap-arg.h>
#include <strescape.h>
#include <mail-storage-private.h>

#include "xaps-daemon.h"


/*
 * Send the request to our daemon over a unix domain socket. The
 * protocol is very simple line based. We use an alarm to make sure
 * this request does not hang.
 */
int send_to_deamon(const char *socket_path, const string_t *payload, struct xaps_attr *xaps_attr) {
    int ret = -1;

    int fd = net_connect_unix(socket_path);
    if (fd == -1) {
        i_error("net_connect_unix(%s) failed: %m", socket_path);
        return -1;
    }

    net_set_nonblock(fd, FALSE);
    alarm(1);                     /* TODO: Should be a constant. What is a good duration? */
#ifdef OSTREAM_UNIX_H
    struct ostream *ostream = o_stream_create_unix(fd, (size_t)-1);
    o_stream_cork(ostream);
    o_stream_nsend(ostream, str_data(payload), str_len(payload));
    o_stream_uncork(ostream);
    {
        if (o_stream_flush(ostream) < 1) {
#else
    {
        if (net_transmit(fd, str_data(payload), str_len(payload)) < 0) {
#endif
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
                    if (xaps_attr) {
                        char *tmp;
                        /* Remove whitespace the end. We expect \r\n. TODO: Looks shady. Is there a dovecot library function for this? */
                        str_append(xaps_attr->aps_topic, strtok_r(&res[3], "\r\n", &tmp));
                    }
                    ret = 0;
                }
            }
        }
    }
#ifdef OSTREAM_UNIX_H
    o_stream_destroy(&ostream);
#endif
    alarm(0);

    net_disconnect(fd);
    return ret;
}

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
 * Notify the backend daemon of an incoming mail. Right now we tell
 * the daemon the username and the mailbox in which a new email was
 * posted. The daemon can then lookup the user and see if any of the
 * devices want to receive a notification for that mailbox.
 */

int xaps_notify(const char *socket_path, struct mail_user *mailuser, struct mailbox *mailbox) {
    /*
     * Construct the request.
     */
    string_t *req = t_str_new(1024);
    str_append(req, "NOTIFY");
    str_append(req, " dovecot-username=");
    xaps_str_append_quoted(req, mailuser->username);
    str_append(req, "\tdovecot-mailbox=");
    xaps_str_append_quoted(req, mailbox->name);
    str_append(req, "\r\n");


    push_notification_driver_debug(XAPS_LOG_LABEL, mailuser, "about to send: %p", req);
    return send_to_deamon(socket_path, req, NULL);
}

/**
 * Send a registration request to the daemon, which will do all the
 * hard work.
 */
int xaps_register(const char *socket_path, struct xaps_attr *xaps_attr) {
    /*
     * Construct our request.
     */

    string_t *req = t_str_new(1024);
    str_append(req, "REGISTER");
    str_append(req, " aps-account-id=");
    xaps_str_append_quoted(req, xaps_attr->aps_account_id);
    str_append(req, "\taps-device-token=");
    xaps_str_append_quoted(req, xaps_attr->aps_device_token);
    str_append(req, "\taps-subtopic=");
    xaps_str_append_quoted(req, xaps_attr->aps_subtopic);
    str_append(req, "\tdovecot-username=");
    xaps_str_append_quoted(req, xaps_attr->dovecot_username);
    str_append(req, "");

    if (xaps_attr->mailboxes == NULL) {
        str_append(req, "\tdovecot-mailboxes=(\"INBOX\")");
    } else {
        str_append(req, "\tdovecot-mailboxes=(");
        int next = 0;
        for (; !IMAP_ARG_IS_EOL(xaps_attr->mailboxes); xaps_attr->mailboxes++) {
            const char *mailbox;
            if (!imap_arg_get_astring(&(xaps_attr->mailboxes[0]), &mailbox)) {
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

    return send_to_deamon(socket_path, req, xaps_attr);
}
