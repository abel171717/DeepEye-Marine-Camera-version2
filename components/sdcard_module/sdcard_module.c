#include "sdcard_module.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "sdcard_module";

#define MOUNT_POINT "/sdcard"

static sdmmc_card_t *card = NULL;
static bool sdcard_mounted = false;
static sdcard_config_t current_config;

esp_err_t sdcard_module_init(const sdcard_config_t *config)
{
    if (sdcard_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_err_t ret;

    memcpy(&current_config, config, sizeof(sdcard_config_t));

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = config->max_files,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Initializing SD card using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config->mosi_gpio,
        .miso_io_num = config->miso_gpio,
        .sclk_io_num = config->sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = config->cs_gpio;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the format_if_mount_failed option");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        spi_bus_free(host.slot);
        return ret;
    }

    sdcard_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at %s", MOUNT_POINT);

    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t sdcard_module_deinit(void)
{
    if (!sdcard_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem (%s)", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_free(host.slot);

    sdcard_mounted = false;
    card = NULL;
    ESP_LOGI(TAG, "SD card unmounted");

    return ESP_OK;
}

esp_err_t sdcard_module_write_file(const char *path, const void *data, size_t size)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, path);

    ESP_LOGI(TAG, "Writing file %s", filepath);
    FILE *f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "File write failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File written successfully");
    return ESP_OK;
}

esp_err_t sdcard_module_append_file(const char *path, const void *data, size_t size)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, path);

    FILE *f = fopen(filepath, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending");
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "File append failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t sdcard_module_read_file(const char *path, void *buffer, size_t *size)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, path);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    size_t read_size = fread(buffer, 1, *size, f);
    fclose(f);

    *size = read_size;
    return ESP_OK;
}

esp_err_t sdcard_module_delete_file(const char *path)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, path);

    if (unlink(filepath) != 0) {
        ESP_LOGE(TAG, "Failed to delete file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File deleted successfully");
    return ESP_OK;
}

esp_err_t sdcard_module_create_dir(const char *path)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char dirpath[256];
    snprintf(dirpath, sizeof(dirpath), "%s/%s", MOUNT_POINT, path);

    if (mkdir(dirpath, 0777) != 0) {
        ESP_LOGE(TAG, "Failed to create directory");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Directory created successfully");
    return ESP_OK;
}

esp_err_t sdcard_module_get_free_space(uint64_t *free_bytes, uint64_t *total_bytes)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    if (f_getfree("0:", &fre_clust, &fs) != 0) {
        ESP_LOGE(TAG, "Failed to get free space");
        return ESP_FAIL;
    }

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    *total_bytes = tot_sect * 512;
    *free_bytes = fre_sect * 512;

    return ESP_OK;
}

bool sdcard_module_is_mounted(void)
{
    return sdcard_mounted;
}

esp_err_t sdcard_module_save_jpeg(const uint8_t *data, size_t size, const char *filename)
{
    if (!sdcard_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    ESP_LOGI(TAG, "Saving JPEG to %s", filepath);

    FILE *f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "Failed to write JPEG data");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "JPEG saved successfully: %zu bytes", size);
    return ESP_OK;
}