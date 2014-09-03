/* Copyright (c) 2014 Stefan Arentz */


#include "lib.h"
#include "network.h"
#include "str.h"
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

static int xaps_notify(const char *username, const char *mailbox)
{
  int ret = -1;

  /*
   * Construct the request. Not sure yet if we also need to send the
   * mailbox name. Maybe the ping is just a global 'hey go check mail'
   */

  string_t *req = t_str_new(1024);
  str_append(req, "NOTIFY");
  str_append(req, " dovecot-username=\"");
  str_append(req, username);
  str_append(req, "\" dovecot-mailbox=\"");
  str_append(req, mailbox);
  str_append(req, "\"\r\n");

  /*
   * Send the request to our daemon over a unix domain socket. The
   * protocol is very simple line based. We use an alarm to make sure
   * this request does not hang.
   */

  const char *socket_path = "/tmp/xapsd.sock"; /* TODO: This needs to move to the configuration */

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
        i_debug("we got a response: %s", res);
        /* if (strncmp(res, "OK ", 3) == 0 && strlen(res) > 6) { */
        /*   /\* Remove whitespace the end. We expect \r\n. TODO: Looks shady. Is there a dovecot library function for this? *\/ */
        /*   if (res[strlen(res)-2] == '\r') { */
        /*     res[strlen(res)-2] = 0x00; */
        /*   } */
        /*   str_append(aps_topic, &res[3]); */
        /*   ret = 0; */
        /* } */
      }
    }
  }
  alarm(0);

  net_disconnect(fd);

  return ret;
}

/* static void xaps_mail_save(void *txn, struct mail *mail) */
/* { */
/*   i_error("mail_save callback"); */
/*   if (xaps_notify() != 0) { */
/*     i_error("cannot notify"); */
/*   } */
/* } */


/* static const struct notify_vfuncs xaps_notify_vfuncs = { */
/*   .mail_save = xaps_mail_save, */
/* }; */

/* static struct notify_context *xaps_notify_ctx; */


/* void foo() { */
/*   exit(123); */
/*   i_debug("mail_allocated"); */
/*   if (xaps_notify() != 0) { */
/*     i_error("cannot notify"); */
/*   } */
/* } */

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

  int ret = asmb->module_ctx.super.save_finish(ctx);
  if (ret == 0) {
    if (xaps_notify(mail->box->storage->user->username, mailbox_get_name(mail->box)) != 0) {
      i_error("cannot notify");
    }
  }

  return ret;
}

static void xaps_mailbox_allocated(struct mailbox *box)
{
  struct xaps_mailbox *asmb;

#if 0
  if (USER_CONTEXT(box->storage->user) == NULL) {
    return;
  }
#endif

  asmb = p_new(box->pool, struct xaps_mailbox, 1);
  asmb->module_ctx.super = box->v;

  //asmb->box_class = xaps_mailbox_classify(box);

  //box->v.copy = antispam_copy;
  //box->v.save_begin = antispam_save_begin;
  box->v.save_finish = xaps_save_finish;
  //box->v.transaction_begin = antispam_transaction_begin;
  //box->v.transaction_commit = antispam_transaction_commit;
  //box->v.transaction_rollback = antispam_transaction_rollback;

  MODULE_CONTEXT_SET(box, xaps_storage_module, asmb);
}


static struct mail_storage_hooks xaps_mail_storage_hooks = {
  .mailbox_allocated = xaps_mailbox_allocated
};






void xaps_plugin_init(struct module *module)
{
  i_info("xaps_plugin_init");
  //xaps_notify_ctx = notify_register(&xaps_notify_vfuncs);

  mail_storage_hooks_add(module, &xaps_mail_storage_hooks);

}

void xaps_plugin_deinit(void)
{
  i_info("xaps_plugin_deinit");
  //notify_unregister(xaps_notify_ctx);

  mail_storage_hooks_remove(&xaps_mail_storage_hooks);
}


const char *xapplepushservice_plugin_dependencies[] = { "notify", NULL };
