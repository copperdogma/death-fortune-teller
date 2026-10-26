idf_component_register(
    SRCS "app_main.cpp" "skull_radio.cpp" "skull_storage.cpp" "skull_wave.cpp" "skull_startup.cpp" "skull_actuators.cpp" "skull_catalog.cpp" "skull_audio.cpp" "death_show.cpp" "death_fortune.cpp" "death_printer.cpp" "skull_config.cpp" "skull_secondary.cpp"
    INCLUDE_DIRS "."
    REQUIRES espressif__esp_matter nvs_flash bt esp_timer esp_psram fatfs sdmmc driver esp_driver_uart esp_driver_ledc esp_hw_support json
)
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
