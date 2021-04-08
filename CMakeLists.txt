cmake_minimum_required(VERSION 3.8)
project(ncic)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules/)

add_subdirectory(src)

install(TARGETS ncic DESTINATION bin)
install(FILES doc/ncicrc DESTINATION share/ncic)
install(DIRECTORY doc/help DESTINATION share/ncic)
