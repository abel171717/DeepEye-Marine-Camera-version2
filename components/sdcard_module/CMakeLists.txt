idf_component_register(
    SRCS "sdcard_module.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs sdmmc
    PRIV_REQUIRES log driver
)