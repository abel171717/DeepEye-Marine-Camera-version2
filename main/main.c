#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

// DS3231 RTC
#include "esp-idf-ds3231.h"
// Camera & SD modules
#include "camera_module.h"
#include "sdcard_module.h"
#include "esp_timer.h"

#define I2C_MASTER_SDA_IO      5
#define I2C_MASTER_SCL_IO      6
#define DS3231_INT_GPIO        2   // EXT0 wake pin

#define SD_MISO_GPIO           8
#define SD_MOSI_GPIO           9
#define SD_SCLK_GPIO           7
#define SD_CS_GPIO             21
#define MAX_SD_FILES           30

static const char *TAG = "RTC_CAM";
static rtc_handle_t *rtc_handle = NULL;

/* --- Set all camera & SD pins to INPUT to avoid leakage --- */
static void set_peripheral_pins_to_input(void)
{
    // I2C
    gpio_set_direction(I2C_MASTER_SDA_IO, GPIO_MODE_INPUT);
    gpio_set_direction(I2C_MASTER_SCL_IO, GPIO_MODE_INPUT);

    // SD card SPI pins
    gpio_set_direction(SD_MISO_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(SD_MOSI_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(SD_SCLK_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(SD_CS_GPIO, GPIO_MODE_INPUT);

    // Camera pins (from your camera_module.c)
    gpio_set_direction(10, GPIO_MODE_INPUT); // XCLK
    gpio_set_direction(40, GPIO_MODE_INPUT); // SIOD
    gpio_set_direction(39, GPIO_MODE_INPUT); // SIOC
    gpio_set_direction(48, GPIO_MODE_INPUT);
    gpio_set_direction(11, GPIO_MODE_INPUT);
    gpio_set_direction(12, GPIO_MODE_INPUT);
    gpio_set_direction(14, GPIO_MODE_INPUT);
    gpio_set_direction(16, GPIO_MODE_INPUT);
    gpio_set_direction(18, GPIO_MODE_INPUT);
    gpio_set_direction(17, GPIO_MODE_INPUT);
    gpio_set_direction(15, GPIO_MODE_INPUT);
    gpio_set_direction(38, GPIO_MODE_INPUT); // VSYNC
    gpio_set_direction(47, GPIO_MODE_INPUT); // HREF
    gpio_set_direction(13, GPIO_MODE_INPUT); // PCLK
}

/* --- DS3231 init & alarm set --- */
static void ds3231_init_and_set_alarm(void)
{
    if (rtc_handle == NULL) {
        rtc_handle = ds3231_init_full(GPIO_NUM_6, GPIO_NUM_5);
        if (!rtc_handle) {
            ESP_LOGE(TAG, "Failed to initialize DS3231");
            return;
        }
    }

    struct tm *now = ds3231_time_get(rtc_handle);
    if (!now) {
        ESP_LOGE(TAG, "Failed to get time from DS3231");
        return;
    }

    ESP_LOGI(TAG, "Current RTC time: %04d-%02d-%02d %02d:%02d:%02d",
             now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
             now->tm_hour, now->tm_min, now->tm_sec);

    struct tm alarm = *now;
    alarm.tm_sec = 0;
    alarm.tm_min += 2;

    time_t alarm_time = mktime(&alarm);
    alarm = *localtime(&alarm_time);

    ds3231_alarm1_fired_flag_reset(rtc_handle);
    ds3231_alarm2_fired_flag_reset(rtc_handle);
    ds3231_alarm1_day_of_month_set(rtc_handle,
                                   alarm.tm_mday, alarm.tm_hour, alarm.tm_min, alarm.tm_sec);
    ds3231_alarm1_enable_flag_set(rtc_handle, true);

    ESP_LOGI(TAG, "DS3231 alarm set for %02d:%02d:%02d",
             alarm.tm_hour, alarm.tm_min, alarm.tm_sec);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Woke up from DS3231 alarm");
    } else {
        ESP_LOGI(TAG, "Power on or other wakeup source");
    }

    // Set new RTC alarm
    ds3231_init_and_set_alarm();

    /* ------ REINITIALIZE PERIPHERALS AFTER WAKE ------ */
    camera_config_params_t camera_params = {
        .frame_size = FRAMESIZE_UXGA,
        .pixel_format = PIXFORMAT_JPEG,
        .jpeg_quality = 12,
        .fb_count = 2
    };

    ESP_LOGI(TAG, "Initializing camera...");
    if (camera_module_init(&camera_params) != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed");
    } else {
        vTaskDelay(pdMS_TO_TICKS(2000)); // sensor warm-up

        sdcard_config_t sd_config = {
            .miso_gpio = SD_MISO_GPIO,
            .mosi_gpio = SD_MOSI_GPIO,
            .sclk_gpio = SD_SCLK_GPIO,
            .cs_gpio = SD_CS_GPIO,
            .max_files = MAX_SD_FILES
        };

        if (sdcard_module_init(&sd_config) == ESP_OK) {
            ESP_LOGI(TAG, "SD card initialized");

            camera_fb_t *fb = camera_module_capture();
            if (fb) {
                char filename[39];
                snprintf(filename, sizeof(filename), "/sdcard/photo_%llu.jpg",
                         (unsigned long long)(esp_timer_get_time() / 1000000ULL));
                if (sdcard_module_save_jpeg(fb->buf, fb->len, filename) == ESP_OK) {
                    ESP_LOGI(TAG, "Saved photo: %s (%zu bytes)", filename, fb->len);
                } else {
                    ESP_LOGE(TAG, "Failed to save photo");
                }
                camera_module_return_fb(fb);
            } else {
                ESP_LOGE(TAG, "Camera capture failed");
            }

            sdcard_module_deinit();
        } else {
            ESP_LOGE(TAG, "SD card initialization failed");
        }

        camera_module_deinit();
    }

    /* ------ SHUT DOWN PERIPHERALS BEFORE SLEEP ------ */
    ESP_LOGI(TAG, "Disabling Wi-Fi to save power...");
    esp_wifi_stop();
    esp_wifi_deinit();

    // EXT0 wakeup on DS3231 INT (active low)
    gpio_pullup_dis(GPIO_NUM_2);
    gpio_pulldown_en(GPIO_NUM_2);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);

    // Set all peripheral pins to input high-Z
    set_peripheral_pins_to_input();

    ESP_LOGI(TAG, "Entering deep sleep...");
    vTaskDelay(pdMS_TO_TICKS(100)); // allow logs to flush
    esp_deep_sleep_start();
}
