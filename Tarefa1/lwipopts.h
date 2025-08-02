#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Inclui as opções padrão do Pico SDK, que contêm as definições de tempo
#include "pico/lwip_opts.h"

// Define a macro crítica para usar a API Sequencial com FreeRTOS
#define NO_SYS 0

// Aumenta o número de timeouts do sistema, conforme a necessidade do MQTT
#define MEMP_NUM_SYS_TIMEOUT (LWIP_PICO_NUM_CONNECTION_TIMEOUTS + LWIP_PICO_NUM_RAW_PCB_CALLBACKS + 4)

// Habilita as funcionalidades de rede necessárias para o projeto
#define LWIP_NETCONN 1
#define LWIP_SOCKET 1
#define LWIP_COMPAT_SOCKETS 1
#define LWIP_DNS 1
#define LWIP_MDNS_RESPONDER 1
#define LWIP_MQTT_CLIENT 1

// Define que a estrutura 'timeval' não deve ser definida pelo lwIP
// para evitar conflitos com a biblioteca padrão do C.
#define LWIP_TIMEVAL_PRIVATE 0

// Tamanhos de pilha para as threads internas do lwIP
#define DEFAULT_THREAD_STACKSIZE 2048
#define TCPIP_THREAD_STACKSIZE 1024

#endif // LWIPOPTS_H