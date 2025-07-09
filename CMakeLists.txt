cmake_minimum_required(VERSION 3.15.0)
project(fnirsi_1013d C ASM)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

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

if(LEGACY_STARTUP)
    add_subdirectory(fnirsi_1013d_sd_card_bootloader)
    add_subdirectory(fnirsi_1013d_startup_screen)
else()
    add_subdirectory(fnirsi_1014d_startup)
endif()

add_subdirectory(fnirsi_1013d_scope)
