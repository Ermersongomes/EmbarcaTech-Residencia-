

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")


include(pico_sdk_import.cmake)

cmake_minimum_required(VERSION 3.13)


# Pull in FreeRTOS
include($ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(tarefa2_project C CXX ASM) 
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_definitions(-DNDEBUG)


add_executable(tarefa2 
    tarefa2.c
    src/led.c
    src/mqtt_comm.c
    src/wifi_conn.c
)

target_include_directories(tarefa2 PRIVATE ${CMAKE_CURRENT_LIST_DIR})
 

target_link_libraries(tarefa2
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    hardware_adc
    hardware_i2c # Oferece suporte Wi-Fi (CYW43) com LwIP (pilha TCP/IP) em um modo thread-safe em segundo plano.
    pico_cyw43_arch_lwip_threadsafe_background # Adiciona suporte a cliente MQTT sobre o LwIP.
    pico_lwip_mqtt # Pilha Lightweight IP (LwIP), que fornece funcionalidades TCP/IP (HTTP, MQTT, UDP, DHCP, etc.).
    pico_lwip # driver do chip Wi-Fi CYW43 (usado no Pico W).
    pico_cyw43_driver
)


pico_enable_stdio_uart(tarefa2 0)
pico_enable_stdio_usb(tarefa2 1)


pico_add_extra_outputs(tarefa2)