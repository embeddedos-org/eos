# STM32F407 Discovery Board — CMake Fragment
#
# Included automatically when -DEOS_BOARD=stm32f407_discovery

set(EOS_BOARD_NAME "stm32f407_discovery")
set(EOS_BOARD_MCU "STM32F407VG")
set(EOS_BOARD_ARCH "cortex-m4")

# CMSIS defines
add_compile_definitions(
    STM32F407xx
    HSE_VALUE=8000000
    EOS_BOARD_STM32F407_DISCOVERY
)

# Linker script
set(BOARD_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/stm32f407_flash.ld"
)

if(CMAKE_CROSSCOMPILING)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -T${BOARD_LINKER_SCRIPT}"
    )
endif()

# Startup assembly
set(BOARD_STARTUP_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/startup_stm32f407.s"
)

# Board-specific HAL sources
set(BOARD_HAL_SOURCES
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_rcc.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_gpio.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_uart.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_spi.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_i2c.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_timer.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_adc.c
    ${CMAKE_SOURCE_DIR}/hal/src/hal_stm32f4/hal_stm32f4_backend.c
)

message(STATUS "  Board: STM32F407 Discovery (Cortex-M4, 168MHz, 1MB Flash, 192KB RAM)")
