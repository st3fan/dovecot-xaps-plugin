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
#include <push-notification-drivers.h>
#include <push-notification-events.h>
#include <push-notification-txn-msg.h>
#include <push-notification-event-messagenew.h>
#include <str.h>
#include <mail-storage.h>
#include <mail-storage-private.h>
#include <push-notification-txn-mbox.h>

#include "push-notification-xaps-plugin.h"
#include "xaps.h"

const char *xaps_plugin_version = DOVECOT_ABI_VERSION;

/*
 * Prepare message handling.
 * On return of false, the event gets dismissed for this driver
 */
static bool xaps_plugin_begin_txn(struct push_notification_driver_txn *dtxn) {
    const struct push_notification_event *const *event;
    struct push_notification_event_messagenew_config *config;

    push_notification_driver_debug(XAPS_LOG_LABEL, dtxn->ptxn->muser, "begin_txn: user: %s mailbox: %s",
                                   dtxn->ptxn->muser->username, dtxn->ptxn->mbox->name);

    // we have to initialize each event
    // the MessageNew event needs a config to appear in the process_msg function
    // so it's handled separately
    array_foreach(&push_notification_events, event) {
        if (strcmp((*event)->name,"MessageNew") == 0) {
            config = p_new(dtxn->ptxn->pool, struct push_notification_event_messagenew_config, 1);
            // Take what you can, give nothing back
            config->flags = PUSH_NOTIFICATION_MESSAGE_HDR_DATE |
                            PUSH_NOTIFICATION_MESSAGE_HDR_FROM |
                            PUSH_NOTIFICATION_MESSAGE_HDR_TO |
                            PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT |
                            PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET;
            push_notification_event_init(dtxn, "MessageNew", config);
        } else {
            push_notification_event_init(dtxn, (*event)->name, NULL);
        }
    }
    return TRUE;
}

/*
 * Process the actual message
 */
static void xaps_plugin_process_msg(struct push_notification_driver_txn *dtxn, struct push_notification_txn_msg *msg) {
    struct push_notification_event_messagenew_data *messagenew;
    struct push_notification_txn_event *const *event;

    if (array_is_created(&msg->eventdata)) {
        array_foreach(&msg->eventdata, event) {
            push_notification_driver_debug(XAPS_LOG_LABEL, dtxn->ptxn->muser,
                                           "Handling event: %s", (*event)->event->event->name);
        }
    }

    // for now we only handle new messages and no flags
    messagenew = push_notification_txn_msg_get_eventdata(msg, "MessageNew");
    if (messagenew != NULL) {
        if (xaps_notify(socket_path, dtxn->ptxn->muser, dtxn->ptxn->mbox) != 0) {
            i_error("cannot notify");
        }
    }
}

// push-notification driver definition

const char *xaps_plugin_dependencies[] = { "push_notification", NULL };

extern struct push_notification_driver push_notification_driver_xaps;

int xaps_plugin_init(struct push_notification_driver_config *dconfig ATTR_UNUSED,
                 struct mail_user *muser,
                 pool_t pPool ATTR_UNUSED,
                 void **pVoid ATTR_UNUSED,
                 const char **pString ATTR_UNUSED) {
    socket_path = mail_user_plugin_getenv(muser, "xaps_socket");
    if (socket_path == NULL) {
        socket_path = DEFAULT_SOCKPATH;
    }
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