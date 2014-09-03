/* Copyright (c) 2014 Stefan Arentz */


#include "lib.h"
#include "network.h"
#include "str.h"
#include "strescape.h"
#include "mail-storage.h"
#include "mail-storage-private.h"
#include "mail-user.h"
#include "notify-plugin.h"
#include "module-context.h"

#include "xaps-plugin.h"

#include <stdlib.h>


#ifdef DOVECOT_ABI_VERSION
const char *xaps_plugin_version = DOVECOT_ABI_VERSION;
#else
const char *xaps_plugin_version = DOVECOT_VERSION;
#endif


static MODULE_CONTEXT_DEFINE_INIT(xaps_storage_module, &mail_storage_module_register);

#define STORAGE_CONTEXT(obj) MODULE_CONTEXT(obj, xaps_storage_module)

/**
 * Quote and escape a string. Not sure if this deals correctly with
 * unicode in mailbox names.
 */

static void xaps_str_append_quoted(string_t *dest, const char *str)
{
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

static int xaps_notify(const char *socket_path, const char *username, const char *mailbox)
{
  int ret = -1;

  /*
   * Construct the request.
   */

  string_t *req = t_str_new(1024);
  str_append(req, "NOTIFY");
  str_append(req, " dovecot-username=");
  xaps_str_append_quoted(req, username);
  str_append(req, " dovecot-mailbox=");
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
  {
    if (net_transmit(fd, str_data(req), str_len(req)) < 0) {
      i_error("write(%s) failed: %m", socket_path);
      ret = -1;
    } else {
      char res[1024];
      ret = net_receive(fd, res, sizeof(res));
      if (ret < 0) {
        i_error("read(%s) failed: %m", socket_path);
      } else {
        if (strncmp(res, "OK ", 3) == 0) {
          ret = 0;
        }
      }
    }
  }
  alarm(0);

  net_disconnect(fd);

  return ret;
}


struct xaps_mailbox {
  union mailbox_module_context module_ctx;
  int messages_written_to_inbox;
};


static int xaps_save_finish(struct mail_save_context *ctx)
{
  struct mailbox_transaction_context *t = ctx->transaction;
  struct xaps_mailbox *asmb = STORAGE_CONTEXT(t->box);
  struct mail *mail = ctx->dest_mail;

  /*
   * TODO: Right now for testing we send the notification here. But I
   * think we should really record all the mailboxes touched in this
   * transaction and then send a single notification with an array of
   * mailboxes with new emails.
   */

  struct mail_user *user = mail->box->storage->user;

  const char *socket_path = mail_user_plugin_getenv(user, "xaps_socket");
  if (socket_path == NULL) {
    socket_path = "/var/run/dovecot/xapsd.sock";
  }

  int ret = asmb->module_ctx.super.save_finish(ctx);
  if (ret == 0) {
    if (xaps_notify(socket_path, user->username, mailbox_get_name(mail->box)) != 0) {
      i_error("cannot notify");
    }
  }

  return ret;
}

static void xaps_mailbox_allocated(struct mailbox *box)
{
  if (box->storage->user == NULL) {
    return;
  }

  struct xaps_mailbox *asmb = p_new(box->pool, struct xaps_mailbox, 1);
  asmb->module_ctx.super = box->v;
  box->v.save_finish = xaps_save_finish;

  MODULE_CONTEXT_SET(box, xaps_storage_module, asmb);
}


static struct mail_storage_hooks xaps_mail_storage_hooks = {
  .mailbox_allocated = xaps_mailbox_allocated
};


void xaps_plugin_init(struct module *module)
{
  mail_storage_hooks_add(module, &xaps_mail_storage_hooks);
}

void xaps_plugin_deinit(void)
{
  mail_storage_hooks_remove(&xaps_mail_storage_hooks);
}
