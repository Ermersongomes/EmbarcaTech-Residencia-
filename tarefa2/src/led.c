#include "pico/stdlib.h"

#include "headers/led.h"

void setup_led()
{
    gpio_init(LED_G);
    gpio_init(LED_B);
    gpio_init(LED_R);

    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_set_dir(LED_R, GPIO_OUT);

    gpio_put(LED_G, 0);
    gpio_put(LED_B, 0);
    gpio_put(LED_R, 0);
}