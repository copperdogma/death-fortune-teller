idf_component_register(
    SRCS "app_main.cpp" "death_fault_log.cpp" "death_fault_archive.cpp" "skull_radio.cpp" "skull_storage.cpp" "skull_wave.cpp" "skull_startup.cpp" "skull_actuators.cpp" "skull_pwm_probe.cpp" "skull_catalog.cpp" "skull_audio.cpp" "death_show.cpp" "death_fortune.cpp" "death_printer.cpp" "skull_config.cpp" "skull_secondary.cpp"
    INCLUDE_DIRS "."
    REQUIRES espressif__esp_matter nvs_flash bt esp_timer esp_psram fatfs sdmmc driver esp_driver_spi esp_driver_sdspi esp_driver_uart esp_driver_ledc esp_driver_mcpwm esp_hw_support json
)
option(DEATH_SD_SPI_TRANSPORT "Use SDSPI on the onboard SD socket for a transport comparison" OFF)
if(DEATH_SD_SPI_TRANSPORT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DEATH_SD_SPI_TRANSPORT=1)
endif()
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
# Error-only transaction status distinguishes response CRC from data CRC.
# Compile SDMMC debug records locally; the runtime tag remains narrowly scoped.
idf_component_get_property(death_sdmmc_driver esp_driver_sdmmc COMPONENT_LIB)
target_compile_definitions(${death_sdmmc_driver} PRIVATE LOG_LOCAL_LEVEL=4)
option(DEATH_SD_REGISTER_CAPTURE "Capture native SDMMC registers at first data error" OFF)
if(DEATH_SD_REGISTER_CAPTURE)
    include("${CMAKE_CURRENT_LIST_DIR}/../diagnostics/sd_register_capture.cmake")
endif()
