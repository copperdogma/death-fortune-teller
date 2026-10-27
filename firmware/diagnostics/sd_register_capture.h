#pragma once
#include <stdbool.h>
#include "driver/sdmmc_types.h"

// Diagnostic hooks only. No controller writes, allocation, or ISR logging.
void death_sd_capture_begin(int slot, const sdmmc_command_t *cmd);
// These bracket the existing single command-register write. No early return
// is permitted between them; they keep ISR attribution coherent at issue.
void death_sd_capture_arm(int slot, int opcode, bool data_expected);
void death_sd_capture_started(void);
void death_sd_capture_isr(void);
void death_sd_capture_end(esp_err_t result, const sdmmc_command_t *cmd);
