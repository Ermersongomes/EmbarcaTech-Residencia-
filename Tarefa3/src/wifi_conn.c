#include "headers/wifi_conn.h"
#include "pico/cyw43_arch.h"

// Conecta-se a uma rede Wi-Fi com as credenciais fornecidas.
bool connect_to_wifi(const char* ssid, const char* password) {
    // Inicializa o hardware Wi-Fi.
    if (cyw43_arch_init()) {
        return false;
    }

    // Habilita o modo station (cliente Wi-Fi).
    cyw43_arch_enable_sta_mode();

    // Tenta conectar à rede com um timeout de 30 segundos.
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 30000) == 0) {
        return true; // Sucesso na conexão.
    } else {
        return false; // Falha na conexão.
    }
}