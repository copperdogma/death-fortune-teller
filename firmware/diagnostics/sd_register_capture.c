#include "sd_register_capture.h"
#include <inttypes.h>
#include <stdbool.h>
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "soc/sdmmc_periph.h"
#include "hal/sdmmc_ll.h"

typedef struct {
    uint32_t raw, masked, dma, status, card_bytes, bus_bytes;
} death_sd_registers_t;

typedef struct {
    uint32_t sequence, arg, length, block_length;
    int slot, opcode;
    death_sd_registers_t start, fault;
    bool prepared, active, captured;
} death_sd_capture_t;

// The driver serializes transactions, but the task and ISR may run on different
// cores. This short spinlock protects publication, first-error capture, and
// teardown. Never log or call another driver while holding it.
static DRAM_ATTR portMUX_TYPE capture_lock = portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR death_sd_capture_t capture;
static bool good_printed;

static death_sd_registers_t read_registers(void)
{
    // Sequential register reads, not an atomic hardware snapshot. No register
    // is cleared. In particular FRUN can remain set from an earlier command.
    death_sd_registers_t value = {
        .raw = SDMMC.rintsts.val,
        .masked = SDMMC.mintsts.val,
        .dma = SDMMC.idsts.val,
        .status = SDMMC.status.val,
        .card_bytes = SDMMC.tcbcnt,
        .bus_bytes = SDMMC.tbbcnt,
    };
    return value;
}

void death_sd_capture_begin(int slot, const sdmmc_command_t *cmd)
{
    portENTER_CRITICAL(&capture_lock);
    capture.active = false;
    capture.captured = false;
    ++capture.sequence;
    capture.slot = slot;
    capture.opcode = cmd->opcode;
    capture.arg = cmd->arg;
    capture.length = cmd->datalen;
    capture.block_length = cmd->blklen;
    capture.prepared = cmd->data != NULL;
    portEXIT_CRITICAL(&capture_lock);
}

void death_sd_capture_arm(int slot, int opcode, bool data_expected)
{
    portENTER_CRITICAL(&capture_lock);
    capture.active = capture.prepared && data_expected &&
        capture.slot == slot && capture.opcode == opcode;
    if (capture.active) capture.start = read_registers();
    // The caller performs the original command register write while this
    // lock is held. ISR cannot capture partially published command metadata.
}

void death_sd_capture_started(void)
{
    portEXIT_CRITICAL(&capture_lock);
}

void death_sd_capture_isr(void)
{
    portENTER_CRITICAL_ISR(&capture_lock);
    // Match the existing driver's data-error classes, not a possibly stale
    // raw FRUN bit. The hook runs before the stock ISR clears either status.
    const uint32_t errors = SDMMC_LL_EVENT_DCRC | SDMMC_LL_EVENT_DTO |
        SDMMC_LL_EVENT_HTO | SDMMC_LL_EVENT_SBE | SDMMC_LL_EVENT_EBE;
    if (capture.active && !capture.captured && (SDMMC.mintsts.val & errors)) {
        capture.fault = read_registers();
        capture.captured = true;
    }
    portEXIT_CRITICAL_ISR(&capture_lock);
}

void death_sd_capture_end(esp_err_t result, const sdmmc_command_t *cmd)
{
    death_sd_capture_t frozen;
    bool print, good = false;
    portENTER_CRITICAL(&capture_lock);
    print = capture.active && capture.captured;
    // One successful sector-read comparison per boot. Response-only probing
    // (including expected CMD5 timeouts) cannot consume it. Completion values
    // are task-time observations after the stock ISR has cleared its flags.
    if (capture.active && !capture.captured && !good_printed &&
        (capture.opcode == 17 || capture.opcode == 18) &&
        result == ESP_OK && cmd->error == ESP_OK) {
        capture.fault = read_registers();
        print = good = good_printed = true;
    }
    if (print) frozen = capture;
    capture.prepared = false;
    capture.active = false;
    capture.captured = false;
    portEXIT_CRITICAL(&capture_lock);
    if (!print) return;

    // Called from the transaction task, after transfer processing. Keep each
    // line short enough for the existing fault logger; sequence joins them.
    ESP_LOGW("sdmmc_req", "SDCAP %s seq=%"PRIu32" slot=%d cmd=%d arg=%08"PRIx32
             " len=%"PRIu32" blk=%"PRIu32" ret=%x err=%x",
             good ? "good-task" : "fault-isr", frozen.sequence, frozen.slot, frozen.opcode, frozen.arg,
             frozen.length, frozen.block_length, (unsigned)result, (unsigned)cmd->error);
    const death_sd_registers_t *r = &frozen.start;
    ESP_LOGW("sdmmc_req", "SDCAP start seq=%"PRIu32" raw=%08"PRIx32" mask=%08"PRIx32
             " dma=%08"PRIx32" status=%08"PRIx32" card=%"PRIu32" bus=%"PRIu32,
             frozen.sequence, r->raw, r->masked, r->dma, r->status, r->card_bytes, r->bus_bytes);
    r = &frozen.fault;
    ESP_LOGW("sdmmc_req", "SDCAP %s seq=%"PRIu32" raw=%08"PRIx32" mask=%08"PRIx32
             " dma=%08"PRIx32" status=%08"PRIx32" card=%"PRIu32" bus=%"PRIu32,
             good ? "good-task" : "fault-isr", frozen.sequence, r->raw, r->masked, r->dma, r->status, r->card_bytes, r->bus_bytes);
}
