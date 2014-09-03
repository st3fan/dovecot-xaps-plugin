#
# The MIT License (MIT)
#
# Copyright (c) 2014 Stefan Arentz <stefan@arentz.ca>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

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
