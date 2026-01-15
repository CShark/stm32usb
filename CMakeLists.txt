cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

project(stm32usb C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LWIP_DIR lwip)
set(LWIP_INCLUDE_DIRS lwip/src/include eth/Inc)
include(lwip/src/Filelists.cmake)

add_subdirectory(Drivers)

add_library(core INTERFACE)

target_include_directories(core INTERFACE
    Inc
    eth/Inc
)

target_sources(core INTERFACE
    Src/main.c
    Src/platform.c
    Src/usb.c
    Startup/syscalls.c
)

# Features
add_library(usb_cdc INTERFACE)
target_sources(usb_cdc INTERFACE
    Src/cdc/cdc_config.c
    Src/cdc/cdc_device.c
)

add_library(usb_hid INTERFACE)
target_sources(usb_hid INTERFACE
    Src/hid/hid_config.c
    Src/hid/hid_device.c
)

add_library(usb_ncm INTERFACE)
target_sources(usb_ncm INTERFACE
    Src/ncm/ncm_config.c
    Src/ncm/ncm_device.c
    Src/ncm/ncm_netif.c
)
target_include_directories(usb_ncm INTERFACE ${LWIP_INCLUDE_DIRS})
target_link_libraries(usb_ncm INTERFACE lwipallapps)

# STM32G441
add_executable(stm32g441)

target_link_libraries(stm32g441 PRIVATE
    stm32g4
    core
    usb_cdc
    usb_hid
    usb_ncm
)

target_sources(stm32g441 PRIVATE
    Startup/startup_stm32g441rbtx.s
)

target_compile_definitions(stm32g441 PRIVATE
    STM32G441xx
)

target_link_options(stm32g441 PRIVATE
    -T "${CMAKE_CURRENT_SOURCE_DIR}/Linker/STM32G441RBTX_FLASH.ld"
)

# STM32G474
add_executable(stm32g474)

target_link_libraries(stm32g474 PRIVATE
    stm32g4
    core
    usb_cdc
    usb_hid
    usb_ncm
)

target_sources(stm32g474 PRIVATE
    Startup/startup_stm32g474xx.s
)

target_compile_definitions(stm32g474 PRIVATE
    STM32G474xx
)

target_link_options(stm32g474 PRIVATE
    -T "${CMAKE_CURRENT_SOURCE_DIR}/Linker/STM32G474XX_FLASH.ld"
)

# STM32F042
add_executable(stm32f042)

target_link_libraries(stm32f042 PRIVATE
    stm32f0
    core
    usb_cdc
    usb_hid
)

target_sources(stm32f042 PRIVATE
    Startup/startup_stm32f042k6ux.s
)

target_compile_definitions(stm32f042 PRIVATE
    STM32F042x6
)

target_link_options(stm32f042 PRIVATE
    -T "${CMAKE_CURRENT_SOURCE_DIR}/Linker/STM32F042K6UX_FLASH.ld"
)