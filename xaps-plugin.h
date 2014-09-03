#ifndef XAPS_PLUGIN_H
#define XAPS_PLUGIN_H

struct module;

extern const char *xapplepushservice_plugin_dependencies[];

void xaps_plugin_init(struct module *module);
void xaps_plugin_deinit(void);

#endif
