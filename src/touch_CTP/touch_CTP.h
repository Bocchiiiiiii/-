/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t tap_count;
    int16_t x_offset;
    int16_t y_offset;
    int16_t c_x_offset;
    int16_t c_y_offset;
    uint16_t width;
    uint16_t height;
    bool needs_redraw;
    bool double_state;
    bool slide_state;
    char status_text[32];
    char direction_text[16];
} touch_ui_state_t;

/**
 * @brief Create a new touch driver (IT7259 via I2C)
 */
esp_err_t esp_lcd_touch_new_i2c_touch(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch);

/**
 * @brief 处理触摸点和手势
 */
bool process_touch_and_gestures(esp_lcd_touch_handle_t tp);

/**
 * @brief 获取UI状态
 */
touch_ui_state_t* get_touch_ui_state(void);

/**
 * @brief 重置重绘标志
 */
void reset_touch_redraw_flag(void);

/**
 * @brief 初始化触摸UI状态
 */
void init_touch_ui_state(int16_t offset_x, int16_t offset_y);

/**
 * @brief I2C address of IT7259 touch controller
 */
#define ESP_LCD_TOUCH_IO_I2C_TOUCH_ADDRESS (0x46)

#ifdef __cplusplus
}
#endif
