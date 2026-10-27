#!/usr/bin/env python3
"""Generate diagnostic driver copies from exact ESP-IDF 5.5.4 inputs."""
import argparse
import hashlib
from pathlib import Path

PINNED = {
    "components/esp_driver_sdmmc/src/sdmmc_host.c": "366dcc3e7f72db930632e1e49cebb4ffa8bc080f631c0aecb179cf187315c0ff",
    "components/esp_driver_sdmmc/src/sdmmc_transaction.c": "7cea7d649edbfa655ab3c3a26ba1d3d25290841091b8900f9643559bdb1bcad6",
    "components/esp_driver_sdmmc/src/sdmmc_internal.h": "ac051519173525429b6f233f8bd7877af61755104d00f9cab25c619be2bd55cb",
    "components/hal/esp32/include/hal/sdmmc_ll.h": "326dbc8ee1dc30b0a24a2480a9692161e2cec98cece8569aeb14a4a8323d9dc6",
    "components/soc/esp32/register/soc/sdmmc_struct.h": "d1834ac0290a94ac8c98a7e9aae5513ca8d74ecbc49b4bb69d8635c964351d41",
}


def replace_once(text, anchor, replacement):
    if text.count(anchor) != 1:
        raise ValueError(f"Expected exactly one anchor: {anchor!r}")
    return text.replace(anchor, replacement, 1)


def generate(idf, output):
    sources = {}
    # Validate everything before creating or replacing any output.
    for relative, expected in PINNED.items():
        raw = (idf / relative).read_bytes()
        actual = hashlib.sha256(raw).hexdigest()
        if actual != expected:
            raise ValueError(f"Pinned ESP-IDF 5.5.4 input changed: {relative}: {actual}")
        sources[Path(relative).name] = raw.decode("utf-8")

    host = replace_once(sources["sdmmc_host.c"], '#include "sdmmc_internal.h"',
                        '#include "sdmmc_internal.h"\n#include "sd_register_capture.h"')
    anchor = "    uint32_t pending = sdmmc_ll_get_intr_status(s_host_ctx.hal.dev) & SDMMC_LL_SD_EVENT_MASK;"
    host = replace_once(host, anchor, "    death_sd_capture_isr();\n" + anchor)
    anchor = "    sdmmc_ll_set_command(s_host_ctx.hal.dev, cmd);"
    host = replace_once(host, anchor,
                        "    death_sd_capture_arm(slot, cmd.cmd_index, cmd.data_expected);\n" +
                        anchor + "\n    death_sd_capture_started();")
    transaction = replace_once(sources["sdmmc_transaction.c"], '#include "sdmmc_internal.h"',
                               '#include "sdmmc_internal.h"\n#include "sd_register_capture.h"')
    anchor = "    ret = sdmmc_host_start_command(slot, hw_cmd, cmdinfo->arg);"
    transaction = replace_once(transaction, anchor, "    death_sd_capture_begin(slot, cmdinfo);\n" + anchor)
    transaction = replace_once(transaction, "out:\n", "out:\n    death_sd_capture_end(ret, cmdinfo);\n")
    generated = {"sdmmc_host.c": host, "sdmmc_transaction.c": transaction,
                 "sdmmc_internal.h": sources["sdmmc_internal.h"]}
    output.mkdir(parents=True, exist_ok=True)
    for name, text in generated.items():
        destination = output / name
        if not destination.exists() or destination.read_text() != text:
            destination.write_text(text)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        generate(args.idf, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"SD register capture generation refused: {error}\n")
