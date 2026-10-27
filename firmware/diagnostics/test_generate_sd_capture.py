#!/usr/bin/env python3
"""Read-only SDK checks; generated/mutated fixtures live in temporary folders."""
import os
from pathlib import Path
import tempfile
import unittest

from generate_sd_capture import PINNED, generate, replace_once


class GeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.idf = Path(os.environ["IDF_PATH"])

    def test_only_intended_hooks_added_and_private_header_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "out"
            generate(self.idf, output)
            originals = self.idf / "components/esp_driver_sdmmc/src"
            host = (output / "sdmmc_host.c").read_text()
            transaction = (output / "sdmmc_transaction.c").read_text()
            self.assertLess(host.index("    death_sd_capture_isr();"),
                            host.index("    uint32_t pending = sdmmc_ll_get_intr_status"))
            self.assertIn("    death_sd_capture_arm(slot, cmd.cmd_index, cmd.data_expected);\n"
                          "    sdmmc_ll_set_command(s_host_ctx.hal.dev, cmd);\n"
                          "    death_sd_capture_started();", host)
            host_hooks = ['#include "sd_register_capture.h"\n',
                          "    death_sd_capture_isr();\n",
                          "    death_sd_capture_arm(slot, cmd.cmd_index, cmd.data_expected);\n",
                          "    death_sd_capture_started();\n"]
            tx_hooks = ['#include "sd_register_capture.h"\n',
                        "    death_sd_capture_begin(slot, cmdinfo);\n",
                        "    death_sd_capture_end(ret, cmdinfo);\n"]
            for hook in host_hooks:
                self.assertEqual(host.count(hook), 1)
                host = host.replace(hook, "")
            for hook in tx_hooks:
                self.assertEqual(transaction.count(hook), 1)
                transaction = transaction.replace(hook, "")
            self.assertEqual(host, (originals / "sdmmc_host.c").read_text())
            self.assertEqual(transaction, (originals / "sdmmc_transaction.c").read_text())
            self.assertEqual((output / "sdmmc_internal.h").read_bytes(),
                             (originals / "sdmmc_internal.h").read_bytes())

    def test_every_changed_pinned_input_refuses_before_output(self):
        for changed in PINNED:
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                for relative in PINNED:
                    dest = root / "idf" / relative
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes((self.idf / relative).read_bytes())
                with (root / "idf" / changed).open("ab") as stream:
                    stream.write(b"\n/* changed upstream */\n")
                with self.assertRaisesRegex(ValueError, "input changed"):
                    generate(root / "idf", root / "out")
                self.assertFalse((root / "out").exists())

    def test_repeat_configure_preserves_generated_timestamps(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            generate(self.idf, output)
            before = {p.name: p.stat().st_mtime_ns for p in output.iterdir()}
            generate(self.idf, output)
            self.assertEqual(before, {p.name: p.stat().st_mtime_ns for p in output.iterdir()})

    def test_missing_or_ambiguous_anchor_refuses(self):
        for text in ("absent", "anchor anchor"):
            with self.assertRaisesRegex(ValueError, "exactly one anchor"):
                replace_once(text, "anchor", "hook")


if __name__ == "__main__":
    unittest.main()
