// Função modificada em src/wifi_conn.c
#include "headers/wifi_conn.h"
#include "pico/cyw43_arch.h"

bool connect_to_wifi(const char* ssid, const char* password) {
    // A inicialização agora é feita aqui para garantir um ciclo completo a cada tentativa.
    if (cyw43_arch_init()) {
        return false;
    }

    cyw43_arch_enable_sta_mode();

    // Tenta conectar com um timeout de 30 segundos.
    // O resultado (sucesso ou falha) é retornado para a função que chamou.
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 30000) == 0) {
        return true; // Sucesso
    } else {
        return false; // Falha
    }
}