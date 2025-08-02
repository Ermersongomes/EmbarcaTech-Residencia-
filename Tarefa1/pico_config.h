#ifndef PICO_CONFIG_H
#define PICO_CONFIG_H

// --- Configurações do Wi-Fi ---
// Defina aqui as credenciais do seu Wi-Fi.
#define WIFI_SSID "Sofia_5G"
#define WIFI_PASSWORD ""

// --- Configurações do lwIP/FreeRTOS ---
// O lwIP precisa saber que está sendo usado com um RTOS.
// Definir NO_SYS como 0 é crucial para usar a API Sequencial.
#define NO_SYS 0

// Aumenta o número de timeouts do sistema para acomodar o cliente MQTT.
// O valor padrão no SDK é geralmente 4. Aqui estamos definindo um valor seguro.
#define MEMP_NUM_SYS_TIMEOUT (8)

// Habilita a API Sequencial (netconn, sockets), DNS e cliente MQTT.
#define LWIP_NETCONN 1
#define LWIP_SOCKET 1
#define LWIP_COMPAT_SOCKETS 1
#define LWIP_DNS 1
#define LWIP_MDNS_RESPONDER 1
#define LWIP_MQTT_CLIENT 1

// Define que a estrutura 'timeval' não deve ser definida pelo lwIP
// para evitar conflitos com a biblioteca padrão do C.
#define LWIP_TIMEVAL_PRIVATE 0

// Tamanhos de pilha para as threads internas do lwIP.
#define DEFAULT_THREAD_STACKSIZE 2048
#define TCPIP_THREAD_STACKSIZE 1024

#endif /* PICO_CONFIG_H */