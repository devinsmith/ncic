cmake_minimum_required(VERSION 3.0)

project(ncic)
set(TARGET_NAME ncic)

find_package(OpenSSL REQUIRED)
find_package(Curses REQUIRED)

# Disable rdynamic
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set(SOURCES ncic.cpp ncic_acct.cpp ncic_alias.cpp ncic_bind.cpp
       ncic_chat.cpp ncic_color.cpp ncic_command.cpp ncic_conf.cpp
       ncic_cstr.cpp ncic_format.cpp ncic_help.cpp ncic_imsg.cpp
       ncic_imwindow.cpp ncic_inet.cpp ncic_input.cpp ncic_io.cpp
       ncic_list.cpp ncic_misc.cpp ncic_msg.cpp ncic_opt.cpp ncic_proto.cpp
       ncic_queue.cpp ncic_screen.cpp ncic_screen_io.cpp ncic_set.cpp
       ncic_status.cpp ncic_swindow.cpp
       ncic_util.cpp ncic_irc.cpp ncic_irc_input.cpp ncic_irc_output.cpp
       ncic_naken.cpp
)

set(HEADERS ncic_acct.h ncic_cstr.h ncic_irc.h     ncic_screen_io.h
ncic_alias.h         ncic_format.h    ncic_list.h    ncic_set_defaults.h
ncic_bind.h          ncic.h           ncic_misc.h    ncic_set.h
ncic_help.h      ncic_msg.h
ncic_chat.h          ncic_imsg.h      ncic_naken.h   ncic_status.h
ncic_color.h         ncic_imwindow.h  ncic_opt.h     ncic_swindow.h
ncic_command_defs.h  ncic_inet.h      ncic_proto.h
ncic_command.h       ncic_input.h     ncic_queue.h   ncic_util.h
ncic_conf.h          ncic_io.h        ncic_screen.h
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

