# 8100-00 — LifeLine Wireless Link Firmware

Firmware for the **LifeLine Wireless Link** (WABS — Wireless Alert Buffer System), the RF bridge board used in the LifeLine personal emergency response system. Each board connects wireless transmitters (CARTs, CPAC receivers, and related devices) to the LifeLine controller over RS-485.

| | |
|---|---|
| **Part number** | 8100-00 |
| **Firmware version** | 6.20 (`VERSION_MAJOR` / `VERSION_MINOR` in `wabs.h`) |
| **MCU** | Texas Instruments MSP430F1611 |
| **RF module** | MaxStream / Digi XT09 (9600 baud serial) |
| **Toolchain** | IAR Embedded Workbench for MSP430 |
| **Output image** | `trunk/SW-057106801.hex` |

## Overview

The Wireless Link operates in one of two roles, selected at power-up by DIP switch **SW1:1**:

| Role | RS-485 (Serial B) | Radio (Serial A) |
|------|-------------------|------------------|
| **Host** | Receives alerts and commands from the LifeLine system controller | Forwards traffic to/from remote Wireless Link units |
| **Remote** | Polls and communicates with field devices (CARTs, CPAC receivers) | Relays alerts and keepalives back to the host |

A single firmware image supports both roles. The main loop branches on `host_unit`, which is set from `CONFIG_HOST_MODE()` at startup.

### Key features

- Alert buffering, duplicate filtering, and repeater support
- MaxStream radio initialization, watchdog, and RSSI monitoring (A/D)
- RS-485 state machines for host controller traffic and remote device polling
- CPAC (Care Partner Alert Communicator) protocol support (`cpac.c`)
- Flash-backed storage for radio configuration parameters
- 3-digit LED status display and diagnostic modes via spare DIP switches

## Repository layout

```
8100-00/
└── trunk/
    ├── wabs.eww          # IAR workspace (open this to build)
    ├── wabs.ewp          # IAR project
    ├── wabs.c / wabs.h   # Main application and protocol logic
    ├── cpu.c / cpu.h     # MSP430 hardware abstraction (GPIO, ADC, LEDs, DIP switches)
    ├── serialA.c         # UART driver — MaxStream RF module
    ├── serialB.c         # UART driver — RS-485 bus
    ├── timers.c          # Low-resolution software timers
    ├── flash.c           # Non-volatile storage for radio parameters
    ├── cpac.c / cpac.h   # CPAC message handling
    └── SW-057106801.hex  # Pre-built firmware image
```

Legacy project files (`wabs_II.pjt`, `WL.Opt`) from earlier toolchains are retained for reference but are not the active build system.

## Building

1. Install **IAR Embedded Workbench for MSP430** (project was last built with IAR C/C++ Compiler V7.10.x; newer IAR versions may require project migration).
2. Open `trunk/wabs.eww` in IAR.
3. Confirm the target device is **MSP430F1611** (Project → Options → General Options → Device).
4. Build the **Debug** configuration (Project → Make).
5. Output files are written to `trunk/Debug/Exe/` (`.d43` debug image) and `trunk/Debug/Obj/`.

To produce a release HEX file for programming, use the IAR output converter or your preferred MSP430 programming tool to export from the built image.

## Programming

Program the target MSP430F1611 using the generated HEX/DFW image and your standard LifeLine programming fixture or MSP430 programmer. A known-good image is included at `trunk/SW-057106801.hex`.

On power-up, the 3-digit LED display shows the firmware version for two seconds (e.g. **620** for v6.20), followed by boot progress codes (**C1**, **C2**, **C3**) during initialization.

## Hardware configuration (DIP switches)

Configuration is read from DIP switches at startup. Changing any switch after boot triggers a watchdog reset once debounced.

### SW1 — Configuration (`cpu_readCfgDIP`)

| Bit | Function |
|-----|----------|
| 1 | **Host mode** — ON = host (connected to LifeLine controller); OFF = remote |
| 2 | **Duplicate alert filter** — OFF = enabled; ON = disabled |
| 3 | **Supervisory filter** — smoke detector status message filtering |
| 4 | **Repeater** — ON = repeater disabled |

### SW2 — Network ID (`cpu_readNetDIP`)

Lower 4 bits set the MaxStream network / PAN ID used during radio initialization.

### SW3 — Spare / Diagnostics (`cpu_readSpareDIP`)

| Bit | Diagnostic mode |
|-----|-------------------|
| 1 | Radio quality display |
| 2 | Radio RSSI display |
| 3 | Radio relay display |
| 4 | Buffer utilization display |

A **rotary address switch** sets the RS-485 / radio address for the unit (read by `read_address_switch()`).

## Conditional compile flags

Diagnostic and test behavior is controlled by `#define` flags in `wabs.h` and `cpu.h`. Release builds should use the documented release settings:

| Flag | Release | Purpose |
|------|---------|---------|
| `CPU_SERIAL_CONSOLE` | `0` | `1` = use RF serial channel as diagnostic console |
| `RADIO_WATCHDOG_ENABLE` | `1` | Radio initialization watchdog |
| `RADIO_WATCHDOG_REDUCE` | `0` | Shortened watchdog timeouts for bench testing |
| `STATE_TRACE_ENABLE` | `0` | Remote/init state tracing |
| `SMOKE_SUPERVISOR_REDUCE` | `0` | Shortened smoke supervisor filter times for testing |
| `VERSION_TEST` | `1` | Beta test indicator on LED decimal point |

## Architecture notes

```
  LifeLine Controller                Remote Wireless Link(s)
        │                                    │
        │ RS-485                             │ RS-485
        ▼                                    ▼
  ┌─────────────┐    MaxStream RF     ┌─────────────┐
  │ Host W.L.   │◄──────────────────►│ Remote W.L.  │
  │ (8100-00)   │                     │ (8100-00)   │
  └─────────────┘                     └──────┬──────┘
                                             │ RS-485
                                             ▼
                                      CARTs / CPAC receivers
```

- **serialA** — AT-command interface to the XT09 radio module (alert relay, keepalives, radio init).
- **serialB** — RS-485 transceiver; parity differs between host (odd) and remote (even/none for CPAC compatibility).
- **State machines** in `wabs.c` drive host RS-485 polling, remote RS-485 polling, radio TX/RX, LED display, and activity aging.

## History

Original development by Venture Technologies, Inc. (Tom Goltz, Bob Halliday) for LifeLine, 2005–2007. The codebase traces back to the WABS2 (Gen 2) hardware platform, ported from an earlier PIC-based WABS1 design to the MSP430.
