# STM32G4
add_library(stm32g4 INTERFACE)

target_include_directories(stm32g4 INTERFACE
    STM32G4xx/Include
)

target_link_libraries(stm32g4 INTERFACE
    cmsis
)

target_sources(stm32g4 INTERFACE
    STM32G4xx/Source/system_stm32g4xx.c
)

target_compile_options(stm32g4 INTERFACE
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

target_link_options(stm32g4 INTERFACE
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

# STM32F0
add_library(stm32f0 INTERFACE)

target_include_directories(stm32f0 INTERFACE
    STM32F0xx/Include
)

target_link_libraries(stm32f0 INTERFACE
    cmsis
)

target_sources(stm32f0 INTERFACE
    STM32F0xx/Source/system_stm32f0xx.c
)

target_compile_options(stm32f0 INTERFACE
    -mcpu=cortex-m0
)

target_link_options(stm32f0 INTERFACE
    -mcpu=cortex-m0
)