/*
 * ESP32-S3 + TK0128F25K (GC9A01 LCD + IT7259 Touch) 电子吧唧
 *
 * 引脚接线:
 *   SPI SCLK: GPIO 6
 *   SPI MOSI: GPIO 15
 *   SPI DC:   GPIO 7
 *   SPI CS:   GPIO 16
 *   I2C SDA:  GPIO 4
 *   I2C SCL:  GPIO 5
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "touch_CTP.h"
#include "LCD_TK0128F25K.h"
#include "anim.h"

/* LCD 尺寸 */
#define EXAMPLE_LCD_H_RES   (240)
#define EXAMPLE_LCD_V_RES   (240)

/* I2C 触摸配置 */
#define EXAMPLE_I2C_NUM                 1
#define EXAMPLE_TOUCH_I2C_CLK_HZ        (400000)
#define EXAMPLE_I2C_SCL                 (GPIO_NUM_5)
#define EXAMPLE_I2C_SDA                 (GPIO_NUM_4)

/* LCD SPI 配置 */
#define EXAMPLE_LCD_SPI_NUM          (SPI2_HOST)
#define EXAMPLE_LCD_PIXEL_CLK_HZ     (40 * 1000 * 1000)
#define EXAMPLE_LCD_CMD_BITS         (8)
#define EXAMPLE_LCD_PARAM_BITS       (8)
#define EXAMPLE_LCD_COLOR_SPACE      (LCD_RGB_ELEMENT_ORDER_BGR)
#define EXAMPLE_LCD_BITS_PER_PIXEL   (16)
#define EXAMPLE_LCD_BL_ON_LEVEL      (1)

/* LCD 引脚 */
#define EXAMPLE_LCD_GPIO_SCLK       (GPIO_NUM_6)
#define EXAMPLE_LCD_GPIO_MOSI       (GPIO_NUM_15)
#define EXAMPLE_LCD_GPIO_RST        (GPIO_NUM_NC)
#define EXAMPLE_LCD_GPIO_DC         (GPIO_NUM_7)
#define EXAMPLE_LCD_GPIO_CS         (GPIO_NUM_16)
#define EXAMPLE_LCD_GPIO_BL         (GPIO_NUM_NC)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static const char *TAG = "badge";

static esp_lcd_panel_io_handle_t tp_io_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;

/* 触摸初始化 */
static void lcd_touch_init(void)
{
    i2c_master_bus_handle_t codec_i2c_bus;

    i2c_master_bus_config_t i2c_bus_cfg1 = {
        .i2c_port = EXAMPLE_I2C_NUM,
        .sda_io_num = EXAMPLE_I2C_SDA,
        .scl_io_num = EXAMPLE_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg1, &codec_i2c_bus));

    esp_lcd_panel_io_i2c_config_t tp_io_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_TOUCH_ADDRESS,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .flags = {
            .disable_control_phase = 1,
        },
        .scl_speed_hz = 400000,
    };

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(codec_i2c_bus, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_V_RES,
        .y_max = EXAMPLE_LCD_H_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_LOGI(TAG, "Initialize touch");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_touch(tp_io_handle, &tp_cfg, &touch_handle));
}

/* 初始化 SPI */
static esp_err_t spi_init(void)
{
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = EXAMPLE_LCD_GPIO_MOSI;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = EXAMPLE_LCD_GPIO_SCLK;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    return ESP_OK;
}

/**
 * @brief 在指定矩形区域填充颜色
 */
esp_err_t fill_rect(esp_lcd_panel_handle_t panel, int x_start, int y_start, int x_end, int y_end, uint16_t color)
{
    if (x_start < 0 || y_start < 0 || x_end > 240 || y_end > 240 ||
        x_start >= x_end || y_start >= y_end) {
        ESP_LOGE(TAG, "Invalid rectangle parameters: (%d,%d)-(%d,%d)",
                 x_start, y_start, x_end, y_end);
        return ESP_ERR_INVALID_ARG;
    }

    int width = x_end - x_start;

    uint16_t *line_buffer = malloc(width * sizeof(uint16_t));
    if (!line_buffer) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < width; i++) {
        line_buffer[i] = color;
    }

    for (int y = y_start; y < y_end; y++) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, x_start, y, x_end, y + 1, line_buffer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw line %d: %s", y, esp_err_to_name(ret));
            free(line_buffer);
            return ret;
        }
    }

    free(line_buffer);
    return ESP_OK;
}

/**
 * @brief 填充整个屏幕
 */
esp_err_t fill_screen(esp_lcd_panel_handle_t panel, uint16_t color)
{
    return fill_rect(panel, 0, 0, 240, 240, color);
}

/**
 * @brief 双线性插值缩放（整数运算，平滑无锯齿）
 */
