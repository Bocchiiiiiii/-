#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_ESP_LCD_TOUCH_MAX_POINTS
#define CONFIG_ESP_LCD_TOUCH_MAX_POINTS 5
#endif

typedef struct esp_lcd_touch_t *esp_lcd_touch_handle_t;

typedef void (*esp_lcd_touch_interrupt_callback_t)(esp_lcd_touch_handle_t tp);

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t strength;
} esp_lcd_touch_coord_t;

typedef struct {
    uint16_t x_max;
    uint16_t y_max;
    int rst_gpio_num;
    int int_gpio_num;
    struct {
        unsigned int swap_xy : 1;
        unsigned int mirror_x : 1;
        unsigned int mirror_y : 1;
    } flags;
    struct {
        unsigned int reset : 1;
        unsigned int interrupt : 1;
    } levels;
    esp_lcd_touch_interrupt_callback_t interrupt_callback;
    void *user_data;
} esp_lcd_touch_config_t;

typedef struct {
    uint8_t points;
    esp_lcd_touch_coord_t coords[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];
    spinlock_t lock;
} esp_lcd_touch_data_t;

typedef struct esp_lcd_touch_t {
    void *io;
    esp_lcd_touch_data_t data;
    esp_lcd_touch_config_t config;
    esp_err_t (*read_data)(esp_lcd_touch_handle_t tp);
    bool (*get_xy)(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
    esp_err_t (*del)(esp_lcd_touch_handle_t tp);
    void *driver_data;
} esp_lcd_touch_t;

esp_err_t esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_handle_t tp, esp_lcd_touch_interrupt_callback_t callback);

#ifdef __cplusplus
}
#endif
