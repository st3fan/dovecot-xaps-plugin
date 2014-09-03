#ifndef IMAP_XAPS_PLUGIN_H
#define IMAP_XAPS_PLUGIN_H

struct module;

extern const char imap_xaps_plugin_binary_dependency[];

void imap_xaps_plugin_init(struct module *module);
void imap_xaps_plugin_deinit(void);

#endif
