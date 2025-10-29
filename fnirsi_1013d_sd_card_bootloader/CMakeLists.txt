set(SRC_FILES
    bl_sd_card_interface.c
    ccu_control.c
    dram_control.c
    fnirsi_1013d_sd_card_bootloader.c
    memcmp.s
    memcpy.s
    memset.s
    start.s
)

add_executable(fnirsi_1013d_sd_card_bootloader ${SRC_FILES})
add_dependencies(fnirsi_1014d_startup host_utils)

target_link_libraries(fnirsi_1013d_sd_card_bootloader PRIVATE gcc)

target_compile_options(fnirsi_1013d_sd_card_bootloader PRIVATE -O0)
target_link_options(fnirsi_1013d_sd_card_bootloader PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/fnirsi_1013d.ld -nostdlib)

add_custom_command(TARGET fnirsi_1013d_sd_card_bootloader POST_BUILD
    # Convert to binary
    COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=.text $<TARGET_FILE:fnirsi_1013d_sd_card_bootloader> ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_sd_card_bootloader.bin

    # Fix BROM header
    COMMAND ${MKSUNXI} ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_sd_card_bootloader.bin

    # Copy to startup_screen directory
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/fnirsi_1013d_startup_screen
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_sd_card_bootloader.bin ${CMAKE_BINARY_DIR}/fnirsi_1013d_startup_screen/fnirsi_1013d.bin
)
