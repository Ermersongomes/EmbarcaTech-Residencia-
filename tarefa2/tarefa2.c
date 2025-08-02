#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include "headers/led.h"
#include "headers/wifi_conn.h"
#include "headers/mqtt_comm.h"

// =================== DADOS DE CONFIGURAÇÃO ===================
// --- Conexão Wi-Fi ---

#define WIFI_SSID "Sofia_2G"
#define WIFI_PASSWORD "@58J7ANP11#"

// --- Identificação do Aluno e Desafio ---

#define CHALLENGE_NUMBER "12"
#define STUDENT_FIRST_NAME "ermerson"
#define STUDENT_LAST_NAME "silva"
// =============================================================

//Configurações do Broker MQTT 
#define MQTT_BROKER_HOSTNAME "mqtt.iot.natal.br"
#define MQTT_USERNAME "desafio" CHALLENGE_NUMBER
#define MQTT_PASSWORD "desafio" CHALLENGE_NUMBER ".laica"
#define MQTT_CLIENT_NAME STUDENT_FIRST_NAME "." STUDENT_LAST_NAME

//Tópicos MQTT
#define MQTT_TOPIC_TEMP "ha/desafio" CHALLENGE_NUMBER "/" STUDENT_FIRST_NAME "." STUDENT_LAST_NAME "/temp"
#define MQTT_TOPIC_JOY "ha/desafio" CHALLENGE_NUMBER "/" STUDENT_FIRST_NAME "." STUDENT_LAST_NAME "/joy"

// Pinos e Configurações de Hardware
#define ADC_TEMP_SENSOR_CHANNEL 4 
#define ADC_JOYSTICK_X_PIN 26 // PIno da possição X do joystick
#define ADC_JOYSTICK_Y_PIN 27 // PIno da possição y do joystick

//Variáveis Globais
static bool g_wifi_is_connected = false;
static SemaphoreHandle_t g_adc_mutex;

// ================= Funções Auxiliares ================= 

