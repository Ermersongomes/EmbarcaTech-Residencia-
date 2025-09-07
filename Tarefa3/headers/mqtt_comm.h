#ifndef MQTT_COMM_H
#define MQTT_COMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lwip/apps/mqtt.h"

// Configura e inicia o cliente MQTT.
void mqtt_setup(const char *client_id, const char *broker_ip, const char *user, const char *pass);

// Publica uma mensagem em um tópico.
bool mqtt_comm_publish(const char *topic, const uint8_t *data, size_t len, bool retain);

// Verifica se o cliente MQTT está conectado.
bool mqtt_is_connected(void);

#endif