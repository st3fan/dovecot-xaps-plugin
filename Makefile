# Makefile

PLUGIN_NAME = xaps_plugin.so
IMAP_PLUGIN_NAME = imap_xaps_plugin.so

DOVECOT_MODULES = /usr/lib/dovecot/modules
DOVECOT_INCLUDES = /usr/include/dovecot

#

PLUGIN_SOURCES := xaps-plugin.c
IMAP_PLUGIN_SOURCES := imap-xaps-plugin.c

.PHONY: all build install clean

all: build

build: ${PLUGIN_NAME} ${IMAP_PLUGIN_NAME}

${PLUGIN_NAME}: ${PLUGIN_SOURCES}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -std=gnu99 -fPIC -shared -Wall -I${DOVECOT_INCLUDES} -DHAVE_CONFIG_H $< -o $@

${IMAP_PLUGIN_NAME}: ${IMAP_PLUGIN_SOURCES}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -std=gnu99 -fPIC -shared -Wall -I${DOVECOT_INCLUDES} -DHAVE_CONFIG_H $< -o $@

install: build
	install ${PLUGIN_NAME} ${DOVECOT_MODULES}
	install ${IMAP_PLUGIN_NAME} ${DOVECOT_MODULES}

clean:
	$(RM) ${PLUGIN_NAME} ${IMAP_PLUGIN_NAME}
