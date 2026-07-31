# BatSource — Firmware Architecture

> Lives in this repo (moved from the older `bat_Source` checkout) so it travels
> with the code it describes. If you change firmware structure, change this file
> in the same commit.

## 1. Hardware Platform

### Microcontroller
**STM32G474VET6** (Arm Cortex-M4, 170 MHz)
- Selected for its High-Resolution Timer (HRTIM, 217 ps resolution) needed for precise converter PWM
- 512 KB flash, 128 KB SRAM
- 5× ADC, 2× DAC, HRTIM, I2C, SPI, UART, QUADSPI, RTC

### Key External ICs

| IC | Role | Interface |
|----|------|-----------|
| BQ76905 (TI) | Battery Management System — 4-cell LiFePO₄ monitoring, protection, passive balancing | I2C4 (0x08) |
| ADS131M04 (TI) | 4-ch precision sigma-delta ADC for isolation current measurement | SPI3 |
| LMG2100R044 ×2 (TI) | GaN half-bridge drivers for primary buck-boost converter | HRTIM GPIO |
| 1EDN7512B | Gate driver for HV flyback primary switch | HRTIM GPIO |
| W25N01GV | 1 Gbit QSPI NAND flash — menu icon store, intended for data logging | QUADSPI |
| ZD24C02B | 2 kbit I2C EEPROM for calibration / ID data | I2C4 (0x50) |
| TCA9554 (PCA9554) | I2C I/O expander — hardware ID reading | I2C4 (0x20) |
| LP5817 / LP5816 | LED drivers — status LEDs and LCD backlight on UI board | I2C4 (0x2D / 0x2C) |
| OPT3004 | Ambient light sensor — automatic backlight control | I2C4 (0x44) |
| LCD (240×320 TFT) | Display — sunlight-readable, replaces original OLED | SPI4 (bit-banged) |

### Battery
4× Tenpower IFR26700-45HE LiFePO₄ cells in series (~14.4 V nominal, 56.96 Wh).
The BQ76905 monitors each cell, measures pack current via a 5 mΩ shunt, and controls charge/discharge MOSFETs.

---

## 2. Power Conversion Topology

### Primary Converter (LV — PS1-1BA)
- **Topology**: 4-switch synchronous buck-boost
- **Switches**: 2× LMG2100R044 GaN half-bridges (44 mΩ R_DS(on), 35 A rated)
- **Switching frequency**: 1 MHz (high-speed mode) / 50 kHz (low-speed mode)
- **Modes**: Battery charging, 60 V voltage out, 10 A current out, resistance measurement

### Secondary / HV Converter (PS1-1BA)
- **Topology**: Isolated flyback (final design)
- **Purpose**: Generate up to 500 V for insulation resistance testing
- **Primary switch**: 200 V MOSFET driven by 1EDN7512B gate driver
- **Transformer**: Custom RM5 core, 8-turn primary
- **Control**: PWM via HRTIM at 350 kHz, RCD snubber on primary switch node

### Output Stage
- Output relays K1/K2/K3 for galvanic isolation between measurement paths
- Gas discharge tubes (GDT) for transient suppression on output terminals
- Shrouded 4 mm safety jacks (Stäubli SLB4-F6.3)

---

## 3. Firmware Architecture

### Design Pattern
Bare-metal, **event-driven**, single-threaded. No RTOS.

```
┌──────────────────────────────────────────────┐
│                   main()                     │
│   peripheral init → main loop                │
│                                              │
│   while(1) {                                 │
│     cli_loop();                              │
│     process_event(event_Get());              │
│     HAL_Delay(1);                            │
│   }                                          │
└──────────────────────────────────────────────┘
         │
         ▼
┌────────────────────┐    ┌────────────────────────────┐
│   Event Queue      │    │  ADC1 DMA-complete ISR     │
│   (64 entries)     │    │  ~25 kHz (CTRL_FREQ)       │
│   event.h/.c       │    │  adc_convert_fast_data()   │
│                    │    │  ctrl_main_ctrl()          │
└────────────────────┘    └────────────────────────────┘
         │
         ▼
┌─────────────────────────────┐
│  State Machine (20 ms tick) │
│  statemachine.h/.c          │
│  Dispatches to mode modules │
└─────────────────────────────┘
```

