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
            "platformio run -e esp32dev_ota -t upload"
        ],
        title="OTA Upload",
        description="Build firmware and upload over OTA using configured password"
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
        "status": "Show system status",
        "log": "Dump rolling log",
        "startup": "Dump startup log",
        "head": "Show last 20 log entries",
        "tail": "Show first 20 startup entries",
        "help": "List telnet commands"
    }

    for cmd, desc in TELNET_CMDS.items():
        cmd_args = cmd
        if cmd in ("head", "tail"):
            cmd_args = f"{cmd} 20"
        env.AddCustomTarget(
            name=f"telnet_{cmd}",
            dependencies=None,
            actions=[
                f"python scripts/telnet_command.py {cmd_args}"
            ],
            title=f"Telnet {cmd}",
            description=desc
        )

    # Telnet stream target
    env.AddCustomTarget(
        name="telnet_stream",
        dependencies=None,
        actions=[
            "python scripts/telnet_stream.py"
        ],
        title="Telnet Stream",
        description="Stream rolling log via telnet"
    )
