#include <stdio.h>         
#include <string.h>
#include "pico/stdlib.h"    
#include "hardware/adc.h"   
#include "FreeRTOS.h"       
#include "task.h"           
#include "semphr.h"         
#include "ssd1306.h"        
#include "hardware/i2c.h"   
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"

// TODO: Defina aqui o seu desafio e nome.
#define DESAFIO_NUM "XX"
#define NOME "primeiro.ultimo"

// Definições do Broker MQTT
#define MQTT_BROKER_HOST "mqtt.iot.natal.br"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_USER "desafio" DESAFIO_NUM
#define MQTT_CLIENT_PASS "desafio" DESAFIO_NUM ".laica"
#define MQTT_CLIENT_ID "pico_client_" NOME

// Tópicos personalizados
#define TOPIC_TEMP "ha/desafio" DESAFIO_NUM "/" NOME "/temp"
#define TOPIC_JOY "ha/desafio" DESAFIO_NUM "/" NOME "/joy"

// Definições dos pinos do display OLED (re-incluídas)
#define I2C_PORT i2c1
#define I2C_SDA 15
#define I2C_SCL 14

// Estruturas para comunicação via Queue
typedef struct {
    float temperature;
} temp_msg_t;

typedef struct {
    char direction[10];
} joy_msg_t;

// Handles para as Queues e Semáforo
QueueHandle_t temp_queue;
QueueHandle_t joy_queue;
SemaphoreHandle_t xADCMutex;

// Declaração global do display (re-incluída)
ssd1306_t disp;

// Variável global para armazenar a última posição do joystick
static char last_joy_position[10] = "Centro";
static float latest_average_temperature = 0.0f;

// Define os pinos
const uint JOYSTICK_X_PIN = 26; // ADC0
const uint JOYSTICK_Y_PIN = 27; // ADC1

//display oled
void oled_draw_centered_text(ssd1306_t *display, const char *texto, int y) {
    int largura = strlen(texto) * 6;
    int x = (128 - largura) / 2;
    ssd1306_draw_string(display, x, y, 1, texto);
}

// Leitura da temperatura no pico
float read_onboard_temperature(const char unit) {
    const float conversionFactor = 3.3f / (1 << 12);
    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;
    if (unit == 'C') {
        return tempC;
    }
    return -1.0f;
}

// Funções da Tarefa Blink para sinalizar publicação
void vTaskBlinkLED(void *pvParameters) {
    for (;;) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(950));
    }
}

