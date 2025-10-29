set(SRC_FILES
    ccu_control.c
    display_control.c
    fnirsi_1013d_startup_screen.c
    fss_display_lib.c
    fss_fpga_control.c
    fss_icons.c
    fss_variables.c
    interrupt.c
    memcmp.s
    memcpy.s
    memmove.s
    memset.s
    sd_card_interface.c
    sin_cos_math.c
    start.s
    timer.c
)

add_executable(fnirsi_1013d_startup_screen ${SRC_FILES})
add_dependencies(fnirsi_1014d_startup host_utils)
add_dependencies(fnirsi_1013d_startup_screen fnirsi_1013d_sd_card_bootloader)

target_link_libraries(fnirsi_1013d_startup_screen PRIVATE gcc)

target_link_options(fnirsi_1013d_startup_screen PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/fnirsi_1013d.ld -nostdlib)

add_custom_command(TARGET fnirsi_1013d_startup_screen POST_BUILD
    # Convert to binary
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:fnirsi_1013d_startup_screen> ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_startup_screen.bin

    # Fix BROM header
    COMMAND ${MKSUNXI} ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_startup_screen.bin

    # Create SD card package
    COMMAND ${FLASHFILEPACKER} -o ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d.bin -i ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_startup_screen.bin -l 0x4000

    # Copy to scope directory
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/fnirsi_1013d_scope
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d.bin /${CMAKE_BINARY_DIR}/fnirsi_1013d_scope/fnirsi_1013d.bin
)
