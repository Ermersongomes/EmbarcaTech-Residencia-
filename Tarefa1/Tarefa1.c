#include <stdio.h>         
#include "pico/stdlib.h"    
#include "hardware/adc.h"   
#include "FreeRTOS.h"       // Biblioteca do FreeRTOS
#include "task.h"           // Header do FreeRTOS para gerenciamento de tarefas (tasks).
#include "semphr.h"         // Header do FreeRTOS para utilização de semáforos
#include <string.h>         
#include "ssd1306.h"        
#include "hardware/i2c.h"   

// Define os pinos
const uint LED_PIN = 13;
const uint JOYSTICK_X_PIN = 26; // ADC0
const uint JOYSTICK_Y_PIN = 27; // ADC1

// Define os pinos do display
#define I2C_PORT i2c1
#define I2C_SDA 15
#define I2C_SCL 14
ssd1306_t disp;

// Handle para o semáforo (mutex) do ADC
SemaphoreHandle_t xADCMutex;

// Variável global para armazenar a temperatura média
float latest_average_temperature = 0.0f;

//display oled
void oled_draw_centered_text(ssd1306_t *display, const char *texto, int y) {
    int largura = strlen(texto) * 6;
    int x = (128 - largura) / 2;
    ssd1306_draw_string(display, x, y, 1, texto);
}

/* Choose 'C' for Celsius or 'F' for Fahrenheit. */
#define TEMPERATURE_UNITS 'C'
#define NUM_READINGS 40 // Número de medições para a média (média atualizada a cada ~8 segundos)

//Leitura da temperatura no pico
float read_onboard_temperature(const char unit) {
    const float conversionFactor = 3.3f / (1 << 12);
    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;
    if (unit == 'C') {
        return tempC;
    }
    return -1.0f; // Removido o Fahrenheit por não ser usado
}

// Funções da tarefa Blink, Tarefa responsável por piscar um LED conectado ao pino definido por LED_PIN.
void vTaskBlinkLED(void *pvParameters) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    for (;;) {
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(950));
    }
}

//Tarefa responsável por ler a temperatura do sensor do Pico, utilizando semáforo (mutex) para garantir acesso exclusivo ao ADC
void vTaskReadTemperature(void *pvParameters) {
    float readings[NUM_READINGS];
    int reading_index = 0;
    float sum = 0;
    float average_temperature;
    uint temp_sensor_adc_pin = 4;

    for (;;) {
        if (xSemaphoreTake(xADCMutex, portMAX_DELAY) == pdTRUE) {
            adc_select_input(temp_sensor_adc_pin);
            float current_temperature = read_onboard_temperature(TEMPERATURE_UNITS);

            if (reading_index >= NUM_READINGS) {
                sum -= readings[reading_index % NUM_READINGS];
            }

            readings[reading_index % NUM_READINGS] = current_temperature;
            sum += current_temperature;
            reading_index++;

            if (reading_index >= NUM_READINGS) {
                average_temperature = sum / NUM_READINGS;
                latest_average_temperature = average_temperature;
                sum = 0;
                reading_index = 0;
            }
            xSemaphoreGive(xADCMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

//Tarefa responsável por ler a posição de um joystick conectado aos pinos ADC0 e ADC1
void vTaskReadJoystick(void *pvParameters) {
    adc_gpio_init(JOYSTICK_X_PIN); // Inicializa o pino X do joystick como ADC
    adc_gpio_init(JOYSTICK_Y_PIN); // Inicializa o pino Y do joystick como ADC

    const uint adc_max = (1 << 12) - 1;
    char joystick_position[15] = "Centro";
    char temp_str[20];

    for (;;) {
        // Tenta pegar o semáforo. Espera indefinidamente até conseguir.
        if (xSemaphoreTake(xADCMutex, portMAX_DELAY) == pdTRUE) {
            // Read Joystick
            adc_select_input(JOYSTICK_X_PIN - 26); // ADC channel 0 é o  GPIO 26
            uint adc_x_raw = adc_read();
            adc_select_input(JOYSTICK_Y_PIN - 26); // ADC channel 1 é o GPIO 27
            uint adc_y_raw = adc_read();

            // Defina as faixas para cada direção.
            uint threshold_high = adc_max * 0.8;
            uint threshold_low = adc_max * 0.2;

            if (adc_y_raw < threshold_low) {
                strcpy(joystick_position, "Esquerda");
            } else if (adc_y_raw > threshold_high) {
                strcpy(joystick_position, "Direita");
            } else if (adc_x_raw < threshold_low) {
                strcpy(joystick_position, "Baixo");
            } else if (adc_x_raw > threshold_high) {
                strcpy(joystick_position, "Cima");
            } else {
                strcpy(joystick_position, "Centro");
            }

            snprintf(temp_str, sizeof(temp_str), "%.2f %c", latest_average_temperature, TEMPERATURE_UNITS);

            printf("posição do joystick: %s  |  Temp: %s\n", joystick_position, temp_str); //Porta serial
            
            //Exibi no display
            ssd1306_clear(&disp);
            oled_draw_centered_text(&disp, temp_str, 16);
            oled_draw_centered_text(&disp, "Joystick:", 32);
            oled_draw_centered_text(&disp, joystick_position, 40);
            ssd1306_show(&disp);

            // Libera o semáforo
            xSemaphoreGive(xADCMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
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

    xTaskCreate(vTaskBlinkLED, "BlinkLED", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskReadTemperature, "ReadTemp", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskReadJoystick, "ReadJoy", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    while (true) {
        // Não deve chegar aqui
    }
}