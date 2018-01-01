cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(dovecot-xaps-plugin)

include_directories(/usr/include/dovecot)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(xaps_plugin SHARED xaps-plugin.c)
add_library(imap_xaps_plugin SHARED imap-xaps-plugin.c)

set_target_properties(xaps_plugin PROPERTIES PREFIX "")
set_target_properties(imap_xaps_plugin PROPERTIES PREFIX "")

install(TARGETS xaps_plugin DESTINATION /usr/lib/dovecot/modules)
install(TARGETS imap_xaps_plugin DESTINATION /usr/lib/dovecot/modules)