// Tarefa responsável por ler a temperatura do sensor do Pico
void vTaskReadTemperature(void *pvParameters) {
    float readings[40]; // NUM_READINGS 40
    int reading_index = 0;
    float sum = 0;
    temp_msg_t temp_msg;
    uint temp_sensor_adc_pin = 4;

    for (;;) {
        if (xSemaphoreTake(xADCMutex, portMAX_DELAY) == pdTRUE) {
            adc_select_input(temp_sensor_adc_pin);
            float current_temperature = read_onboard_temperature('C');

            if (reading_index >= 40) {
                sum -= readings[reading_index % 40];
            }

            readings[reading_index % 40] = current_temperature;
            sum += current_temperature;
            reading_index++;

            if (reading_index >= 40) {
                latest_average_temperature = sum / 40;
                temp_msg.temperature = latest_average_temperature;
                xQueueSend(temp_queue, &temp_msg, portMAX_DELAY);
                sum = 0;
                reading_index = 0;
            }
            xSemaphoreGive(xADCMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Tarefa responsável por ler a posição de um joystick
void vTaskReadJoystick(void *pvParameters) {
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
    const uint adc_max = (1 << 12) - 1;
    char current_joy_position[10];
    uint threshold_high = adc_max * 0.8;
    uint threshold_low = adc_max * 0.2;
    joy_msg_t joy_msg;

    for (;;) {
        if (xSemaphoreTake(xADCMutex, portMAX_DELAY) == pdTRUE) {
            adc_select_input(JOYSTICK_X_PIN - 26);
            uint adc_x_raw = adc_read();
            adc_select_input(JOYSTICK_Y_PIN - 26);
            uint adc_y_raw = adc_read();

            if (adc_y_raw < threshold_low) {
                strcpy(current_joy_position, "esquerda");
            } else if (adc_y_raw > threshold_high) {
                strcpy(current_joy_position, "direita");
            } else if (adc_x_raw < threshold_low) {
                strcpy(current_joy_position, "baixo");
            } else if (adc_x_raw > threshold_high) {
                strcpy(current_joy_position, "cima");
            } else {
                strcpy(current_joy_position, "centro");
            }

            if (strcmp(current_joy_position, last_joy_position) != 0) {
                strcpy(last_joy_position, current_joy_position);
                strcpy(joy_msg.direction, current_joy_position);
                xQueueSend(joy_queue, &joy_msg, portMAX_DELAY);
            }

            xSemaphoreGive(xADCMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Função de callback para a conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if(status == MQTT_CONNECT_ACCEPTED) {
        printf("[MQTT] Conectado ao broker com sucesso!\n");
    } else {
        printf("[MQTT] Falha na conexão com o broker: %d\n", status);
    }
}

// Tarefa para conectar e publicar dados via MQTT
// Tarefa para conectar e publicar dados via MQTT
void vTaskMQTT(void *pvParameters) {
    printf("[MQTT] Inicializando MQTT Task...\n");
    
    // Inicialização do Wi-Fi
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_BRAZIL)) {
        printf("Falha na inicialização do Wi-Fi\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

    // A chamada para a função de conexão agora usará as macros de pico_config.h
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("[WIFI] Falha na conexão. Tentando novamente...\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    printf("[WIFI] Conectado com sucesso!\n");

    mqtt_client_t* mqtt_client = mqtt_client_new();
    struct mqtt_connect_client_info_t ci;
    memset(&ci, 0, sizeof(ci));
    ci.client_id = MQTT_CLIENT_ID;
    ci.client_user = MQTT_CLIENT_USER;
    ci.client_pass = MQTT_CLIENT_PASS;
    ci.keep_alive = 60;
    ci.will_topic = NULL;

    ip_addr_t broker_addr;
    ip4addr_aton(MQTT_BROKER_HOST, &broker_addr);

    printf("[MQTT] Tentando conectar ao broker em %s...\n", MQTT_BROKER_HOST);
    mqtt_client_connect(mqtt_client, &broker_addr, MQTT_BROKER_PORT, mqtt_connection_cb, NULL, &ci);

    temp_msg_t temp_msg;
    joy_msg_t joy_msg;
    TickType_t xLastPublishTime;
    xLastPublishTime = xTaskGetTickCount();

    for (;;) {
        // Publicação periódica de temperatura (a cada 30 segundos)
        if (xTaskGetTickCount() - xLastPublishTime > pdMS_TO_TICKS(30000)) {
            char payload[10];
            snprintf(payload, sizeof(payload), "%.0f", latest_average_temperature);
            if(mqtt_client_is_connected(mqtt_client)){
                mqtt_publish(mqtt_client, TOPIC_TEMP, payload, strlen(payload), 1, true, NULL, NULL);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                sleep_ms(50);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            }
            xLastPublishTime = xTaskGetTickCount();
        }

        // Publicação da direção do joystick ao receber uma nova mensagem da fila
        if (xQueueReceive(joy_queue, &joy_msg, 0) == pdPASS) {
            if(mqtt_client_is_connected(mqtt_client)){
                mqtt_publish(mqtt_client, TOPIC_JOY, joy_msg.direction, strlen(joy_msg.direction), 1, true, NULL, NULL);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                sleep_ms(50);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            }
        }
        
        // Exibe no display OLED (opcional)
        ssd1306_clear(&disp);
        char temp_str[20];
        snprintf(temp_str, sizeof(temp_str), "%.0f %c", latest_average_temperature, 'C');
        oled_draw_centered_text(&disp, temp_str, 16);
        oled_draw_centered_text(&disp, "Joystick:", 32);
        oled_draw_centered_text(&disp, last_joy_position, 40);
        ssd1306_show(&disp);
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Pequeno delay para a tarefa não consumir 100% da CPU
    }
}

int main() {
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    xADCMutex = xSemaphoreCreateMutex();
    if (xADCMutex == NULL) {
        printf("Erro ao criar o mutex para o ADC!\n");
        return -1;
    }

    // Cria as queues para comunicação entre as tasks
    temp_queue = xQueueCreate(10, sizeof(temp_msg_t));
    joy_queue = xQueueCreate(10, sizeof(joy_msg_t));

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
    ssd1306_clear(&disp);
    oled_draw_centered_text(&disp, "Iniciando...", 28);
    ssd1306_show(&disp);
    sleep_ms(2000);

    // Criação das tarefas
    xTaskCreate(vTaskBlinkLED, "BlinkLED", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskReadTemperature, "ReadTemp", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskReadJoystick, "ReadJoy", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskMQTT, "MQTT_Task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    while (true) {}
}