#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "headers/mqtt_comm.h"
#include "lwipopts.h"
#include <stdbool.h>
#include <stdio.h>

// Cliente MQTT e informações de conexão.
static mqtt_client_t *s_mqtt_client;
static struct mqtt_connect_client_info_t s_client_info;

// --- Funções de Callback ---

// Chamada quando o status da conexão MQTT muda.
static void mqtt_connection_callback(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("[MQTT] Conexão com o broker bem-sucedida.\n");
    } else {
        printf("[MQTT] Falha na conexão, codigo de erro: %d\n", status);
    }
}

// Chamada após uma tentativa de publicação.
static void mqtt_publication_callback(void *arg, err_t result) {
    if (result != ERR_OK) {
        printf("[MQTT] Erro ao confirmar publicacao, codigo: %d\n", result);
    }
}

// --- Funções Internas ---

// Inicia a conexão com o broker MQTT.
static void start_broker_connection(const ip_addr_t *broker_addr) {
    printf("[MQTT] Tentando conectar ao IP %s\n", ipaddr_ntoa(broker_addr));
    mqtt_client_connect(s_mqtt_client, broker_addr, 1883, mqtt_connection_callback, NULL, &s_client_info);
}

// Chamada após a resolução de um hostname via DNS.
static void dns_resolution_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr != NULL) {
        printf("[DNS] Hostname resolvido para o IP: %s\n", ipaddr_ntoa(ipaddr));
        start_broker_connection(ipaddr);
    } else {
        printf("[DNS] Falha ao resolver o hostname do broker.\n");
    }
}

// --- Funções Públicas (API) ---

// Configura e inicia o cliente MQTT.
void mqtt_setup(const char *client_id, const char *broker_host, const char *user, const char *pass) {
    s_mqtt_client = mqtt_client_new();
    if (s_mqtt_client == NULL) {
        printf("[MQTT] Falha ao alocar memoria para o cliente.\n");
        return;
    }

    // Armazena as informações de conexão.
    s_client_info.client_id = client_id;
    s_client_info.client_user = user;
    s_client_info.client_pass = pass;
    s_client_info.keep_alive = 60;

    // Resolve o hostname do broker para um endereço IP.
    ip_addr_t broker_addr;
    err_t err = dns_gethostbyname(broker_host, &broker_addr, dns_resolution_callback, NULL);

    if (err == ERR_OK) { // IP já está no cache.
        printf("[DNS] IP do broker encontrado no cache local.\n");
        start_broker_connection(&broker_addr);
    } else if (err != ERR_INPROGRESS) { // Erro ao iniciar a resolução.
        printf("[DNS] Erro ao iniciar resolucao: %d\n", err);
    }
}

// Publica uma mensagem em um tópico.
bool mqtt_comm_publish(const char *topic, const uint8_t *data, size_t len, bool retain) {
    if (!mqtt_client_is_connected(s_mqtt_client)) {
        return false;
    }

    err_t status = mqtt_publish(
        s_mqtt_client,
        topic,
        data,
        len,
        0, // QoS 0
        retain ? 1 : 0,
        mqtt_publication_callback,
        NULL
    );

    return status == ERR_OK;
}

// Verifica se o cliente MQTT está conectado.
bool mqtt_is_connected(void) {
    if (s_mqtt_client) {
        return mqtt_client_is_connected(s_mqtt_client);
    }
    return false;
}