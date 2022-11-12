cmake_minimum_required(VERSION 3.20)

include(pico_sdk_import.cmake)

project(hello C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(pico_vip 
    pico_vip.cpp data.h devices.cpp devices.h video_out.pio)

pico_generate_pio_header(pico_vip ${CMAKE_CURRENT_LIST_DIR}/video_out.pio)

pico_enable_stdio_usb(pico_vip 1)

target_link_libraries(pico_vip 
    pico_stdlib hardware_dma hardware_pwm hardware_clocks hardware_pio pico_multicore)

# pico_set_binary_type(pico_vip no_flash)

pico_add_extra_outputs(pico_vip)
