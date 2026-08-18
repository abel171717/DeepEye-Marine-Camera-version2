idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES camera_module
             sdcard_module 
             nvs_flash
             fatfs
             driver
             esp_timer
             esp_psram
             spi_flash
             esp_wifi
            #  esp_sleep
            #  esp_adc
            )
