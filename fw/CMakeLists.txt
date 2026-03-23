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
target_include_directories(niufw PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/xsm3
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
)

pico_enable_stdio_usb(niufw 0)
pico_enable_stdio_uart(niufw 1)
pico_add_extra_outputs(niufw)
