set(SOURCES
    startup/${THREADX_TOOLCHAIN}/tx_initialize_low_level.S
    stm32cubel4/stm32l4xx_hal_msp.c
    stm32cubel4/stm32l4xx_it.c
    board_init.c
    console.c
    main.c
    stm_networking.c
    newlib_nano.c

    microros_netx_transports.c
    microros_time.c
)

add_executable(stm32l475_azure_iot
    startup/${THREADX_TOOLCHAIN}/startup_stm32l475xx.s
    ${SOURCES}
)

target_link_libraries(stm32l475_azure_iot
    azrtos::threadx
    azrtos::netxduo

    stm32cubel4
    netx_driver

    libmicroros-prebuilt
)

target_include_directories(stm32l475_azure_iot
    PUBLIC
        .
)

set_target_linker(stm32l475_azure_iot ${CMAKE_CURRENT_LIST_DIR}/startup/gnu/STM32L475xx_FLASH.ld)

post_build(stm32l475_azure_iot)
