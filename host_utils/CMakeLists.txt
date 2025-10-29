cmake_minimum_required(VERSION 3.15.0)
project(host_utils)

add_executable(flashfilepacker flashfilepacker.c)
add_executable(mksunxi mksunxi.c)

include(GNUInstallDirs)

install(TARGETS flashfilepacker mksunxi
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
