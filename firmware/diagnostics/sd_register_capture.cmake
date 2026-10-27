# Included only when the diagnostic is explicitly enabled. Never edit IDF.
idf_build_get_property(_death_capture_target IDF_TARGET)
if(NOT _death_capture_target STREQUAL "esp32")
    message(FATAL_ERROR "DEATH_SD_REGISTER_CAPTURE is pinned to classic ESP32")
endif()
if(DEATH_SD_SPI_TRANSPORT)
    message(FATAL_ERROR "DEATH_SD_REGISTER_CAPTURE requires native SDMMC")
endif()
idf_build_get_property(_death_capture_idf IDF_PATH)
idf_build_get_property(_death_capture_python PYTHON)
set(_death_capture_dir "${CMAKE_CURRENT_LIST_DIR}")
set(_death_capture_output "${CMAKE_CURRENT_BINARY_DIR}/death_sd_register_capture")
execute_process(
    COMMAND "${_death_capture_python}" "${_death_capture_dir}/generate_sd_capture.py"
            --idf "${_death_capture_idf}" --output "${_death_capture_output}"
    RESULT_VARIABLE _death_capture_result
    ERROR_VARIABLE _death_capture_error)
if(NOT _death_capture_result EQUAL 0)
    message(FATAL_ERROR "SD register capture: ${_death_capture_error}")
endif()
# Re-run configure if the generator, template, or any pinned input changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_death_capture_dir}/generate_sd_capture.py"
    "${_death_capture_idf}/components/esp_driver_sdmmc/src/sdmmc_host.c"
    "${_death_capture_idf}/components/esp_driver_sdmmc/src/sdmmc_transaction.c"
    "${_death_capture_idf}/components/esp_driver_sdmmc/src/sdmmc_internal.h"
    "${_death_capture_idf}/components/hal/esp32/include/hal/sdmmc_ll.h"
    "${_death_capture_idf}/components/soc/esp32/register/soc/sdmmc_struct.h")
idf_component_get_property(_death_capture_driver esp_driver_sdmmc COMPONENT_LIB)
idf_component_get_property(_death_capture_component esp_driver_sdmmc COMPONENT_DIR)
get_target_property(_death_capture_sources "${_death_capture_driver}" SOURCES)
set(_death_capture_replaced 0)
set(_death_capture_new_sources)
foreach(_death_capture_source IN LISTS _death_capture_sources)
    get_filename_component(_death_capture_absolute "${_death_capture_source}" ABSOLUTE
        BASE_DIR "${_death_capture_component}")
    if(_death_capture_absolute STREQUAL "${_death_capture_component}/src/sdmmc_host.c" OR
       _death_capture_absolute STREQUAL "${_death_capture_component}/src/sdmmc_transaction.c")
        get_filename_component(_death_capture_name "${_death_capture_absolute}" NAME)
        list(APPEND _death_capture_new_sources "${_death_capture_output}/${_death_capture_name}")
        math(EXPR _death_capture_replaced "${_death_capture_replaced} + 1")
    else()
        list(APPEND _death_capture_new_sources "${_death_capture_source}")
    endif()
endforeach()
if(NOT _death_capture_replaced EQUAL 2)
    message(FATAL_ERROR "Expected two pinned SDMMC driver sources, found ${_death_capture_replaced}")
endif()
list(APPEND _death_capture_new_sources "${_death_capture_dir}/sd_register_capture.c")
set_property(TARGET "${_death_capture_driver}" PROPERTY SOURCES "${_death_capture_new_sources}")
target_include_directories("${_death_capture_driver}" PRIVATE "${_death_capture_dir}")
message(STATUS "Death: first data-error register capture enabled (pinned IDF copies)")