### Event System (`event.h` / `event.c`)
- Circular queue of 64 `EVENT_STRUCT` items
- Each event carries: type (`EVENTS` enum), optional callback pointer, optional argument
- ISRs and peripheral callbacks post events; the main loop drains them
- `event_Add()` reserves, fills and publishes a slot inside one critical
  section. This is deliberate: filling then publishing lock-free is **not** safe
  here, because reading `head` and filling the slot are separate steps, and a
  producer preempted between them can clobber a slot another producer has
  already published.

| Event | Trigger |
|-------|---------|
| `EVENT_IIC_RX/TX` | I2C DMA transfer complete |
| `EVENT_IIC_ERROR` | I2C NACK / bus error |
| `EVENT_EEPROM_READ` | EEPROM read complete |
| `EVENT_EEPROM_TIMEOUT` | EEPROM no-response |
| `EVENT_SM_STEP` | State-machine timer tick (20 ms) |
| `EVENT_BMS_TIMER` | BMS polling timer (1 s) |

### State Machine (`statemachine.h` / `statemachine.c`)
Step period: **20 ms** (`STATEMACHINE_STEP_PERIOD_mS`), driven from TIM2 via the
software timer list in `timer.c`. Handles UI menu navigation, output enable
logic, and the protection interlock.

| Mode | Description |
|------|-------------|
| `STATEMACHINE_IDLE` | Startup / menu navigation |
| `STATEMACHINE_MODE_60V_OUT` | 60 V regulated voltage output (hold OUT to enable) |
| `STATEMACHINE_MODE_10A_OUT` | 10 A regulated current output (hold OUT to enable) |
| `STATEMACHINE_MODE_RESISTANCE_1A` | Milliohmmeter, 1 A pulsed excitation |
| `STATEMACHINE_MODE_RESISTANCE_1mA` | Ohmmeter, 1 mA excitation |
| `STATEMACHINE_MODE_ISOMETER` | Insulation resistance test (HV flyback active) |
| `STATEMACHINE_MODE_VOLTMETER` | Passive voltage measurement |
| `STATEMACHINE_MODE_AMPMETER` | Passive current measurement (SEK half-bridge shorted) |
| `STATEMACHINE_MODE_SETTINGS` | Settings menu |
| `STATEMACHINE_MODE_SHUTDOWN` | Controlled power-down sequence |
| `STATEMACHINE_MODE_CHARGE` | Internal battery charging — auto-entered from IDLE when a 15–24 V supply is detected, not user-selectable |

Settings sub-modes (`statemachine_settings_modes_t`): `MENU`, `BMS`,
`DISPLAY`, `CALIBRATION`, `ABOUT`.

Per-mode data (control mode, relay/shunt mask, enable GPIO, entry flags, ADC
trigger) is centralised rather than duplicated across `statemachine.c`,
`ctrl_main.c`, `adc.c` and `aux_io_ctrl.c` — see `mode_table.c`.

### Control Loop (`ctrl_main.h/c`, `ctrl_PID_control.h/c`)

Executed from the **ADC1 DMA-complete interrupt**, not directly from HRTIM.
The chain is: HRTIM timer period → HRTIM ADC trigger (with post-scaler) → ADC
conversion → DMA → `HAL_ADC_ConvCpltCallback()`, which acts only for `hadc1`
and decimates by 2 (`adc_interrupt_cnt`). With the converter at 1 MHz and an
HRTIM ADC post-scaler of ÷20, that lands at **25 kHz = `CTRL_FREQ`**.

`CTRL_FREQ` is what every integral gain and startup ramp is scaled by, so any
change to switching frequency or post-scaler changes the effective loop tuning.

**PID controller** (`ctrl_PID_control.h`): generic P/I/D with integrator-
clamping anti-windup and configurable saturation limits. The D term is compiled
out (`NO_D_PART`); all loops run as PI.

