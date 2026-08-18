#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int miso_gpio;
    int mosi_gpio;
    int sclk_gpio;
    int cs_gpio;
    int max_files;
} sdcard_config_t;

esp_err_t sdcard_module_init(const sdcard_config_t *config);

esp_err_t sdcard_module_deinit(void);

esp_err_t sdcard_module_write_file(const char *path, const void *data, size_t size);

esp_err_t sdcard_module_append_file(const char *path, const void *data, size_t size);

esp_err_t sdcard_module_read_file(const char *path, void *buffer, size_t *size);

esp_err_t sdcard_module_delete_file(const char *path);

esp_err_t sdcard_module_create_dir(const char *path);

esp_err_t sdcard_module_get_free_space(uint64_t *free_bytes, uint64_t *total_bytes);

bool sdcard_module_is_mounted(void);

esp_err_t sdcard_module_save_jpeg(const uint8_t *data, size_t size, const char *filename);

#ifdef __cplusplus
}
#endif