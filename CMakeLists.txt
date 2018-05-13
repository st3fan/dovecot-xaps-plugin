cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(dovecot-xaps-plugin)

include_directories(/usr/include/dovecot)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(push_notification_xaps_plugin SHARED push-notification-xaps-plugin.c xaps-daemon.c)
add_library(imap_xaps_plugin SHARED imap-xaps-plugin.c xaps-daemon.c)

set_target_properties(push_notification_xaps_plugin PROPERTIES PREFIX "")
set_target_properties(imap_xaps_plugin PROPERTIES PREFIX "")

install(TARGETS push_notification_xaps_plugin DESTINATION /usr/lib/dovecot/modules)
install(TARGETS imap_xaps_plugin DESTINATION /usr/lib/dovecot/modules)
