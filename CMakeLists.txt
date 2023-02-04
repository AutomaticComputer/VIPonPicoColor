cmake_minimum_required(VERSION 3.20)

include(pico_sdk_import.cmake)

project(pico_vip C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(pico_vip) 
target_sources(pico_vip PUBLIC
    pico_vip.cpp data.h devices.cpp devices.h video_out.pio)

target_include_directories(pico_vip PUBLIC ${CMAKE_CURRENT_LIST_DIR})

pico_generate_pio_header(pico_vip ${CMAKE_CURRENT_LIST_DIR}/video_out.pio)

pico_enable_stdio_usb(pico_vip 1)

target_link_libraries(pico_vip PUBLIC
    pico_stdlib hardware_dma hardware_pwm hardware_clocks hardware_pio pico_multicore)

# pico_set_binary_type(pico_vip no_flash)

pico_add_extra_outputs(pico_vip)

# executable with 78.75 MHz

add_executable(pico_vip_78750) 
target_sources(pico_vip_78750 PUBLIC
    pico_vip.cpp data.h devices.cpp devices.h video_out.pio)

target_compile_definitions(pico_vip_78750 PUBLIC -DCLOCK_78750)
target_include_directories(pico_vip_78750 PUBLIC ${CMAKE_CURRENT_LIST_DIR})

pico_generate_pio_header(pico_vip_78750 ${CMAKE_CURRENT_LIST_DIR}/video_out.pio)

pico_enable_stdio_usb(pico_vip_78750 1)

target_link_libraries(pico_vip_78750 PUBLIC
    pico_stdlib hardware_dma hardware_pwm hardware_clocks hardware_pio pico_multicore)


# pico_set_binary_type(pico_vip no_flash)

pico_add_extra_outputs(pico_vip_78750)
