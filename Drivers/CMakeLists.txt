add_library(cmsis INTERFACE)

target_include_directories(cmsis INTERFACE
    CMSIS/Include
)

add_subdirectory(CMSIS/Device/ST)