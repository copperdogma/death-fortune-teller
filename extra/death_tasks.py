Import("env")

if env.get("PIOENV", "") == "esp32dev":
    env.AddCustomTarget(
        name="flash_monitor_usb",
        dependencies=None,
        actions=[
            "python scripts/flash_and_monitor.py --mode usb --seconds 30"
        ],
        title="Flash + Monitor (USB)",
        description="Upload firmware over USB and capture 30s of serial output"
    )

if env.get("PIOENV", "") == "esp32dev_ota":
    # OTA composite target (build + upload)
    env.AddCustomTarget(
        name="ota_upload",
        dependencies=None,
        actions=[
            "bash -lc 'python scripts/ota_upload_auto.py ${DEATH_FORTUNE_HOST:+--host $DEATH_FORTUNE_HOST}'"
        ],
        title="OTA Upload",
        description="Auto-discover (if needed), disable Bluetooth, upload OTA, then re-enable"
    )

    env.AddCustomTarget(
        name="flash_monitor_ota",
        dependencies=None,
        actions=[
            "python scripts/flash_and_monitor.py --mode ota --capture telnet --seconds 30 --delay-after-flash 8"
        ],
        title="Flash + Monitor (OTA)",
        description="Upload firmware over OTA and capture 30s of telnet logs"
    )

    # Telnet helper targets
    TELNET_CMDS = {
        "status": ("status", "Show system status", "--read-timeout 2 --post-send-wait 1.2 --retries 3 --retry-delay 2"),
        "log": ("log", "Dump rolling log", "--read-timeout 2 --post-send-wait 2 --retries 2"),
        "startup": ("startup", "Dump startup log", "--read-timeout 2 --post-send-wait 2 --retries 2"),
        "head": ("head 20", "Show last 20 log entries", "--read-timeout 2 --post-send-wait 2 --retries 2"),
        "tail": ("tail 20", "Show first 20 startup entries", "--read-timeout 2 --post-send-wait 2 --retries 2"),
        "help": ("help", "List telnet commands", "--read-timeout 1 --post-send-wait 1"),
        "bluetooth_off": ("bluetooth off", "Disable Bluetooth (manual)", "--read-timeout 2 --post-send-wait 3 --retries 2"),
        "bluetooth_on": ("bluetooth on", "Enable Bluetooth (manual)", "--read-timeout 2 --post-send-wait 2 --retries 2"),
        "reboot": ("reboot", "Reboot the skull remotely", "--read-timeout 1 --post-send-wait 1")
    }

    for target_name, (cmd_args, desc, extra_opts) in TELNET_CMDS.items():
        env.AddCustomTarget(
            name=f"telnet_{target_name}",
            dependencies=None,
            actions=[
                f"python scripts/telnet_command.py {cmd_args} --auto-discover {extra_opts}"
            ],
            title=f"Telnet {cmd_args}",
            description=desc,
        )

    # Telnet stream target
    env.AddCustomTarget(
        name="telnet_stream",
        dependencies=None,
        actions=[
            "python scripts/telnet_stream.py --auto-discover"
        ],
        title="Telnet Stream",
        description="Stream rolling log via telnet",
    )

    # New debugging tools
    env.AddCustomTarget(
        name="discover_esp32",
        dependencies=None,
        actions=[
            "python scripts/discover_esp32.py --fast"
        ],
        title="Discover ESP32",
        description="Scan network for ESP32 devices and show status",
    )

    env.AddCustomTarget(
        name="discover_esp32_full",
        dependencies=None,
        actions=[
            "python scripts/discover_esp32.py"
        ],
        title="Discover ESP32 (Full)",
        description="Perform a full network scan for ESP32 devices",
    )

    env.AddCustomTarget(
        name="system_status",
        dependencies=None,
        actions=[
            "python scripts/system_status.py --fast"
        ],
        title="System Status",
        description="Show complete system status dashboard",
    )

    env.AddCustomTarget(
        name="troubleshoot",
        dependencies=None,
        actions=[
            "python scripts/troubleshoot.py --fast"
        ],
        title="Troubleshoot",
        description="Interactive troubleshooting guide for OTA issues",
    )
