cmake_minimum_required(VERSION 3.15.0)
project(fnirsi_1013d C ASM)

include(ExternalProject)

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

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_RANLIB arm-none-eabi-ranlib)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu='arm926ej-s' -mfloat-abi=soft -O3 -Wall -Wno-char-subscripts -Wno-write-strings -fno-stack-protector")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu='arm926ej-s' -mfloat-abi=soft -mthumb -O0 -Wall -x assembler-with-cpp -fmessage-length=0")

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
