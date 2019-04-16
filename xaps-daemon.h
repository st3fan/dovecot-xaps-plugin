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

#include <lib.h>
#include <str.h>
#include <push-notification-events.h>

#ifndef DOVECOT_XAPS_PLUGIN_XAPS_H
#define DOVECOT_XAPS_PLUGIN_XAPS_H

#define XAPS_LOG_LABEL "XAPS Push Notification: "
#define DEFAULT_SOCKPATH "/var/run/dovecot/xapsd.sock"

struct xaps_attr {
    const char *aps_version, *aps_account_id, *aps_device_token, *aps_subtopic;
    const struct imap_arg *mailboxes;
    const char *dovecot_username;
    string_t *aps_topic;
};

int send_to_daemon(const char *socket_path, const string_t *payload, struct xaps_attr *xaps_attr);

int xaps_notify(const char *socket_path, const char *username, struct mail_user *mailuser, struct mailbox *mailbox, struct push_notification_txn_msg *msg);

int xaps_register(const char *socket_path, struct xaps_attr *xaps_attr);

#endif