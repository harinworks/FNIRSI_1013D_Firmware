set(SRC_FILES
    ccu_control.c
    ch340_class.c
    commands.c
    diskio.c
    display_control.c
    display_lib.c
    DS3231.c
    ff.c
    ffunicode.c
    fnirsi_1013d_scope.c
    font_0.c
    font_2.c
    font_3.c
    font_4.c
    font_5.c
    fpga_control.c
    icons.c
    interrupt.c
    mass_storage_class.c
    memcmp.s
    memcpy.s
    memmove.s
    memset.s
    menu.c
    power_and_battery.c
    ref_and_math.c
    scope_functions.c
    sd_card_interface.c
    sin_cos_math.c
    spi_control.c
    start.s
    statemachine.c
    test.c
    timer.c
    touchpanel.c
    usb_interface.c
    variables.c
)

add_executable(fnirsi_1013d_scope ${SRC_FILES})
add_dependencies(fnirsi_1013d_scope host_utils)
add_dependencies(fnirsi_1013d_scope fnirsi_1013d_startup)

target_link_libraries(fnirsi_1013d_scope PRIVATE gcc)
target_link_libraries(fnirsi_1013d_scope PRIVATE m)

target_link_options(fnirsi_1013d_scope PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/fnirsi_1013d.ld -nostdlib)

add_custom_command(TARGET fnirsi_1013d_scope POST_BUILD
    # Convert to binary
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:fnirsi_1013d_scope> ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_scope.bin

    # Fix BROM header
    COMMAND ${MKSUNXI} ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_scope.bin

    # Create SD card package
    COMMAND ${FLASHFILEPACKER} -o ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d.bin -i ${CMAKE_CURRENT_BINARY_DIR}/fnirsi_1013d_scope.bin -l 0x8000
)
