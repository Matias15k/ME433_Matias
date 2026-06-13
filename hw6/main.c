#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "hardware/gpio.h"
#include "usb_descriptors.h"
#include "mpu6050.h"

#define BUTTON_PIN 15   // wired to GND; uses internal pull-up
#define LED_PIN    14   // external LED (mode indicator)

// 0 = IMU mouse mode, 1 = circle (remote-working) mode
static int mode = 0;

static uint32_t blink_interval_ms = 250; // updated by mount callbacks

void led_blinking_task(void);
void hid_task(void);

int main(void) {
    board_init();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // LED off = IMU mode

    mpu6050_init();

    tud_init(BOARD_TUD_RHPORT);
    if (board_init_after_tusb) board_init_after_tusb();

    while (1) {
        tud_task();
        led_blinking_task();
        hid_task();
    }
}


void tud_mount_cb(void)    { blink_interval_ms = 1000; }
void tud_umount_cb(void)   { blink_interval_ms = 250;  }
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; blink_interval_ms = 2500; }
void tud_resume_cb(void)   { blink_interval_ms = tud_mounted() ? 1000 : 250; }

//for tiny isb

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
    (void)instance; (void)report; (void)len;
}

//translation
static int8_t accel_to_delta(int16_t a) {
    int16_t abs_a = a < 0 ? -a : a;
    int8_t mag;
    if      (abs_a < 1000)  mag = 0;
    else if (abs_a < 4000)  mag = 1;
    else if (abs_a < 9000)  mag = 3;
    else if (abs_a < 14000) mag = 5;
    else                    mag = 8;
    return (a < 0) ? -mag : mag;
}

//main hid
void hid_task(void) {
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;
    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    // Button debounce: require 3 consecutive stable samples before acting
    static bool prev_btn = true; // idle HIGH with pull-up
    static int  debounce_cnt = 0;
    bool cur_btn = gpio_get(BUTTON_PIN);
    if (cur_btn != prev_btn) {
        if (++debounce_cnt >= 3) {
            if (!cur_btn) { // falling edge = button pressed
                mode ^= 1;
                gpio_put(LED_PIN, mode); // LED on = circle mode
            }
            prev_btn = cur_btn;
            debounce_cnt = 0;
        }
    } else {
        debounce_cnt = 0;
    }

    if (tud_suspended()) {
        if (!gpio_get(BUTTON_PIN)) tud_remote_wakeup();
        return;
    }

    if (!tud_hid_ready()) return;

    int8_t dx = 0, dy = 0;

    if (mode == 0) {
        // IMU mode: tilt the board to move the cursor
        mpu6050_data_t d;
        mpu6050_read(&d);
        dx = accel_to_delta(d.ax);
        dy = accel_to_delta(d.ay);
    } else {
        // Circle mode: slow circle, ~3 seconds per revolution
        static float phase = 0.0f;
        static float rem_x = 0.0f, rem_y = 0.0f;
        const float R   = 80.0f;
        const float dph = 2.0f * 3.14159265f / 300.0f; // 300 steps = 3 s at 100 Hz

        // Tangential velocity components
        rem_x += -R * sinf(phase) * dph;
        rem_y +=  R * cosf(phase) * dph;

        phase += dph;
        if (phase >= 6.28318530f) phase -= 6.28318530f;

        // Integer truncation + carry fractional remainder so no motion is lost
        dx = (int8_t)rem_x; rem_x -= (float)dx;
        dy = (int8_t)rem_y; rem_y -= (float)dy;
    }

    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, dx, dy, 0, 0);
}

//blink LED
void led_blinking_task(void) {
    static uint32_t start_ms = 0;
    static bool led_state = false;

    if (!blink_interval_ms) return;
    if (board_millis() - start_ms < blink_interval_ms) return;
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = !led_state;
}