// Pisca o LED verde para sinalizar uma publicação MQTT.
static void indicate_mqtt_publication(void)
{
    gpio_put(LED_G, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(LED_G, 0);
}

/* ============================================================
 | Essa função envia uma mensagem usando MQTT.                |
 |                                                            |
 | - topic: é o nome do tópico pra onde a mensagem vai.       |
 | - payload: é o que vai ser enviado (o conteúdo).           |
 | - retain: se for true, o broker vai guardar essa mensagem. |
 |                                                            |
 | Também acende o LED pra mostrar que enviou.                |
  ============================================================*/

static void publish_message_with_led_feedback(const char *topic, const char *payload, bool retain)
{
    if (!g_wifi_is_connected)
        return;

    if (mqtt_comm_publish(topic, (const uint8_t *)payload, strlen(payload), retain))
    {
        printf("Publicado em %s: %s\n", topic, payload);
        indicate_mqtt_publication();
    }
    else
    {
        printf("Falha ao publicar em %s.\n", topic);
    }
}

// ========================= Tarefas do FreeRTOS =========================

//que gerencia a conexão Wi-Fi e a inicialização do MQTT.
void wifi_management_task(void *pvParameters) {
    // Esta tarefa controla todos os logs de status da conexão.
    while (true) {
        if (!g_wifi_is_connected) {
            printf("[STATUS] Conectando a rede Wi-Fi: %s\n", WIFI_SSID);
            
            if (connect_to_wifi(WIFI_SSID, WIFI_PASSWORD)) {
                g_wifi_is_connected = true;
                printf("[OK] Conexao Wi-Fi estabelecida!\n");
                
                printf("[STATUS] Iniciando cliente MQTT...\n");
                mqtt_setup(MQTT_CLIENT_NAME, MQTT_BROKER_HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD);
            } else {
                // Se a conexão falhar, reinicializa para tentar de novo do zero.
                cyw43_arch_deinit(); 
                printf("[ERRO] Falha na conexao Wi-Fi. Nova tentativa em 10s.\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

//Tarefa que lê a temperatura interna periodicamente e a publica.
void temperature_reading_task(void *pvParameters)
{
    char temp_buffer[8];
    const float CONVERSION_FACTOR = 3.3f / (1 << 12);

    while (true)
    {
        // Aguarda o intervalo de 30 segundos para a próxima leitura/publicação
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (g_wifi_is_connected && xSemaphoreTake(g_adc_mutex, portMAX_DELAY) == pdTRUE)
        {
            adc_select_input(ADC_TEMP_SENSOR_CHANNEL);
            float adc_voltage = (float)adc_read() * CONVERSION_FACTOR;
            float temperature_celsius = 27.0f - (adc_voltage - 0.706f) / 0.001721f;

            xSemaphoreGive(g_adc_mutex); // Libera o recurso do ADC

            snprintf(temp_buffer, sizeof(temp_buffer), "%d", (int)temperature_celsius);
            publish_message_with_led_feedback(MQTT_TOPIC_TEMP, temp_buffer, true);
        }
    }
}

// Tarefa que monitora o joystick e publica mudanças de estado.
void joystick_monitoring_task(void *pvParameters)
{
    char last_pos[10] = "centro";
    char current_pos[10] = "centro";

    const uint16_t ADC_MAX = (1 << 12) - 1;
    const uint16_t THRESHOLD_HIGH = ADC_MAX * 0.8;
    const uint16_t THRESHOLD_LOW = ADC_MAX * 0.2;

    while (true)
    {
        if (g_wifi_is_connected && xSemaphoreTake(g_adc_mutex, portMAX_DELAY) == pdTRUE)
        {
            adc_select_input(ADC_JOYSTICK_X_PIN - 26);
            uint16_t x_value = adc_read();

            adc_select_input(ADC_JOYSTICK_Y_PIN - 26);
            uint16_t y_value = adc_read();

            xSemaphoreGive(g_adc_mutex); // Libera o recurso do ADC

            if (y_value < THRESHOLD_LOW)
                strcpy(current_pos, "ESQUERDA");
            else if (y_value > THRESHOLD_HIGH)
                strcpy(current_pos, "DIREITA");
            else if (x_value < THRESHOLD_LOW)
                strcpy(current_pos, "BAIXO");
            else if (x_value > THRESHOLD_HIGH)
                strcpy(current_pos, "CIMA");
            else
                strcpy(current_pos, "centro");
        }

        if (strcmp(last_pos, current_pos) != 0)
        {
            if (strcmp(current_pos, "centro") != 0)
            {
                publish_message_with_led_feedback(MQTT_TOPIC_JOY, current_pos, true);
            }
            strcpy(last_pos, current_pos);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Função que mostra uma mensagem no início do sistema
static void display_startup_banner(void) {
    char line_buffer[60];
    const int banner_width = 44;

    printf("\n+%.*s+\n", banner_width + 2, "==============================================");
    
    snprintf(line_buffer, sizeof(line_buffer), "Inicializando Sistema Embarcado MQTT IoT");
    printf("| %-*s |\n", banner_width, line_buffer);

    printf("+%.*s+\n", banner_width + 2, "----------------------------------------------");

    snprintf(line_buffer, sizeof(line_buffer), "Tarefa 2 - Desafio %s", CHALLENGE_NUMBER);
    printf("| %-*s |\n", banner_width, line_buffer);

    snprintf(line_buffer, sizeof(line_buffer), "Aluno: %s %s", STUDENT_FIRST_NAME, STUDENT_LAST_NAME);
    printf("| %-*s |\n", banner_width, line_buffer);
    
    printf("+%.*s+\n\n", banner_width + 2, "==============================================");
}

int main()
{
    stdio_init_all();

    // Aguarda um momento para o monitor serial estabilizar
    sleep_ms(6000);

    // Exibe o banner de boas-vindas
    display_startup_banner();

    // Inicializa o hardware
    setup_led();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(ADC_JOYSTICK_X_PIN);
    adc_gpio_init(ADC_JOYSTICK_Y_PIN);

    // Cria um mutex para garantir acesso seguro e exclusivo ao ADC
    g_adc_mutex = xSemaphoreCreateMutex();
    if (g_adc_mutex == NULL)
    {
        printf("ERRO CRITICO: Nao foi possivel criar o mutex do ADC!\n");
        while (true)
            ; // Trava o sistema em caso de falha
    }

    // Cria as tarefas do sistema 
    xTaskCreate(wifi_management_task, "WiFi_Task", 512, NULL, 2, NULL);
    xTaskCreate(temperature_reading_task, "Temp_Task", 256, NULL, 1, NULL);
    xTaskCreate(joystick_monitoring_task, "Joy_Task", 256, NULL, 1, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    while (true)
        ;
    return 0;
}