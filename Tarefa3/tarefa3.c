#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

#include "headers/led.h"
#include "headers/wifi_conn.h"
#include "headers/mqtt_comm.h"
#include "headers/sensor_mpu6050.h"

// =================== DADOS DE CONFIGURAÇÃO (PREENCHA AQUI!) ===================
#define WIFI_SSID "Sofia_2G"
#define WIFI_PASSWORD "@58J7ANP11#"
#define CHALLENGE_NUMBER "12"
#define STUDENT_FIRST_NAME "ermerson"
#define STUDENT_LAST_NAME "silva"
// ==============================================================================

// --- Configurações do Broker MQTT ---
#define MQTT_BROKER_HOSTNAME "mqtt.iot.natal.br"
#define MQTT_USERNAME "desafio" CHALLENGE_NUMBER
#define MQTT_PASSWORD "desafio" CHALLENGE_NUMBER ".laica"
#define MQTT_CLIENT_NAME "bitdoglab_" STUDENT_FIRST_NAME

// --- Tópico MQTT para o MPU6050 ---
#define MQTT_TOPIC_MPU "ha/desafio" CHALLENGE_NUMBER "/" STUDENT_FIRST_NAME "." STUDENT_LAST_NAME "/mpu6050"

// --- Variáveis Globais ---
static bool g_wifi_is_connected = false;
static SemaphoreHandle_t g_i2c_mutex; 
static char g_ip_address[20] = "0.0.0.0";



// ================= Funções Auxiliares ================= 

// Pisca o LED verde para indicar que uma mensagem MQTT foi publicada.
static void indicate_mqtt_publication(void) {
    gpio_put(LED_G, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(LED_G, 0);
}

// Publica uma mensagem em um tópico MQTT específico.
static void publish_message(const char *topic, const char *payload) {
    // Tenta publicar a mensagem. A tarefa de gerenciamento de conexão é responsável por restabelecer a conexão em caso de falha.
    if (mqtt_comm_publish(topic, (const uint8_t *)payload, strlen(payload), false)) {
        printf("Publicado em %s:\n%s\n", topic, payload);
        indicate_mqtt_publication();
    } else {
        printf("Falha ao publicar em %s.\n", topic);
    }
}

// ========================= Tarefas do FreeRTOS =========================

//Tarefa de gerenciamento de conexão. Monitora continuamente as conexões Wi-Fi e MQTT. Se a conexão MQTT cair, esta tarefa tentará restabelecê-la.
void wifi_management_task(void *pvParameters) {
    while (true) {
        if (g_wifi_is_connected) {
            // Se o Wi-Fi está conectado, verifica o status do cliente MQTT.
            if (!mqtt_is_connected()) {
                printf("[AVISO] Cliente MQTT desconectado. Tentando reconectar...\n");
                // Tenta restabelecer a conexão com o broker
                mqtt_setup(MQTT_CLIENT_NAME, MQTT_BROKER_HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
            }
        } else {
            // Se o Wi-Fi não está conectado, tenta conectar.
            printf("[STATUS] Conectando a rede Wi-Fi: %s\n", WIFI_SSID);
            if (connect_to_wifi(WIFI_SSID, WIFI_PASSWORD)) {
                g_wifi_is_connected = true;
                strncpy(g_ip_address, ip4addr_ntoa(netif_ip4_addr(netif_default)), sizeof(g_ip_address));
                printf("[OK] Conexao Wi-Fi estabelecida! IP: %s\n", g_ip_address);
                
                printf("[STATUS] Iniciando cliente MQTT...\n");
                mqtt_setup(MQTT_CLIENT_NAME, MQTT_BROKER_HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
            } else {
                g_wifi_is_connected = false; 
                cyw43_arch_deinit();
                printf("[ERRO] Falha na conexao Wi-Fi. Nova tentativa em 5s.\n");
            }
        }
        // Aguarda 5 segundos antes da próxima verificação.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


// Tarefa que lê os dados do sensor MPU6050 e os publica via MQTT.
void mpu6050_reading_task(void *pvParameters) {
    int16_t accel_raw[3], gyro_raw[3], temp_raw;
    float accel_g[3], gyro_dps[3], temp_c;
    
    char json_payload[512]; 

    // Aguarda a conexão Wi-Fi e MQTT ser estabelecida pela primeira vez.
    while (!g_wifi_is_connected || !mqtt_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) {
        // Apenas publica se o MQTT estiver conectado.
        if (mqtt_is_connected()) {
            if (xSemaphoreTake(g_i2c_mutex, portMAX_DELAY) == pdTRUE) {
                // 1. Lê os dados brutos do sensor.
                read_raw_data(accel_raw, gyro_raw, &temp_raw);
                xSemaphoreGive(g_i2c_mutex);

                // 2. Converte os dados para unidades padrão (g, °/s, °C).
                mpu6050_convert_to_g(accel_raw, accel_g);
                mpu6050_convert_to_dps(gyro_raw, gyro_dps);
                temp_c = mpu6050_convert_to_celsius(temp_raw);

                // 3. Monta a string JSON com os dados do sensor.
                snprintf(json_payload, sizeof(json_payload),
                    "{\"team\":\"desafio%s\",\"device\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"sensor\":\"MPU-6050\",\"data\":{\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"gyro\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"temperature\":%.1f},\"timestamp\":\"2025-09-06T10:00:16\"}",
                    CHALLENGE_NUMBER,
                    MQTT_CLIENT_NAME,
                    g_ip_address,
                    WIFI_SSID,
                    accel_g[0], accel_g[1], accel_g[2],     
                    gyro_dps[0], gyro_dps[1], gyro_dps[2], 
                    temp_c                                  
                );

                // 4. Publica a mensagem JSON no tópico MQTT.
                publish_message(MQTT_TOPIC_MPU, json_payload);
            }
        }
        
        // Aguarda 10 segundos para a próxima publicação.
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// ========================= Função Principal =========================

int main() {
    stdio_init_all();
    sleep_ms(7000); // Atraso para inicialização do terminal serial.

    printf("=== INICIALIZANDO PROJETO MPU6050 + Home Assistant ===\n");

    // Inicializa os periféricos.
    setup_led();
    
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    init_mpu6050();
    printf("MPU6050 inicializado.\n");

    // Cria um mutex para proteger o barramento I2C contra acessos concorrentes.
    g_i2c_mutex = xSemaphoreCreateMutex();
    if (g_i2c_mutex == NULL) {
        printf("ERRO CRITICO: Falha ao criar mutex do I2C!\n");
        while (true);
    }

    // Cria as tarefas do sistema operacional.
    xTaskCreate(wifi_management_task, "WiFi_Task", 1024, NULL, 2, NULL);
    xTaskCreate(mpu6050_reading_task, "MPU_Task", 1024, NULL, 1, NULL);

    // Inicia o escalonador do FreeRTOS.
    vTaskStartScheduler();

    while (true); // O código nunca deve chegar aqui.
    return 0;
}