**Tunable loops** — `ctrl_pid_table[]` in `ctrl_main.c` is the single source of
truth. Each row carries the live controller, its EEPROM-backed calibration
fields, its defaults and its duty saturation limits. `ctrl_main_init()`,
`config_store_setDefaults()` and the CLI's `setPI` / `saveEEPROM` /
`readEEPROM` all iterate it, so adding a loop means adding one row.

| # | Name | Used for |
|---|------|----------|
| 0 | Buck | Buck voltage (resistance modes) |
| 1 | Boost | 60 V output |
| 2 | Current | 10 A output |
| 3 | BoostIoutLimit | Output current limiter on the boost path |
| 4 | ChargeCurrent | CC phase of charging |
| 5 | ChargeVoltage | CV tail of charging |
| 6 | HVVoltage | ISOMETER HV flyback voltage |
| 7 | HVCurrent | ISOMETER leakage-current limiter |

**Key parameters** (`ctrl_param.h`):

| Parameter | Value |
|-----------|-------|
| `CTRL_FREQ` | 25 000 Hz |
| `CTRL_PARAM_SW_FREQ_HIGH` | 1 000 000 Hz |
| `CTRL_PARAM_SW_FREQ_LOW` | 50 000 Hz |
| `CTRL_PARAM_SW_FREQ_HV` | 350 000 Hz |
| `CTRL_PARAM_CHARGE_CURRENT_mA` | 1000 mA |
| `CTRL_PARAM_CHARGE_END_VOLTAGE_mV` | 14 200 mV (3550 mV × 4) |
| `CTRL_PARAM_CHARGE_TAPER_CURRENT_mA` | 225 mA |

> **Note**: PID gains remain initial/placeholder values. No tuning has been
> performed — see Known Issues.

### Charging (CC/CV)
Auto-entered from IDLE when the terminal voltage is 15–24 V, no BMS fault is
latched, and the pack is below the end voltage minus a restart margin. Runs
constant-current until the end voltage, then latches (one-way) into a
constant-voltage tail. The cycle completes once the pack is at the end voltage
**and** current has tapered below `CTRL_PARAM_CHARGE_TAPER_CURRENT_mA`.

---

## 4. Peripheral Map

### ADC
| Instance | DMA | Role |
|----------|-----|------|
| ADC1 | DMA1_Channel2 | `I_OUT` — **carries the control loop** |
| ADC2 | DMA1_Channel3 | `V_TERM` |
| ADC3 | DMA1_Channel4 | `V_IN` |
| ADC4 | DMA1_Channel5 | `V_OUT` / `V_HV` (channel switched per mode) |
| ADC5 | DMA1_Channel1 | 12-channel scan: rails, NTCs, VBAT, VREFINT; plus an injected group for `I_ISO` / `I_BAT` |

ADC5 uses 256× hardware oversampling with 4-bit right shift (effective 16-bit result).

`adc_configure_mode()` re-points triggers and channels per operating mode.
HRTIM ADC triggers: TRG1 ← Timer B (PRIM, ÷20), TRG2 ← Timer C (HV, ÷15),
TRG3 ← Timer E (SEK, ÷20).

**Concurrency**: `adc_data.raw` is DMA-written and declared `volatile`.
`adc_data.converted` is computed in the ISR into a private instance and copied
into the globally visible one as a single snapshot under a critical section,
once per state-machine tick (`adc_snapshot_converted()`), so main-context
readers never see a torn mix of two sample instants. `ctrl_main_ctrl()` takes
`const ADC_CONVERTED_DATA *` so it cannot reach for raw or calibration fields
that the ISR-side instance does not populate.

### Interrupt Priorities (`irq_priority.h`)
`HAL_Init()` sets `NVIC_PRIORITYGROUP_4` and `__NVIC_PRIO_BITS` is 4, so all
four bits are preemption priority (range 0–15, no subpriority). Lower = more
urgent.

| Tier | Value | IRQs |
|------|-------|------|
| `IRQ_PRIO_CTRL_LOOP` | 0 | DMA1_Channel2 (ADC1) — the control loop, and only this |
| `IRQ_PRIO_EXT_ADC` | 4 | Other ADC DMA channels, SPI3 + its DMA, EXTI2 (ADS131M04 DRDY) |
| `IRQ_PRIO_AUX` | 8 | I2C4, SPI4/LCD, TIM2 (state machine), TIM6 |

