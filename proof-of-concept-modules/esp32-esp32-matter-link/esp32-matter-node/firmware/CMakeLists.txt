# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# ESP_MATTER_PATH related lines are removed as we will use managed components.
# if(NOT DEFINED ENV{ESP_MATTER_PATH})
#     message(FATAL_ERROR "ESP_MATTER_PATH environment variable is not set. Please source esp-matter's export.sh.")
# endif()

set(PROJECT_VER "1.0")
set(PROJECT_VER_NUMBER 1)

# MATTER_SDK_PATH might still be needed by some esp-matter internals if not fully managed.
# We'll keep its definition for now, but it won't be used for component discovery here.
if(DEFINED ENV{ESP_MATTER_PATH})
    set(MATTER_SDK_PATH "$ENV{ESP_MATTER_PATH}/connectedhomeip/connectedhomeip")
else()
    # If esp-matter is purely a managed component, this path might need to point inside managed_components.
    # For now, leave it conditional. If build fails related to MATTER_SDK_PATH, we'll address it.
    message(WARNING "ESP_MATTER_PATH is not defined. MATTER_SDK_PATH might be incorrect if esp-matter scripts rely on it externally.")
endif()

# This should be done before using the IDF_TARGET variable.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# EXTRA_COMPONENT_DIRS for esp-matter removed. Managed dependencies will be handled by IDF.

project(sensors)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
# For RISCV chips, project_include.cmake sets -Wno-format, but does not clear various
# flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
