cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(dovecot-xaps-plugin)

if (APPLE)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_read_args")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_send_command_error")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_send_line")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_client_send_tagline")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_command_register")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_command_unregister")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_imap_client_created_hook_set")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_driver_debug")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_driver_register")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_driver_unregister")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_event_init")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-U,_push_notification_events")
endif ()

include_directories(/usr/include/dovecot)
include_directories(/usr/local/include/dovecot)
find_library(LIBDOVECOT dovecot /usr/lib/dovecot/ /usr/local/lib/dovecot/)
find_library(LIBDOVECOTSTORAGE dovecot-storage /usr/lib/dovecot/ /usr/local/lib/dovecot/)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(lib25_xaps_push_notification_plugin MODULE xaps-daemon.c xaps-push-notification-plugin.c)
add_library(lib25_xaps_imap_plugin MODULE xaps-daemon.c xaps-imap-plugin.c)

target_link_libraries(lib25_xaps_push_notification_plugin ${LIBDOVECOT} ${LIBDOVECOTSTORAGE})
target_link_libraries(lib25_xaps_imap_plugin ${LIBDOVECOT} ${LIBDOVECOTSTORAGE})

set_target_properties(lib25_xaps_push_notification_plugin PROPERTIES PREFIX "")
set_target_properties(lib25_xaps_imap_plugin PROPERTIES PREFIX "")

install(TARGETS lib25_xaps_push_notification_plugin DESTINATION /usr/lib/dovecot/modules)
install(TARGETS lib25_xaps_imap_plugin DESTINATION /usr/lib/dovecot/modules)