SysTick stays at 15 (`TICK_INT_PRIORITY`).

Because the control loop can now preempt `statemachine_step()`,
`ctrl_main_stop_control()` clears `mode` **before** disabling PWM and
`ctrl_main_start_ctrl()` assigns `mode` **last**. That ordering is load-bearing
— a preempting tick must see either `CTRL_MODE_OFF` or a fully configured mode,
never a half-built one.

### DAC
| Channel | Signal | Purpose |
|---------|--------|---------|
| DAC1_CH1 (PA4) | `VREF_2_UC` | Analog reference voltage |
| DAC1_CH2 (PA5) | `I_1A_REF` | 1 A current reference for resistance mode; driven as a 1 Hz / 50 % square wave via TIM6 so the milliohm reading can be gated to the on-phase |

### I2C (I2C4: SCL=PC6, SDA=PC7)
All slaves share one bus. Transactions are queued and asynchronous (DMA +
event-driven); the queue is guarded against the I2C ISR and drops with
`ERROR_IIC_OVF` when full rather than blocking.

### SPI
- **SPI3** (PA10/11/12, CS=PA15): ADS131M04 external ADC. DRDY_N interrupt on PD2; Sync/Reset on PD0.
- **SPI4** (PE2–6): LCD display — software bit-bang due to PCB layout constraint.

### HRTIM
| Macro | Timer | `sTimerxRegs[]` index | Role |
|-------|-------|----------------------|------|
| `HRTIM_CHANNEL_PRIM` | Timer B | 1 | Primary buck-boost |
| `HRTIM_CHANNEL_HV` | Timer C | 2 | HV flyback |
| `HRTIM_CHANNEL_SEK` | Timer E | 4 | Secondary winding |
| `HRTIM_CHANNEL_ALL` | — | 0xFF (sentinel) | "all channels", disable only |

`hrtim_set_freq()` and `hrtim_set_duty()` reject invalid channels and clamp the
period / compare value to `[0x60, 0xFFDF]`, the window the HRTIM imposes.

### UART (USART2: TX=PD5, RX=PD6)
CLI debug interface — calibration, diagnostics, parameter read/write. Polled,
no interrupt. Includes a keyboard-control mode (`control`) that drives the
on-device encoder/buttons from the terminal, for UI work without the display
board attached.

### QUADSPI
W25N01GV NAND flash (CS=PA2, CLK=PA3, IO=PA6/7/PB0/1). Holds the menu icon
store (`icon_store.c`, seeded from `icon_seed_data.c` via the `flashIcons` CLI
command). Data logging not yet wired up.

### Key GPIO Groups
| Group | Signals | Notes |
|-------|---------|-------|
| Converter control | `CONV_CTRL_EN`, `CONV_CTRL_PRIM_L/H`, `CONV_CTRL_SEC_L/H`, `HV_CTRL_PRIM/EN` | Enable/disable half-bridge legs |
| Output switching | `OUT_SEL_ISO`, `OUT_SEL_HV`, `SHUNT_EN`, `SHUNT_ISO_EN` | Route signal paths, driven per mode from `mode_table[]` |
| Protection | `OVP_N`, `OVP_RESET`, `OCP_N`, `DISCHARGE_N` | Latch reset and discharge control |
| BMS | `BMS_CTRL_WAKEUP` (PF9) | Wake BQ76905 from low-power state |
| UI inputs | `BUTTON_ESC` (PD4), `BUTTON_OUT` (PB9), `BUTTON_OK` (PE1), `ENCODER_A/B` | User controls |
| UI detect | `UI_PRESENT` | Detects whether display board is connected |

---

## 5. Configuration, Calibration and Protection

### Persistent config (`config_store.c`, `eeprom.c`)
Versioned layout on the 256-byte I2C EEPROM: an 8-byte header (`psvn`), a
16-byte `hardware_data` block (serial number) and a 192-byte `calibration`
block, each with its own CRC32. A block is adopted only if its CRC checks out,
so a corrupt block falls back to defaults rather than leaving garbage in a live
calibration value. Loaded at boot before `adc_init()` and `ctrl_main_init()`.

