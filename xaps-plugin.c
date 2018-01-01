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

#if (DOVECOT_VERSION_MAJOR >= 2u) && (DOVECOT_VERSION_MINOR >= 2u)
#include "net.h"
#else
#include "network.h"
#endif

#include "str.h"
#include "strescape.h"
#include "mail-storage.h"
#include "mail-storage-private.h"
#include "notify-plugin.h"

#include "xaps-plugin.h"


#define XAPS_CONTEXT(obj) MODULE_CONTEXT(obj, xaps_storage_module)
#define XAPS_MAIL_CONTEXT(obj) MDULE_CONTEXT(obj, xaps_mail_module)
#define XAPS_USER_CONTEXT(obj) MODULE_CONTEXT(obj, xaps_user_module)


#ifdef DOVECOT_ABI_VERSION
const char *xaps_plugin_version = DOVECOT_ABI_VERSION;
#else
const char *xaps_plugin_version = DOVECOT_VERSION;
#endif


static MODULE_CONTEXT_DEFINE_INIT(xaps_storage_module, &mail_storage_module_register);
//static MODULE_CONTEXT_DEFINE_INIT(xaps_user_module, &mail_user_module_register);
//static MODULE_CONTEXT_DEFINE_INIT(xaps_mail_module, &mail_module_register);


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


struct xaps_mailbox {
    union mailbox_module_context module_ctx;
    int message_count;
};


static struct mailbox_transaction_context *
xaps_transaction_begin(struct mailbox *box, enum mailbox_transaction_flags flags) {
    i_debug("xaps_transaction_begin");

    struct xaps_mailbox *xaps_mailbox = XAPS_CONTEXT(box);
    xaps_mailbox->message_count = 0;

    union mailbox_module_context *zbox = XAPS_CONTEXT(box);
    return zbox->super.transaction_begin(box, flags);
}


static int xaps_save_finish(struct mail_save_context *ctx) {
    i_debug("xaps_save_finish");

    struct mailbox_transaction_context *t = ctx->transaction;
    struct xaps_mailbox *xaps_mailbox = XAPS_CONTEXT(t->box);

    int ret = xaps_mailbox->module_ctx.super.save_finish(ctx);
    if (ret == 0) {
        xaps_mailbox->message_count++;
    }

    return ret;
}


static int
xaps_transaction_commit(struct mailbox_transaction_context *t, struct mail_transaction_commit_changes *changes_r) {
    i_debug("xaps_transaction_commit");

    /*
     * If the message count in this transaction is not zero then we have
     * written new messages to the inbox. Call the notifier.
     */

    struct xaps_mailbox *xaps_mailbox = XAPS_CONTEXT(t->box);
    if (xaps_mailbox->message_count != 0) {
        const char *socket_path = mail_user_plugin_getenv(t->box->storage->user, "xaps_socket");
        if (socket_path == NULL) {
            socket_path = "/var/run/dovecot/xapsd.sock";
        }
        if (xaps_notify(socket_path, t->box->storage->user->username, t->box->name) != 0) {
            i_error("cannot notify");
        }
    }

    /*
     * Call the original transaction_commit()
     */

    union mailbox_module_context *zbox = XAPS_CONTEXT(t->box);
    return zbox->super.transaction_commit(t, changes_r);
}

static void xaps_mailbox_allocated(struct mailbox *box) {
    if (box->storage->user == NULL) {
        return;
    }

    struct xaps_mailbox *asmb = p_new(box->pool, struct xaps_mailbox, 1);
    asmb->module_ctx.super = box->v;
    box->v.transaction_begin = xaps_transaction_begin;
    box->v.save_finish = xaps_save_finish;
    box->v.transaction_commit = xaps_transaction_commit;

    MODULE_CONTEXT_SET(box, xaps_storage_module, asmb);
}


static struct mail_storage_hooks xaps_mail_storage_hooks = {
        .mailbox_allocated = xaps_mailbox_allocated
};


void xaps_plugin_init(struct module *module) {
    i_debug("xaps_plugin_init");
    mail_storage_hooks_add(module, &xaps_mail_storage_hooks);
}

void xaps_plugin_deinit(void) {
    i_debug("xaps_plugin_deinit");
    mail_storage_hooks_remove(&xaps_mail_storage_hooks);
}