static uint16_t *image_scale_bilinear(uint16_t src_w, uint16_t src_h, const uint16_t *src,
                                      uint16_t dst_w, uint16_t dst_h)
{
    uint16_t *dst = heap_caps_malloc(dst_w * dst_h * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!dst) return NULL;

    // 定点数：16 位整数 + 8 位小数
    uint32_t x_step = ((uint32_t)(src_w - 1) << 8) / dst_w;
    uint32_t y_step = ((uint32_t)(src_h - 1) << 8) / dst_h;

    uint32_t y_pos = 0;
    for (uint16_t dy = 0; dy < dst_h; dy++) {
        uint16_t sy = y_pos >> 8;
        uint8_t  wy = y_pos & 0xFF;
        uint16_t sy2 = (sy + 1 < src_h) ? sy + 1 : sy;

        uint32_t x_pos = 0;
        for (uint16_t dx = 0; dx < dst_w; dx++) {
            uint16_t sx = x_pos >> 8;
            uint8_t  wx = x_pos & 0xFF;
            uint16_t sx2 = (sx + 1 < src_w) ? sx + 1 : sx;

            // 读取四个角点，分离为 R/G/B 通道 (0-255)
            uint16_t p00 = src[sy  * src_w + sx];
            uint16_t p10 = src[sy  * src_w + sx2];
            uint16_t p01 = src[sy2 * src_w + sx];
            uint16_t p11 = src[sy2 * src_w + sx2];

            // 解包 RGB565 → R8 G8 B8
            uint8_t r00 = (p00 >> 8) & 0xF8; r00 |= r00 >> 5;
            uint8_t g00 = (p00 >> 3) & 0xFC; g00 |= g00 >> 6;
            uint8_t b00 = (p00 << 3) & 0xF8; b00 |= b00 >> 5;

            uint8_t r10 = (p10 >> 8) & 0xF8; r10 |= r10 >> 5;
            uint8_t g10 = (p10 >> 3) & 0xFC; g10 |= g10 >> 6;
            uint8_t b10 = (p10 << 3) & 0xF8; b10 |= b10 >> 5;

            uint8_t r01 = (p01 >> 8) & 0xF8; r01 |= r01 >> 5;
            uint8_t g01 = (p01 >> 3) & 0xFC; g01 |= g01 >> 6;
            uint8_t b01 = (p01 << 3) & 0xF8; b01 |= b01 >> 5;

            uint8_t r11 = (p11 >> 8) & 0xF8; r11 |= r11 >> 5;
            uint8_t g11 = (p11 >> 3) & 0xFC; g11 |= g11 >> 6;
            uint8_t b11 = (p11 << 3) & 0xF8; b11 |= b11 >> 5;

            // 双线性混合（整数额，范围 0-255）
            #define BLEND(a, b, w) (((a) * (256 - (w)) + (b) * (w)) >> 8)
            uint8_t r = BLEND(BLEND(r00, r10, wx), BLEND(r01, r11, wx), wy);
            uint8_t g = BLEND(BLEND(g00, g10, wx), BLEND(g01, g11, wx), wy);
            uint8_t b = BLEND(BLEND(b00, b10, wx), BLEND(b01, b11, wx), wy);

            // 打包回 RGB565
            dst[dy * dst_w + dx] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

            x_pos += x_step;
        }
        y_pos += y_step;
    }
    return dst;
}

/* LCD 初始化 */
static esp_err_t lcd_init(void)
{
    /* LCD backlight - not used (BL pin is NC) */

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_LCD_GPIO_DC,
        .cs_gpio_num = EXAMPLE_LCD_GPIO_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &io_config, &lcd_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_LCD_GPIO_RST,
        .rgb_ele_order = EXAMPLE_LCD_COLOR_SPACE,
        .bits_per_pixel = EXAMPLE_LCD_BITS_PER_PIXEL,
    };

    ESP_LOGI(TAG, "Install TK0128F25K panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_tk0128f25k(lcd_io, &panel_config, &lcd_panel));

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_disp_on_off(lcd_panel, true);
    esp_lcd_panel_invert_color(lcd_panel, true);

    return ESP_OK;
}

/* 触摸任务 */
void touch_task(void *pvParameters)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)pvParameters;

    while (1) {
        process_touch_and_gestures(tp);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    vTaskDelete(NULL);
}

/* 显示任务 —— 从 raw 分区读取 GIF 帧并播放 */
void display_task(void *pvParameters)
{
    // 查找名为 "frames" 的 raw data 分区
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "frames");
    if (!part) {
        ESP_LOGE(TAG, "Partition 'frames' not found!");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Found frames partition: offset=0x%lx size=%lu KB",
             part->address, part->size / 1024);

    int16_t draw_x = (240 - ANIM_FRAME_SIZE) / 2;
    int16_t draw_y = (240 - ANIM_FRAME_SIZE) / 2;
    (void)draw_x; (void)draw_y;  // unused, kept for reference

    uint16_t *frame_buf = heap_caps_malloc(ANIM_FRAME_BYTES, MALLOC_CAP_DMA);
    if (!frame_buf) {
        ESP_LOGE(TAG, "Failed to alloc frame buffer!");
        vTaskDelete(NULL);
        return;
    }

    int frame_idx = 0;
    while (1) {
        esp_err_t ret = esp_partition_read(part, frame_idx * ANIM_FRAME_BYTES,
                                           frame_buf, ANIM_FRAME_BYTES);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Frame %d read error", frame_idx);
            frame_idx = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 240x240 原生分辨率，直接绘制
        lcd_draw_image_rgb565(lcd_panel, 0, 0, 240, 240, frame_buf);

        frame_idx++;
        if (frame_idx >= ANIM_FRAME_COUNT) frame_idx = 0;

        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Electronic Badge - ESP32-S3 + TK0128F25K");
    ESP_LOGI(TAG, "========================================");

    /* 打印芯片信息 */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP32-S3: %d CPU cores, WiFi%s%s",
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "/WiFi" : "");

    /* 初始化外设 */
    lcd_touch_init();
    spi_init();
    lcd_init();

    /* 清屏白色 */
    fill_screen(lcd_panel, COLOR_WHITE);

    ESP_LOGI(TAG, "Starting animation (%d frames, %dx%d)...",
             ANIM_FRAME_COUNT, ANIM_FRAME_SIZE, ANIM_FRAME_SIZE);
    xTaskCreate(touch_task, "touch_task", 4096, (void *)touch_handle, 5, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
}
