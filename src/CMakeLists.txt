cmake_minimum_required(VERSION 3.13.4)

project(ncic)
set(TARGET_NAME ncic)

find_package(OpenSSL REQUIRED)
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

set(SOURCES ncic.c ncic_acct.c ncic_alias.c ncic_bind.c
       ncic_chat.c ncic_color.c ncic_command.c ncic_conf.c
       ncic_cstr.c ncic_format.c ncic_help.c ncic_imsg.c
       ncic_imwindow.c ncic_inet.c ncic_input.c ncic_io.c
       ncic_list.c ncic_misc.c ncic_msg.c ncic_opt.c ncic_proto.c
       ncic_queue.c ncic_screen.c ncic_screen_io.c ncic_set.c
       ncic_status.c ncic_swindow.c
       ncic_util.c ncic_irc.c ncic_irc_input.c ncic_irc_output.c
       ncic_naken.c ncic_log.c
)

set(HEADERS ncic_acct.h ncic_cstr.h ncic_irc.h     ncic_screen_io.h
ncic_alias.h         ncic_format.h    ncic_list.h    ncic_set_defaults.h
ncic_bind.h          ncic.h           ncic_misc.h    ncic_set.h
ncic_help.h      ncic_msg.h
ncic_chat.h          ncic_imsg.h      ncic_naken.h   ncic_status.h
ncic_color.h         ncic_imwindow.h  ncic_opt.h     ncic_swindow.h
ncic_inet.h      ncic_proto.h
ncic_command.h       ncic_input.h     ncic_queue.h   ncic_util.h
ncic_conf.h          ncic_io.h        ncic_screen.h ncic_log.h
)



add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})
target_compile_definitions(${TARGET_NAME} PRIVATE SYSTEM_NCICRC=\"${CMAKE_INSTALL_PREFIX}/share/ncic/ncicrc\")
target_link_libraries(${TARGET_NAME} PRIVATE ${OPENSSL_LIBRARIES} ${CURSES_LIBRARIES})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(config.h.in config.h)

install(TARGETS ncic DESTINATION bin)

#if(MSVC)
#  target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
#else()
#  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Werror)
#endif()

