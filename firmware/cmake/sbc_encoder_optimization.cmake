# Sampled CPU0 watchdog stacks identify the SBC encoder as the hotspot. Keep
# the rest of Bluetooth/Matter size-optimized and do not modify the SDK tree.
idf_component_get_property(_skull_bt_target bt COMPONENT_LIB)
idf_component_get_property(_skull_bt_directory bt COMPONENT_DIR)
get_target_property(_skull_bt_sources "${_skull_bt_target}" SOURCES)
set(_skull_sbc_encoder_count 0)
foreach(_skull_source IN LISTS _skull_bt_sources)
    if(_skull_source MATCHES "host/bluedroid/external/sbc/encoder/srce/[^/]+\\.c$")
        if(NOT IS_ABSOLUTE "${_skull_source}")
            set(_skull_source "${_skull_bt_directory}/${_skull_source}")
        endif()
        # Source options follow target/directory options, so this overrides
        # the project's -Os only for these eight encoder translation units.
        set_property(SOURCE "${_skull_source}" TARGET_DIRECTORY "${_skull_bt_target}"
            APPEND PROPERTY COMPILE_OPTIONS "-O2")
        math(EXPR _skull_sbc_encoder_count "${_skull_sbc_encoder_count}+1")
    endif()
endforeach()
if(NOT _skull_sbc_encoder_count EQUAL 8)
    message(FATAL_ERROR "Expected eight pinned SDK SBC encoder sources; found ${_skull_sbc_encoder_count}. Review optimization scope.")
endif()
message(STATUS "TwoSkulls: -O2 for ${_skull_sbc_encoder_count} SBC encoder sources only")
