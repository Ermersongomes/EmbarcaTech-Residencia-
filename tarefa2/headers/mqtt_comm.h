#ifndef MQTT_COMM_H
#define MQTT_COMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void mqtt_setup(const char *client_id, const char *broker_ip, const char *user, const char *pass);

// Modificado para incluir o parâmetro de retenção
bool mqtt_comm_publish(const char *topic, const uint8_t *data, size_t len, bool retain);

#endif