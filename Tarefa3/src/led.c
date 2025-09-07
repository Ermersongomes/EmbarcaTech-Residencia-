#include "pico/stdlib.h"
#include "headers/led.h"

// Configura os pinos dos LEDs como saídas.
void setup_led() {
    // Inicializa os pinos GPIO para os LEDs.
    gpio_init(LED_G);
    gpio_init(LED_B);
    gpio_init(LED_R);

    // Define a direção dos pinos como saída.
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_set_dir(LED_R, GPIO_OUT);

    // Apaga todos os LEDs.
    gpio_put(LED_G, 0);
    gpio_put(LED_B, 0);
    gpio_put(LED_R, 0);
}