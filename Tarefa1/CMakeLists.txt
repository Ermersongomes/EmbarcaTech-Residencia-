cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Inicializa pico_sdk
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Inclui o SDK do Raspberry Pi Pico (deve vir antes de project)
include(pico_sdk_import.cmake)

# Inclui o FreeRTOS
include($ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(Tarefa1 C CXX ASM)

# Inicializa o Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(Tarefa1
    Tarefa1.c
    ssd1306.c
)

pico_set_program_name(Tarefa1 "Tarefa1")
pico_set_program_version(Tarefa1 "0.1")

pico_enable_stdio_uart(Tarefa1 1)
pico_enable_stdio_usb(Tarefa1 1)

# Adiciona bibliotecas para Wi-Fi e MQTT
target_link_libraries(Tarefa1
        pico_stdlib
        FreeRTOS-Kernel FreeRTOS-Kernel-Heap4
        hardware_i2c
        hardware_adc
        pico_cyw43_arch_lwip_sys_freertos
        lwip
        mbedtls
)

target_include_directories(Tarefa1 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(Tarefa1)