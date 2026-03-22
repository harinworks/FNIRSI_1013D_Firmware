cmake_minimum_required(VERSION 3.15.0)
project(fnirsi_1013d C ASM)

include(ExternalProject)
include(cmake/GenerateVersion.cmake)
include(cmake/arm-none-eabi.cmake)

ExternalProject_Add(host_utils
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/host_utils
    BINARY_DIR ${CMAKE_BINARY_DIR}/host_utils
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS 
        ${CMAKE_BINARY_DIR}/host_utils/flashfilepacker
        ${CMAKE_BINARY_DIR}/host_utils/mksunxi
)

set(FLASHFILEPACKER ${CMAKE_BINARY_DIR}/host_utils/flashfilepacker)
set(MKSUNXI ${CMAKE_BINARY_DIR}/host_utils/mksunxi)

configure_file(cmake/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
)

if(USE_TP_CONFIG)
    add_compile_definitions(USE_TP_CONFIG)
endif()

if(USE_LR_CONFIG)
    add_compile_definitions(USE_LR_CONFIG)
endif()

if(SAVE_TP_CONFIG)
    add_compile_definitions(SAVE_TP_CONFIG)
endif()

add_subdirectory(fnirsi_1013d_startup)
add_subdirectory(fnirsi_1013d_scope)
