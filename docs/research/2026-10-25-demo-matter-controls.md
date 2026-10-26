# Demo Matter controls — 2026-10-25

## Problem and decision

The demo needs three Home-visible states/actions with one retained Matter fabric: Start begins the complete show, Waiting reflects idle readiness, and Dead Stop cancels. The current accessory already exposes two On/Off plug-in endpoints in Home. Retain that supported device type rather than introducing a mode-selection cluster whose Home presentation is unverified.

Use endpoint 1 for Start, endpoint 2 for Dead Stop (the previous Near endpoint), and append endpoint 3 for Waiting. Root endpoint 0 plus these three requires `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT=4` in defaults and the generated configuration. Preserve fabric/NVS and existing endpoint numbering. Home may need to refresh its cached accessory topology; actual discovery of endpoint 3 remains a device/Home check.

Start remains On throughout the complete show and cooldown. Waiting is On when the control layer reports Waiting. Dead Stop is a momentary action that returns Off; its effect is cancellation back to Waiting. Writes to Waiting do not trigger a show. Off writes to Start do not substitute for Dead Stop. Incoming writes can be accepted and reconciled with actual state to avoid turning a harmless ignored action into a Home communication error.

## State synchronization

The current `reset_momentary` resets both outlets after 300 ms; replace it with authoritative state synchronization. Run `attribute::update(endpoint, OnOff::Id, OnOff::Attributes::OnOff::Id, &value)` only on the Matter system layer via `ScheduleLambda` or `ScheduleWork`. Retain the existing `internal_update` guard so firmware-generated updates cannot trigger a show.

Defer reconciliation after a PRE_UPDATE callback. Updating the same attribute inline can be overwritten when the incoming write commits after the callback returns. Coalesce pending updates and update only booleans that actually changed, avoiding 20 ms reporting churn.

The show state is owned by the diagnostic task. Publish an atomic snapshot for the Matter worker; do not call non-atomic `death::busy()` or `state_name()` across those task boundaries. Boot reconciliation must never interpret a hydrated/persisted Start On value as a new command.

## Endpoint labels

`on_off_plug_in_unit::config_t` has no endpoint-name field. Basic Information NodeLabel on the root labels the whole accessory. Adding Bridged Device Basic Information to ordinary non-bridged outlet endpoints solely to force names would change the device model and is not justified for this demo.

The installed ESP-Matter 1.5.0 source supports Descriptor Semantic Tags with text labels:

```cpp
chip::app::DataModel::Provider::SemanticTag tag{};
tag.mfgCode.SetNonNull(static_cast<chip::VendorId>(CONFIG_DEVICE_VENDOR_ID));
tag.namespaceID = 0;
tag.tag = code; // A distinct manufacturer-specific code per endpoint.
tag.label.SetValue(chip::app::DataModel::MakeNullable(
    chip::CharSpan::fromCharString("Start")));
ESP_ERROR_CHECK(esp_matter::cluster::descriptor::feature::tag_list::add(
    esp_matter::cluster::get(ep, chip::app::Clusters::Descriptor::Id)));
ESP_ERROR_CHECK(esp_matter::endpoint::set_semantic_tags(ep, &tag, 1));
```

`tag_list::add` returns `esp_err_t`. `set_semantic_tags` copies tag values into the endpoint, but its label string storage must remain alive; string literals satisfy that requirement. Set these before starting the server. The actual generated vendor ID is currently test VID `0xFFF1`.

A temporary `-fsyntax-only` check of this pattern using the actual application compile command passed on the native toolchain. No production source or hardware was changed by this check. This establishes SDK API applicability, not Apple Home's display policy. Automatic custom outlet labels remain unverified; manual Home naming may still be necessary.

## Sources and limits

- Installed `firmware/managed_components/espressif__esp_matter/components/esp_matter/data_model/esp_matter_endpoint.cpp`: plug-in unit cluster creation.
- Installed `esp_matter_feature.cpp`: Descriptor TagList feature and `esp_err_t` return type.
- Installed `esp_matter_data_model.h/.cpp`: semantic tag API, storage, and string-lifetime requirement.
- Installed CHIP generated `clusters/shared/Structs.h`: SemanticTagStruct field types.
- [Espressif generic switch example](https://github.com/espressif/esp-matter/blob/main/examples/generic_switch/main/app_main.cpp): official use of endpoint semantic tags.
- [Apple Matter best practices](https://developer.apple.com/apple-home/downloads/Matter-Accessory-Best-Practices-for-Apple-Home.pdf): general accessory interoperability guidance; this bounded pass found no guarantee that custom composed-endpoint text labels become Home outlet names.

No re-commissioning, fabric reset, Home UI mutation, firmware flash, or device topology test was performed by this research task. Root owns implementation and hardware qualification.
