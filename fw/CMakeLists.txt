cmake_minimum_required(VERSION 3.13)

# pico sdk
set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/pico-sdk CACHE PATH "")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(niufw C CXX ASM)
pico_sdk_init()

add_executable(niufw
  src/main.c
  src/usb_descriptors.c
)
set_target_properties(niufw PROPERTIES SUFFIX ".elf")
pico_generate_pio_header(niufw ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)
pico_generate_pio_header(niufw ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio)

# set by the Makefile via -DHAND=; not meant to be edited here
set(HAND "HAND_LEFT" CACHE STRING "HAND_LEFT or HAND_RIGHT")
target_compile_definitions(niufw PRIVATE HAND=${HAND})
target_include_directories(niufw PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(niufw
    pico_stdlib
    pico_multicore
    pico_util
    tinyusb_device
    tinyusb_board
    hardware_gpio
    hardware_pio
    hardware_i2c
    hardware_uart
)

pico_enable_stdio_usb(niufw 0)
# off: stdio would claim GP0/GP1, which are the TRRS link pins
pico_enable_stdio_uart(niufw 0)
pico_add_extra_outputs(niufw)
