# OpenDTU-HEMS

> [!WARNING]
> **Early development / experimental project.** OpenDTU-HEMS is currently being designed and built on top of OpenDTU-OnBattery. HEMS-specific functionality described below is a roadmap unless explicitly stated otherwise. Do not rely on it yet for unattended or safety-critical energy control.

OpenDTU-HEMS is an experimental, local-first Home Energy Management System based on [OpenDTU-OnBattery](https://github.com/hoylabs/OpenDTU-OnBattery).

The goal is to keep the proven OpenDTU-OnBattery core, including inverter communication, battery/BMS integration, power-meter support and the Dynamic Power Limiter, while adding a higher-level energy-management layer for storage, dynamic tariffs and other controllable or observable energy devices.

The project is intended to remain useful without a mandatory cloud service or Home Assistant instance. External systems such as Home Assistant, MQTT or other automation platforms may be used for visualization, history and manual control, but the core energy-management logic should be able to operate locally on the device.

## Project principles

- **Preserve the OpenDTU-OnBattery core.** Existing inverter, battery, meter and Dynamic Power Limiter functionality should remain available.
- **Local-first operation.** Device communication and control should prefer local interfaces such as Modbus TCP, local APIs or MQTT over vendor clouds.
- **Vendor-neutral abstractions.** Storage systems, EV chargers, meters and other devices should be integrated through generic capabilities instead of hard-coding the HEMS logic around one manufacturer.
- **Fail-safe behaviour.** A HEMS failure must not leave important devices in an unreasonable or unusable state. Read-only integrations and device-side watchdogs should be preferred where active control is not necessary.
- **One coordinator per control path.** Multiple independent systems should not fight over the same actuator.
- **Observable decisions.** The HEMS should expose why it made a decision and support a shadow/dry-run mode before real control is enabled.

## Planned HEMS functionality

The following features are planned. Their exact scope and implementation may change while the architecture is being developed.

### Energy optimizer

A slower strategic control layer above the existing Dynamic Power Limiter is planned. While the DPL continues to handle fast power balancing, the optimizer should decide higher-level policies such as:

- when storage may charge or discharge;
- reserve and target state-of-charge levels;
- whether grid charging is economically sensible;
- how future electricity prices influence current battery use;
- how expected PV generation should affect charging decisions;
- how large controllable or observable loads should influence storage behaviour.

### Dynamic electricity prices

Planned tariff support includes:

- day-ahead and similar time-variable tariffs;
- current and future price windows;
- configurable price-spread and profitability calculations;
- charging decisions that account for storage efficiency and available capacity;
- later optimization across multiple tariff time slots instead of reacting only to a single price threshold.

### Generic energy-storage integration

Storage systems should be represented through a generic interface exposing values and capabilities such as:

- state of charge;
- current charge/discharge power;
- available charge/discharge limits;
- charge/discharge permissions;
- data age and communication state;
- optional target-power or power-limit control.

Local protocols such as **Modbus TCP** are preferred where suitable. A **Marstek Venus** system is one example target for development, but the architecture is intended to support other AC- or DC-coupled storage systems as well.

### EV charger integration

EV chargers should be usable as observable loads and, where appropriate, optionally as controllable loads.

Possible telemetry includes:

- vehicle connected / disconnected;
- charging state;
- charging power and current;
- session energy;
- phase information;
- faults and communication state.

A **Tinkerforge WARP** is one example of a charger that can provide useful local telemetry. Generic Modbus-capable EVSEs and other local interfaces are also potential targets.

Active EVSE control is deliberately optional. A system may use charger status only, for example to prevent a home battery from discharging into an actively charging vehicle, while leaving the wallbox itself fully autonomous.

### Forecasts and policy inputs

Future providers may supply additional information to the optimizer, for example:

- PV generation forecasts;
- household load forecasts;
- configurable departure times or energy requirements for EVs;
- feed-in tariffs;
- minimum reserve levels;
- user-defined operating modes and priorities.

### Shadow mode and diagnostics

Before an optimizer is allowed to control real hardware, it should be possible to run it in a non-invasive shadow mode. In this mode the firmware records decisions such as:

```text
WOULD_CHARGE_STORAGE
WOULD_BLOCK_DISCHARGE
WOULD_ALLOW_EV_CHARGING
```

without sending commands to the device.

The UI and telemetry should make the relevant inputs and resulting decision visible so behaviour can be validated before control is enabled.

## Architecture direction

The intended separation is roughly:

```text
             Tariffs / Forecasts
                     |
                     v
            +------------------+
            |  Energy Optimizer|
            | Policies / Limits|
            +--------+---------+
                     |
                     v
            +------------------+
            | Dynamic Power    |
            | Limiter / Control|
            +--------+---------+
                     |
       +-------------+-------------+
       |             |             |
       v             v             v
   Inverters      Storage       Other devices
   / PV            systems       / EVSEs
```

The exact implementation is intentionally still evolving. The important distinction is that fast physical power regulation and slower economic optimization remain separate responsibilities.

## Development status

OpenDTU-HEMS is currently an **early-development fork**. The `development` branch is intended to stay close to the upstream OpenDTU-OnBattery development line, while HEMS-specific work is developed separately.

At this stage, assume that the HEMS-specific features above are **not production-ready** unless documented otherwise.

---

# OpenDTU-OnBattery base project

The sections below retain the relevant OpenDTU-OnBattery information because OpenDTU-HEMS builds directly on that project and keeps its core functionality.

[![OpenDTU-OnBattery Build](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/build.yml/badge.svg)](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/build.yml)
[![cpplint](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/cpplint.yml/badge.svg)](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/yarnlint.yml)
[![Yarn Prettier](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/yarnprettier.yml/badge.svg)](https://github.com/hoylabs/OpenDTU-OnBattery/actions/workflows/yarnprettier.yml)
![GitHub tag (latest SemVer)](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/AndreasBoehm/856dda48c1cadac6ea495213340c612b/raw/openDTUcoreRelease.json)

OpenDTU-OnBattery is a fork of [OpenDTU](https://github.com/tbnobody/OpenDTU),
which adds support for battery chargers, battery management systems (BMS), and
power meters on a single ESP32. Its Dynamic Power Limiter can adjust the
power production of one or more solar- and/or battery-powered inverter(s) to
the actual household consumption. In this way, it is possible to implement a
zero export policy.

## Inverter Firmware Compatibility

⚠️ **WARNING: Avoid firmware version 2.0.4** ⚠️

| Version | PDL\*) | Temporary Limit | Persistent Limit | Recommendation                  |
|:--------|:------:|:---------------:|:----------------:|---------------------------------|
| 1.0.x   | ❌     | ✅              | ✅               | Good Option if PDL not required |
| 1.1.12  | ✅     | ✅              | ✅               | Best Option                     |
| 2.0.4   | ❌     | ❌              | ✅               | Avoid/Downgrade                 |

\*) PDL = Power Distribution Logic, i.e., the inverter's ability to limit the
inputs individually to achieve the desired AC output power.

**Key Issues:**
- **Version 2.0.4**: Inverter reports 100% power limit after 4 minutes without
  limit updates, causing the DPL to stop working
  ([#1901](https://github.com/hoylabs/OpenDTU-OnBattery/issues/1901)).

**Recommendation**: Avoid version 2.0.4 completely.

## ⚠️ About Hardware For Sale

### Do It Yourself

You are always welcome to select and use your own set of parts to run
OpenDTU-OnBattery. The upstream [documentation](https://opendtu-onbattery.net/hardware/)
should give you the information you need. There are also [case
designs](https://opendtu-onbattery.net/3rd_party/cases/) available for you to
print yourself.

### Ready-To-Use

If you are interested in ready-to-use hardware available for sale, the
OpenDTU-OnBattery project endorses the **[OpenDTU Fusion
board](https://opendtu-onbattery.net/3rd_party/opendtu_fusion/)**.

### Distribution Channels

OpenDTU Fusion boards and accessories (cases in particular) are available
exclusively through [AllianceApps](https://shop.allianceapps.io/).

Please note that the OpenDTU-OnBattery project does not have any connections to hardware sold on platforms
like eBay, Kleinanzeigen, or similar marketplaces. Items sold there may not
meet the quality and compatibility standards expected by the upstream community.

### Why Purchase Through the Endorsed Channels?

Buying from the trusted sources listed above ensures:

1. Access to reliable, well-tested hardware that meets the project's requirements.
2. Support for suppliers who align with the values of the OpenDTU community.

### 🚨 Misleading Claims

The OpenDTU-OnBattery maintainers have noted that some sellers falsely advertise that purchasing their hardware
supports the OpenDTU-OnBattery, OpenDTU, and/or AhoyDTU developers financially.
The upstream OpenDTU-OnBattery project does **not** receive any financial benefit from the sale of hardware.

## Getting Started

Until HEMS-specific installation and configuration documentation exists, use the OpenDTU-OnBattery documentation to learn [what hardware](https://opendtu-onbattery.net/hardware/)
to acquire, how to [initialize](https://opendtu-onbattery.net/firmware/) it
with OpenDTU-OnBattery firmware, and how to
[configure](https://opendtu-onbattery.net/firmware/device_profiles/)
the base functionality for your hardware.

## Important Differences inherited from OpenDTU-OnBattery

Generally speaking, OpenDTU-OnBattery and the OpenDTU upstream project are compatible
with each other, because OpenDTU-OnBattery mostly only extends the upstream
project. However, there are a few notable differences aside from the added functionality:

* OpenDTU-OnBattery, due to its code footprint, cannot offer support for
  over-the-air (OTA) updates on ESP32 with only 4MB of flash memory. Consult
  the [documentation](https://opendtu-onbattery.net/firmware/howto/upgrade_8mb/#background)
  to learn more.
* Unlike in the OpenDTU upstream project, you **must** compile the web application
  yourself when attempting to build your own firmware blob. See the
  [documentation](https://opendtu-onbattery.net/firmware/compile_webapp/) for
  details.

## Upstream documentation

The canonical documentation of OpenDTU-OnBattery is hosted at
[https://opendtu-onbattery.net](https://opendtu-onbattery.net).

Additional helpful information is available in the upstream community-maintained [GitHub Wiki](https://github.com/hoylabs/OpenDTU-OnBattery/wiki).

For changes to the base project, see the upstream [releases](https://github.com/hoylabs/OpenDTU-OnBattery/releases).

## Project History

The original OpenDTU project was started from [a discussion on
Mikrocontroller.net](https://www.mikrocontroller.net/topic/525778). The
original ambition was to replace the original Hoymiles DTU (Telemetry Gateway)
to avoid using Hoymiles' cloud. With a lot of reverse engineering, the Hoymiles
protocol was decrypted and analyzed.

In the summer of 2022 [@helgeerbe](https://github.com/helgeerbe) bought a
Victron MPPT charge controller, and did not like the idea of setting up a separate
ESP32 to receive the charger's data. He decided to fork OpenDTU and extend it
with battery charger support and a Dynamic Power Limiter.

In early October 2024, the project moved to the newly founded GitHub
organisation `hoylabs` and has since been maintained by multiple community members.

## Acknowledgments

OpenDTU-HEMS would not exist without the projects it builds upon.

* Special thanks to Thomas Basler ([@tbnobody](https://github.com/tbnobody)),
  the author of [OpenDTU](https://github.com/tbnobody/OpenDTU), for his continued effort.
* Thanks to [@helgeerbe](https://github.com/helgeerbe) for starting OpenDTU-OnBattery.
* Thanks to the `hoylabs` maintainers and all OpenDTU-OnBattery contributors for building and maintaining the project that forms the foundation of OpenDTU-HEMS.

OpenDTU-HEMS remains licensed under the terms inherited from its upstream project.
