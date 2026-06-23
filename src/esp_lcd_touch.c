#include "esp_lcd_touch.h"

esp_err_t esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_handle_t tp, esp_lcd_touch_interrupt_callback_t callback)
{
    if (!tp) return ESP_ERR_INVALID_ARG;
    tp->config.interrupt_callback = callback;
    return ESP_OK;
}