Holds ADC offset/gain trims for six internal channels plus their external-ADC
counterparts, all 16 PID gains, pack capacity, and the 1 A reference DAC code.

### Calibration (`calibration.c`)
Six calibratable channels (`V_TERM`, `V_SENS`, `V_OUT`, `V_HV`, `I_OUT`,
`I_ISO`), each with a zero step and an optional gain step. Reachable from both
the Settings > Calibration screen and the CLI (`zeroCal`, `gainCal`,
`setOffset`, `setGain`). Persists immediately on each step.

### Protection (`protection.c`)
Polled once per state-machine tick. Classifies the four external NTC
temperatures into OK / WARNING / ERROR with hysteresis, and latches the `OVP_N`
and `OCP_N` hardware fault pins as ERROR. `OCP_N` is only evaluated in ISOMETER
mode, where it is meaningful.

An ERROR-level fault forces the converter output off from `statemachine_step()`
regardless of mode. Button-gated modes require the OUT button to be released
and pressed again before resuming; CHARGE drops back to IDLE and re-enters on
its own if the supply is still present.

---

## 6. Source Module Reference

| File (Core/Src/) | Role |
|------------------|------|
| `main.c` | Peripheral init, main event loop |
| `statemachine.c` | 11-mode FSM, UI navigation, protection interlock |
| `mode_table.c` | Per-mode descriptor table — single source of truth for mode data |
| `ctrl_main.c` | Control mode dispatch, per-mode PI loops, PID registry |
| `ctrl_PID_control.c` | Generic PI with integrator-clamping anti-windup |
| `adc.c` | ADC config per mode, scaling, ISR conversion + snapshot |
| `protection.c` | OVP/OCP and temperature supervision |
| `config_store.c` | Versioned, CRC'd EEPROM configuration store |
| `eeprom.c` | ZD24C02B I2C EEPROM driver |
| `calibration.c` | Per-channel offset/gain calibration |
| `event.c` | 64-entry event queue |
| `timer.c` | Software millisecond timers on TIM2 |
| `list.c` | Fixed-pool list used by `timer.c` |
| `error.c` | Error code ring buffer (persistence not implemented) |
| `bq76905.c` | BMS driver (I2C, voltage/current/temp, balancing, SoC) |
| `ads131m04.c` | External 4-ch ADC driver (SPI, CRC, gain config) |
| `hrtim.c` | HRTIM init, frequency/duty helpers, SEK short for AMPMETER |
| `dac.c` | DAC reference output, 1 Hz square wave for milliohm mode |
| `temp_conv.c` | NTC resistance → temperature conversion |
| `display.c` | Screen layout and rendering (calls µGUI) |
| `menu.c` | Menu tree: order, names, accent colours, procedural icons |
| `icon_store.c` | Menu icon bitmaps in QSPI flash, with procedural fallback |
| `input.c` | Encoder/button abstraction with debounce; HW or CLI-simulated |
| `ui_ctrl.c` | Status LEDs and ambient-adaptive backlight |
| `opt3004.c` | Ambient light sensor |
| `lp581x.c` | LP5816/LP5817 LED driver |
| `driver_pca9554.c` | I/O expander for hardware ID reading |
| `w25n01gv.c` | QSPI NAND flash driver |
| `cli.c` | UART command parser (calibration / diagnostics / keyboard control) |
| `aux_io_ctrl.c` | Relay and GPIO output helpers |

---

## 7. Build and Verification

Built with STM32CubeIDE (managed build, `-Os`). The bundled ARM toolchain lives
under `/opt/st/stm32cubeide_*/plugins/…gnu-tools-for-stm32.*/tools/bin/`.

There is no unit test harness and no host build. The automated check available
is **`sw/bat_source/tools/check.sh`**, which syntax-compiles every
`Core/Src/*.c` with `-Wall -Wextra` using the same flags as `.cproject` and
diffs the diagnostics against `tools/warnings.baseline`. New warnings fail;
resolved ones are reported as progress. `--update` regenerates the baseline.

