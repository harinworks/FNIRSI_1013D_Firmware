set(SRC_FILES
    bl_fpga_control.c
    bl_sd_card_interface.c
    ccu_control.c
    dram_control.c
    fnirsi_1013d_startup_from_sd_card.c
    memcmp.s
    memcpy.s
    memset.s
    start.s
)

add_executable(fnirsi_1013d_startup_from_sd_card ${SRC_FILES})

target_link_libraries(fnirsi_1013d_startup_from_sd_card PRIVATE gcc)

target_compile_options(fnirsi_1013d_startup_from_sd_card PRIVATE -O0)
target_link_options(fnirsi_1013d_startup_from_sd_card PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/fnirsi_1013d.ld -nostdlib)

add_custom_command(TARGET fnirsi_1013d_startup_from_sd_card POST_BUILD
    # Convert to binary
    COMMAND arm-none-eabi-objcopy -O binary --only-section=.text $<TARGET_FILE:fnirsi_1013d_startup_from_sd_card> ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_startup_from_sd_card.bin

    # Fix BROM header
    COMMAND ${CMAKE_SOURCE_DIR}/bin/mksunxi ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_startup_from_sd_card.bin
)
