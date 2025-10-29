set(SRC_FILES
    bl_display_lib.c
    bl_fonts.c
    bl_fpga_control.c
    bl_sd_card_interface.c
    bl_uart.c
    bl_variables.c
    ccu_control.c
    display_control.c
    dram_control.c
    fnirsi_1014d_startup.c
    memcmp.s
    memcpy.s
    memset.s
    spi_control.c
    start.s
    touchpanel.c
)

add_executable(fnirsi_1014d_startup ${SRC_FILES})
add_dependencies(fnirsi_1014d_startup host_utils)

target_link_libraries(fnirsi_1014d_startup PRIVATE gcc)

target_compile_options(fnirsi_1014d_startup PRIVATE -O0)
target_link_options(fnirsi_1014d_startup PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/fnirsi_sd_card_bootloader.ld -nostdlib)

add_custom_command(TARGET fnirsi_1014d_startup POST_BUILD
    # Convert to binary (only .text section)
    COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=.text $<TARGET_FILE:fnirsi_1014d_startup> ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1014d_startup.bin

    # Fix BROM header
    COMMAND ${MKSUNXI} ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1014d_startup.bin

    # Copy to scope directory
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/fnirsi_1013d_scope
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1014d_startup.bin ${CMAKE_BINARY_DIR}/fnirsi_1013d_scope/fnirsi_1013d.bin
)
