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
#include <net.h>
#include <push-notification-drivers.h>
#include <push-notification-events.h>
#include <push-notification-txn-msg.h>
#include <push-notification-event-messagenew.h>

#include <str.h>
#include <strescape.h>
#include <mail-storage.h>
#include <mail-storage-private.h>

#include "push-notification-xaps-plugin.h"

#define XAPS_LOG_LABEL "XAPS Push Notification: "

const char *xaps_plugin_version = DOVECOT_ABI_VERSION;

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

static int xaps_notify(const char *socket_path, const char *username, const char *mailbox) {
    int ret = -1;

    /*
     * Construct the request.
     */

    string_t *req = t_str_new(1024);
    str_append(req, "NOTIFY");
    str_append(req, " dovecot-username=");
    xaps_str_append_quoted(req, username);
    str_append(req, "\tdovecot-mailbox=");
    xaps_str_append_quoted(req, mailbox);
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

    alarm(1);                     /* TODO: Should be a constant. What is a good duration? */
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

static bool xaps_plugin_begin_txn(struct push_notification_driver_txn *dtxn) {
    const struct push_notification_event *const *event;
    struct push_notification_event_messagenew_config *config;

    // we have to initialize each event
    // the MessageNew event needs a config to appear in the process_msg function
    // so it's handled separately
    array_foreach(&push_notification_events, event) {
        if (strcmp((*event)->name,"MessageNew") == 0) {
            config = p_new(dtxn->ptxn->pool, struct push_notification_event_messagenew_config, 1);
            config->flags = PUSH_NOTIFICATION_MESSAGE_HDR_DATE |
                            PUSH_NOTIFICATION_MESSAGE_HDR_FROM |
                            PUSH_NOTIFICATION_MESSAGE_HDR_TO |
                            PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT |
                            PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET;
            push_notification_event_init(dtxn, "MessageNew", config);
            push_notification_driver_debug(XAPS_LOG_LABEL, dtxn->ptxn->muser,
                                           "Handling MessageNew event");
        } else {
            push_notification_event_init(dtxn, (*event)->name, NULL);
        }
    }
    return TRUE;
}

static void xaps_plugin_process_msg(struct push_notification_driver_txn *dtxn, struct push_notification_txn_msg *msg) {
    struct push_notification_event_messagenew_data *messagenew;
    struct push_notification_txn_event *const *event;

    if (array_is_created(&msg->eventdata)) {
        array_foreach(&msg->eventdata, event) {
            i_debug((*event)->event->event->name);
        }
    }
    
    messagenew = push_notification_txn_msg_get_eventdata(msg, "MessageNew");
    if (messagenew != NULL) {
        i_debug("send");
        const char *socket_path = mail_user_plugin_getenv(dtxn->ptxn->muser, "xaps_socket");
        if (socket_path == NULL) {
            socket_path = "/var/run/dovecot/xapsd.sock";
        }
        if (xaps_notify(socket_path, dtxn->ptxn->muser->username, dtxn->ptxn->mbox->name) != 0) {
            i_error("cannot notify");
        }
    }
}

// push plugin definition

const char *xaps_plugin_dependencies[] = { "push_notification", NULL };

extern struct push_notification_driver push_notification_driver_xaps;

int xaps_plugin_init(struct push_notification_driver_config *module ATTR_UNUSED,
                 struct mail_user *pUser ATTR_UNUSED,
                 pool_t pPool ATTR_UNUSED,
                 void **pVoid ATTR_UNUSED,
                 const char **pString ATTR_UNUSED) {
    return 0;
}

void xaps_plugin_deinit(struct push_notification_driver_user *duser ATTR_UNUSED) {
}

struct push_notification_driver push_notification_driver_xaps = {
        .name = "xaps",
        .v = {
                .init = xaps_plugin_init,
                .begin_txn = xaps_plugin_begin_txn,
                .process_msg = xaps_plugin_process_msg,
                .deinit = xaps_plugin_deinit,
        }
};

// plugin init and deinit

void push_notification_xaps_plugin_init(struct module *module ATTR_UNUSED) {
    push_notification_driver_register(&push_notification_driver_xaps);
}

void push_notification_xaps_plugin_deinit(void) {
    push_notification_driver_unregister(&push_notification_driver_xaps);
}