#include <stdio.h>
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

void setServo(int angle);

#define PWMPIN 16

bool timer_interrupt_function(__unused struct repeating_timer *t) {
    // read the adc
    uint16_t result1 = adc_read();
    // print the voltage
    printf("%f\r\n",(float)result1/4095*3.3);
    return true;
}

int main()
{
    stdio_init_all();
    cyw43_arch_init();

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    struct repeating_timer timer;
    add_repeating_timer_ms(-100, timer_interrupt_function, NULL, &timer);

    // turn on the pwm
    gpio_set_function(PWMPIN, GPIO_FUNC_PWM); // Set the LED Pin to be PWM
    uint slice_num = pwm_gpio_to_slice_num(PWMPIN); // Get PWM slice number
    // the clock frequency is 150MHz divided by a float from 1 to 255
    float div = 50; // must be between 1-255
    pwm_set_clkdiv(slice_num, div); // sets the clock speed
    uint16_t wrap = 60000; // when to rollover, must be less than 65535

    // set the PWM frequency and resolution
    // this sets the PWM frequency to 150MHz / div / wrap
    pwm_set_wrap(slice_num, wrap);
    pwm_set_enabled(slice_num, true); // turn on the PWM

    pwm_set_gpio_level(PWMPIN, 0); // set the duty cycle to 50%

    // turn on the adc
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    while (true) {
        // loop through servo angles
        int i = 0;
        for (i=10; i<170; i++){
            setServo(i);
            sleep_ms(10);
        }
        for (i=170; i>10; i--){
            setServo(i);
            sleep_ms(10);
        }
        // pwm_set_gpio_level(PWMPIN, (int)(0.05*60000));
        // sleep_ms(1000);
        // pwm_set_gpio_level(PWMPIN, (int)(0.05));
        // sleep_ms(1000);
    }
}

void setServo(int angle){
    pwm_set_gpio_level(PWMPIN, (int)((0.03+(angle/180.0)*0.1)*60000));
}