Run it after every change. It compares on (file, diagnostic text) and ignores
line numbers, so unrelated edits above a warning do not perturb it. It does not
link, so section sizes and stack headroom still need a full CubeIDE build.

---

## 8. Error Handling

**Error codes** (`error.h`):

| Code | Cause |
|------|-------|
| `ERROR_IIC_READCONF` | I2C config read failed |
| `ERROR_IIC_INIT` | I2C init failed |
| `ERROR_IIC_SELFTEST` | I2C self-test failed |
| `ERROR_IIC_OVF` | I2C queue overflow |
| `ERROR_TTC_OVF` | Timer queue overflow |
| `ERROR_EVENT_OVF` | Event queue overflow |
| `ERROR_EEPROM_TIMEOUT` | EEPROM did not acknowledge |

Errors are queued but **not persisted** — `error_Write()` is an unimplemented
stub. The queue is guarded against ISR / main-context races.

**Hardware protection layers:**
1. **BMS** (BQ76905) — over/under-voltage, over-current, over-temperature per cell; watchdog
2. **OCP latch** — analog overcurrent detection via shunt; blocks gate driver via `OCP_N`
3. **OVP crowbar** — shorts converter secondary when output voltage exceeds threshold; latch requires explicit `OVP_RESET`
4. **Active discharge** — MOSFET Q18 safely dissipates stored energy on shutdown
5. **Firmware** — `protection.c` plus the interlock in `statemachine_step()` (see §5)

---

## 9. UI & Display

- **Display**: 240×320 TFT LCD, driven via µGUI + custom LCD driver (`Library/LCD/`)
- **Backlight**: LP5816 LED driver, brightness faded toward an OPT3004-derived target
- **Controls**: Rotary encoder (quadrature) + 3 pushbuttons (ESC, OUT, OK), debounced in `input.c`
- **Icons**: bitmaps from QSPI flash when available, procedural vector glyphs otherwise
- **Display boards**: Isolated supply from a forward converter on PS1-1BA; PS1-2BA carries the display, encoder, and LED driver ICs

---

## 10. Known Issues & Open Items

| # | Area | Issue |
|---|------|-------|
| 1 | Power-on | Inrush current (~26 A) triggers BMS overcurrent at startup — precharge circuit needs redesign |
| 2 | GaN drivers | Bootstrap capacitor discharge limits maximum converter on-time at low duty cycles |
| 3 | I2C | Level shifter outputs 2.4 V instead of 3.3 V — marginal for some slaves |
| 4 | Current meas. | ~100 mA noise floor on instrumentation amp output |
| 5 | Isolation meas. | 129 µA offset on isolation current channel (op-amp input leakage) |
| 6 | Voltage ref | Reference rail requires output load capacitor for stability |
| 7 | PID tuning | **All PID gains are placeholder values; no tuning has been performed.** This matters more than it used to: the HV loops previously defaulted to zero gain, which silently disabled ISOMETER. They now default to real (untuned) values, so ISOMETER will actually drive the flyback. Treat the first run as a bench bring-up. |
| 8 | ISOMETER ADC | `adc_configure_mode()` does not assign the shared hadc1..4 trigger for ISOMETER, so its control-loop rate inherits whatever mode ran before it — while `CTRL_FREQ`-scaled gains assume 25 kHz. Deferred pending bench validation. |
| 9 | Ohmmeter ADC | `RESISTANCE_1mA`'s case label in `adc_configure_mode()` is commented out, so it gets no ADC trigger **and** no hadc4 configure/start — yet its buck loop regulates on `converted.v_out`, which hadc4 supplies. Deferred pending bench validation. |
| 10 | Charge thresholds | The CC→CV latch tests the internal, uncalibrated `v_in`, while cycle termination tests the BMS `StackVoltage`. Same threshold, two different sensors. |
| 11 | Calibration UI | The Settings > Calibration screen re-runs `adc_configure_mode()` and blocking delays every tick, and does not restore the previous mode's ADC routing on exit. |
| 12 | Data logging | QSPI flash driver exists and holds the icon store, but logging is not wired into the application |
| 13 | Error log | `error_Write()` is an unimplemented stub, so queued errors are discarded |
