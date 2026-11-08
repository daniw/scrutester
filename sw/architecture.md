# BatSource — Firmware Architecture

> Lives in this repo (moved from the older `bat_Source` checkout) so it travels
> with the code it describes. If you change firmware structure, change this file
> in the same commit.

## 1. Hardware Platform

### Microcontroller
**STM32G474VET6** (Arm Cortex-M4, LQFP-100; rated 170 MHz, run at **160 MHz**: HSI 16 MHz ÷ 2 × 40 ÷ 2 PLL, `SystemClock_Config()` / `.ioc`)
- Selected for its High-Resolution Timer (HRTIM, ×32 DLL: ~195 ps resolution at 160 MHz) needed for precise converter PWM
- 512 KB flash, 128 KB SRAM
- 5× ADC, 2× DAC, HRTIM, I2C, SPI, UART, QUADSPI, RTC

### Key External ICs

| IC | Role | Interface |
|----|------|-----------|
| BQ76905 (TI) | Battery Management System — 4-cell LiFePO₄ monitoring, protection, passive balancing | I2C4 (0x08) |
| ADS131M04 (TI) | 4-ch 24-bit sigma-delta ADC: ch0 `V_TERM`, ch1 `I_OUT`, ch2 `V_SENS`, ch3 `I_ISO` (`adc.c`); OSR 16256 at 8 MHz CLKIN (TIM16) → ~246 Hz | SPI3 |
| LMG2100R044 ×2 (TI) | GaN half-bridge drivers for primary buck-boost converter | HRTIM GPIO |
| 1EDN7512B | Gate driver for HV flyback primary switch | HRTIM GPIO |
| W25N01GV | 1 Gbit QSPI NAND flash — menu icon store, intended for data logging | QUADSPI |
| ZD24C02B | 2 kbit I2C EEPROM for calibration / ID data | I2C4 (0x50) |
| TCA9554 (PCA9554) | I2C I/O expander — hardware ID reading | I2C4 (0x20) |
| LP5817 / LP5816 | LED drivers — status LEDs and LCD backlight on UI board | I2C4 (0x2D / 0x2C) |
| OPT3004 | Ambient light sensor — automatic backlight control | I2C4 (0x44) |
| LCD (240×320 TFT) | Display — sunlight-readable, replaces original OLED | SPI4 (hardware, DMA — see §4) |

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
- Output relays K1 (converter output ↔ jacks) and K2 (HV flyback path), both normally closed, opened by driving `OUT_SEL_HV` / `OUT_SEL_ISO` high, for galvanic isolation between measurement paths
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
│     while ((e = event_Get()) != VOID)        │
│       process_event(e);                      │
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
| `EVENT_BMS_TIMER` | BMS polling timer (`BQ76905_POLL_ms`, 100 ms) |

### State Machine (`statemachine.h` / `statemachine.c`)
Step period: **20 ms** (`STATEMACHINE_STEP_PERIOD_mS`), driven from TIM2 via the
software timer list in `timer.c`. Handles UI menu navigation, output enable
logic, and the protection interlock.

| Mode | Description |
|------|-------------|
| `STATEMACHINE_IDLE` | Startup / menu navigation |
| `STATEMACHINE_MODE_60V_OUT` | Regulated voltage output, 0–60 V, buck / 4-switch / boost chosen on the fly (hold OUT to enable) — see §"Full-range voltage source" |
| `STATEMACHINE_MODE_10A_OUT` | 10 A regulated current output (hold OUT to enable) |
| `STATEMACHINE_MODE_RESISTANCE_1A` | Milliohmmeter, 1 A pulsed excitation |
| `STATEMACHINE_MODE_RESISTANCE_1mA` | Ohmmeter, 1 mA excitation |
| `STATEMACHINE_MODE_ISOMETER` | Insulation resistance test (HV flyback active) |
| `STATEMACHINE_MODE_VOLTMETER` | Passive voltage measurement |
| `STATEMACHINE_MODE_AMPMETER` | Passive current measurement (SEK half-bridge shorted) |
| `STATEMACHINE_MODE_SETTINGS` | Settings menu |
| `STATEMACHINE_MODE_SHUTDOWN` | Controlled power-down sequence |
| `STATEMACHINE_MODE_CHARGE` | Internal battery charging — auto-entered from IDLE when a 15–24 V supply is detected, not user-selectable |
| `STATEMACHINE_MODE_EMI` | EMI test (debug only: `EMI_DEBUG` in `debug.h`): both converters switch with no external load. HV flyback open loop at `CTRL_PARAM_EMI_HV_DUTY` (1 %), LV converter regulates `OUT_LV` with the full-range voltage loop of 60V_OUT (encoder setpoint, default `CTRL_PARAM_EMI_VOLTAGE_DEFAULT_mV` 40 V), so the LV loop returns the HV energy to the battery. Both relays closed. An OUT press toggles run/stop (not hold-to-enable); aborts above `PROTECTION_EMI_MAX_mV` / `PROTECTION_EMI_MAX_mA` on V_TERM / I_OUT (protection limits); exempt from the inactivity power-off while running. Bench-run before the rework: works, but the LV loop oscillates (untuned). |

Settings sub-modes (`statemachine_settings_modes_t`): `MENU`, `BMS`,
`DISPLAY`, `CALIBRATION`, `ADC1`, `ADC2`, `PROTECTION`, `ABOUT`, `SERVICE`,
`STORAGE`.
The list shows Display, About, Storage mode and Service; Service opens a 4-digit PIN entry
(`SETTINGS_SERVICE_PIN`, encoder digit, OK next, ESC back; wrong PIN shows a
message for 1 s). A correct PIN unlocks BMS, Calibration, ADC 1/2 and
Protection (replacing Service) until reset/power-off; the flag is RAM only.
The CLI is not restricted. `statemachine_settings_list()` builds the visible
list; `current_menu_index` is the position in it.

**Storage mode** (Settings > Storage mode): shows the SoC (ideal ~50 %) and
shuts the BMS down on OK. Refused while V_TERM is above
`CTRL_PARAM_CHARGE_START_VIN_LOW_mV` (a charger would wake the BMS again).
`statemachine_storage_mode()` switches the outputs off, forces
`BMS_CTRL_WAKEUP` low (TRM SLUUCI8A 8.5: TS high during shutdown ends in soft
shutdown) and calls `BQ76905_Shutdown()`, which sends the SHUTDOWN subcommand
(0x0010) three times, 30 ms apart, blocking I2C (TRM: twice within 4 s, the
third skips the 10 s delay). The BMS opens CHG/DSG and the device loses power
(~1 uA in shutdown vs 5.9 uA in SLEEP). If still alive after ~2 s (watchdog
kicked) it shows "Storage mode failed" and returns to the list. Wake path:
charger -> K1 (normally closed) powers the MCU -> boot pulses
`BMS_CTRL_WAKEUP` -> BMS starts with factory defaults -> `BQ76905_init()`
reconfigures it. Independent of this, the BMS already shuts itself down at
cell < 2.0 V / stack < 8.0 V (`bq76905_config.h`).

Per-mode data (control mode, relay/shunt mask, enable GPIO, entry flags, ADC
trigger) is centralised rather than duplicated across `statemachine.c`,
`ctrl_main.c`, `adc.c` and `aux_io_ctrl.c` — see `mode_table.c`.

**Inactivity auto power-off**: `STATEMACHINE_INACTIVITY_TIMEOUT_TICKS`
(`statemachine.c`, 10 min), checked once per tick in every mode, not just
`STATEMACHINE_IDLE`. Resets on any encoder movement (tracked as the raw
count, independent of what the current mode does with it), any button
press/hold, or `ctrl_main_handle.mode == CTRL_MODE_CHARGE`. `CHARGE` is the
only mode kept unconditionally exempt — there is no button that would
otherwise ever satisfy the timeout mid-charge. `60V_OUT`/`10A_OUT`/`ISOMETER`
are hold-OUT-to-enable, so `out_button_pressed` alone (already one of the
button terms above) already covers "stay on while OUT is held" for them, no
special-casing needed. `RESISTANCE_1A`/`1mA` auto-start their control loop on
entry (`MODE_F_AUTOSTART_CTRL`, `mode_table.c`) and keep it running with OUT
untouched — earlier this instead keyed off `ctrl_main_handle.mode !=
CTRL_MODE_OFF` (a control loop actively running at all), which gave them a
blanket exemption from the timeout no matter how long they sat untouched;
now, like any other passive-readout mode, only holding OUT keeps them from
timing out. It deliberately does **not** use `statemachine_handle.output_on`:
`mode_table.c`
documents that field as left stale across a mode switch for the
passive-readout modes (see the `VOLTMETER` entry there), so it can't be
trusted as an "active right now" signal, unlike `ctrl_main_handle.mode`,
which `ctrl_main_stop_control()` (called from `statemachine_switchtoIdle()`)
reliably clears every time `IDLE` is (re-)entered.
`STATEMACHINE_MODE_SHUTDOWN` never becomes a lasting `current_mode` (see
`statemachine_switchfromIdle()`), so it needs no case of its own. Once the
timeout fires, `gpio_power_off()` (`gpio.c`) runs — also used by the CLI's
`turnOff`: deasserts `ON_REQ`, waits ~100 ms to confirm the on/off controller
actually cut power, and re-asserts it (logging that shutdown failed) if the
MCU is still running.

### Control Loop (`ctrl_main.h/c`, `ctrl_PID_control.h/c`)

Executed from the **ADC1 DMA-complete interrupt**, not directly from HRTIM.
The chain is: HRTIM timer period → HRTIM ADC trigger (with post-scaler) → ADC
conversion → DMA → `HAL_ADC_ConvCpltCallback()`, which acts only for `hadc1`
and decimates by 2 (`adc_interrupt_cnt`). The trigger is the mode's
`mode_table[].adc_trigger`; every active mode lands at **25 kHz = `CTRL_FREQ`**:

| Modes | ADC trigger | Source |
|-------|-------------|--------|
| 60V_OUT, 10A_OUT, RESISTANCE_1A/1mA, EMI | TRG7 (`ADC_TRIGGER_HRTIM_FIXED`) | Timer D, fixed 1 MHz, no outputs, ÷20. PRIM/SEK change frequency per region, and the sample sits at a fixed point of the sync frame |
| CHARGE | TRG3 (`ADC_TRIGGER_HRTIM_SEK`) | Timer E (SEK, 1 MHz), ÷20 |
| ISOMETER | TRG5 (`ADC_TRIGGER_HRTIM_HV`) | Timer C (HV, 350 kHz), ÷7 |
| IDLE | TRG1 (`ADC_TRIGGER_HRTIM_PRIM`) | Timer B (PRIM), ÷20; no loop |
| VOLTMETER, AMPMETER, SETTINGS | none | keep the previous trigger |

Only ADC1's DMA transfer-complete IRQ is enabled; ADC2-5 DMA runs without
IRQs (`adc_start_dma_quiet()`), the data just lands in `adc_data.raw`.

`CTRL_FREQ` is what every integral gain and startup ramp is scaled by, so any
change to switching frequency or post-scaler changes the effective loop tuning.

**PID controller** (`ctrl_PID_control.h`): generic P/I/D with integrator-
clamping anti-windup and configurable saturation limits. The D term is compiled
out (`NO_D_PART`); all loops run as PI.

**Tunable loops** — `ctrl_pid_table[]` in `ctrl_main.c` is the single source of
truth. Each row carries the live controller, its EEPROM-backed gain fields
(`config_store.gains`), its defaults and its duty saturation limits. `ctrl_main_init()`,
`config_store_setDefaults()` and the CLI's `setPI` / `saveEEPROM` /
`readEEPROM` all iterate it, so adding a loop means adding one row.

| # | Name | Used for |
|---|------|----------|
| 0 | Buck | Buck voltage (resistance modes, buck region of the voltage source) |
| 1 | Boost | Boost region of the voltage source (60V_OUT) |
| 2 | Current | 10 A output |
| 3 | BoostIoutLimit | Output current limiter of the voltage source (all regions) |
| 4 | ChargeCurrent | CC phase of charging |
| 5 | ChargeVoltage | CV tail of charging: outer loop at 50 Hz, output = charge current (A/V) |
| 6 | HVVoltage | ISOMETER HV flyback voltage |
| 7 | HVCurrent | ISOMETER leakage-current limiter |
| 8 | FourSwitch | 4-switch region of the voltage source |
| 9 | 10AVoltLimit | 10 A compliance limiter: PI on the fast `v_out` (target `CTRL_PARAM_10A_COMPLIANCE_mV`, 5 V) caps the Current PI's duty, whose integrator then tracks the applied duty, so open terminals no longer wind it up to the 0.5 ceiling (~V_IN/2). Starts at its ceiling; gains = buck voltage loop, untuned. |

### Full-range voltage source (60V_OUT)

The LV converter is a 4-switch buck-boost, `V_out = D_prim·V_in/(1 − D_sek)`.
`ctrl_main_ctrl_voltage_wide()` picks the region from `m = V_ref/V_in`
(`vout_region.c`, host-tested by `tools/test_vout_region.sh`), using the ramped
reference so the start-up ramp walks through the regions by itself:

| Region | Entered when | PRIM | SEK | PI |
|--------|--------------|------|-----|----|
| BUCK | m < 1 − `VOUT_REGION_MARGIN` (0.75) | 1 MHz, duty = PI | 50 kHz, `CONST_DUTY_LOW` | Buck |
| FOUR_SWITCH | in between | 1 MHz, D | 1 MHz, same D | FourSwitch |
| BOOST | m > 1 + margin (1.25) | 50 kHz, `CONST_DUTY_HIGH` | 1 MHz, duty = PI | Boost |

The way back needs `VOUT_REGION_HYSTERESIS` (0.05) more. The ratio uses V_IN
low-passed (`VOUT_VIN_FILTER_s`, 10 ms) and each region is held at least
`VOUT_REGION_DWELL_s` (20 ms); with the raw sample it toggled at the 1.25 edge.
A region change preloads the incoming PI with the duty that gives the same
ideal ratio. Legs and burst then change over three control ticks
(`vout_transition_step()`, host-tested), as a burst-leg switch is immediate
but a new period only lands at that timer's next reset: (0) legs that become
fast switch, the old burst keeps a sync block; (1) the burst moves to the new
fast leg (off in 4SW); (2) legs that become parked drop to 50 kHz. A leg
waiting to be parked runs at 1 MHz with its duty limited to
[`VOUT_MIN_RUN_DUTY`, `VOUT_MAX_RUN_DUTY`]. So no parked pulse lands outside
an idle block (before: one per change, and buck <-> boost could leave the
new parked leg in burst idle). The 1 A limiter (fast internal `i_out`) caps the duty in every region; it
starts at its ceiling and the voltage PI's integrator tracks the applied duty
while it is capped (from 0 it caused a 5 V -> 9 V start-up overshoot). Gains bench-tuned at V_in ~13 V, light load; limiter untested.

**Parked-leg sync and pulse skipping** (60V, 10A and RESISTANCE_1A/1mA modes,
`VOUT_SYNC_BURST_DISABLE` in `debug.h` removes it). The parked leg's 1.5 % low pulse used
to land anywhere, including while the fast leg's high side was on, which gave
a ≥ 2.4 A floor into a short. The master timer (in the `.ioc`) defines a 20 µs
frame: compare 1 resets Timers B/D, compare 3 resets E half a frame later, so
the parked pulse sits at the same frame time in buck and boost. The burst
controller (clock Timer D, trigger master compare 2, `hrtim.c`) idles a block
of fast cycles there, **on the fast leg only** (low side on, high side off);
a 50 kHz leg in up/down mode only enters/leaves burst idle at its roll-over
(every 10 µs), which on the bench left SEC_L on for ~10 µs per frame. Block =
`CTRL_PARAM_VOUT_SYNC_CYCLES` (3: ~100 nC bootstrap charge at 100 mA = 1 µs,
plus margin; master compares 2/3 in the `.ioc` assume 3). Below the HRTIM
compare floor (3.75 % at 1 MHz) the block grows and the run duty is held at
`CTRL_PARAM_VOUT_MIN_RUN_DUTY` (`vout_burst_plan()`, host-tested; buck, 10A
and RESISTANCE only). These modes trigger the ADC from Timer D, so the sample
sits at a fixed frame time (~19.5 µs, outside the idle block); Timer B's
post-scaler phase depends on the mode history. CHARGE (SEK fast, PRIM parked)
still runs free: there the idle block (SEC_L on) would cap the high-side
share at ~0.81 and need a charger ≥ ~17 V. In boost the idle block holds SEC_L
on, so it counts as SEK duty 1: `vout_burst_plan_low_side()` plans the
high-side share instead (before, the real SEK average was ~0.15 above the
command). The boost duty range is therefore ~0.19 (floor from
`CTRL_PARAM_VOUT_MAX_RUN_DUTY`) to `CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH`. The 4-switch region runs without the burst. Bench-checked in 10A,
buck and boost.

**Key parameters** (`ctrl_param.h`):

| Parameter | Value |
|-----------|-------|
| `CTRL_FREQ` | 25 000 Hz |
| `CTRL_PARAM_SW_FREQ_HIGH` | 1 000 000 Hz |
| `CTRL_PARAM_SW_FREQ_LOW` | 50 000 Hz |
| `CTRL_PARAM_SW_FREQ_HV` | 350 000 Hz |
| `CTRL_PARAM_CHARGE_CURRENT_mA` | 1000 mA |
| `CTRL_PARAM_CHARGE_END_VOLTAGE_mV` | 14 000 mV (3500 mV × 4) |
| `CTRL_PARAM_CHARGE_TAPER_CURRENT_mA` | 100 mA |
| `CTRL_PARAM_CHARGE_RESTART_MARGIN_mV` | 300 mV |
| `CTRL_PARAM_STACK_VOLTAGE_MIN/MAX_VALID_mV` | 8000 / 20 000 mV |

> **Note**: Buck, Boost, FourSwitch and Current are bench-tuned (`ctrl_param.h`
> comments; V_in ~13 V, light load). BoostIoutLimit, ChargeCurrent,
> ChargeVoltage, HVVoltage, HVCurrent and 10AVoltLimit still run initial
> values — see Known Issues #7.

### Charging (multi-phase start, then CC/CV)
Auto-entered from IDLE when the external terminal voltage
(`v_term_ext_mv`) is 15–24 V, no BMS safety fault (`safetyStatusA`/`B`) is
latched, the BMS `StackVoltage` is inside its plausibility window and below the
end voltage minus the restart margin, and no lockout is active (see below).
The same charger-window check refuses entry from the CLI's `state` command.

Starting charge used to produce very high inrush currents, from two causes: the
output relay closed against a running converter whose output was not at the
charger voltage, and the CC loop was preloaded so the SEK duty started near
zero, which is the *maximum*-current end. The start is therefore a sequence
(`charge_seq.c`, phases `charge_phase_t`; the ISR publishes the phase in
`ctrl_main_handle.charge_phase`, the state machine steps it every 20 ms):

| Phase | Output relay K1 | What runs |
|-------|-----------------|-----------|
| `PRECHARGE` | open (`OUT_SEL_HV`=1; K2 also open, `OUT_SEL_ISO`=1) | Converter as an **open-loop boost**: PRIM ramps `CTRL_PARAM_CHARGE_PRECHARGE_PRIM_START_FRACTION` (0.1) → full duty over `CTRL_PARAM_CHARGE_PRECHARGE_PRIM_RAMP_s` (0.2 s) with SEK held at `CTRL_PARAM_CHARGE_PRECHARGE_SEK_DUTY_MIN` (0.015), then the SEK duty follows `D = 1 − 0.985·V_in/V_ref` (clamped to `_SEK_DUTY_MIN`/`_MAX`, own copies of the 60V boost limits) with `V_ref` ramped to `CTRL_PARAM_CHARGE_PRECHARGE_RATIO` (0.99) × the latched charger voltage over `CTRL_PARAM_CHARGE_PRECHARGE_SEK_RAMP_s` (0.2 s), then a `CTRL_PARAM_CHARGE_PRECHARGE_HOLD_s` (0.2 s) hold. The start in `ctrl_main_start_ctrl()` writes the first ramp tick's duties (`charge_precharge_start()`), so there is no duty step. |
| `CLOSE` | commanded closed (`OUT_SEL_HV`=0), wait `CTRL_PARAM_CHARGE_RELAY_SETTLE_ms` (100 ms) | ISR holds the duties. At the command the SEK duty is stepped to the **zero-current duty** `D0 = 1 − 0.985·V_in/V_term` and the CC PI is preloaded to `0.985 − D0`. |
| `CC_RAMP` | closed | Charge-current loop; the reference ramps 0 → `CTRL_PARAM_CHARGE_CURRENT_mA` over `CTRL_PARAM_CHARGE_RAMP_s` (5 s). |
| `CV` | closed | Constant-voltage tail (below). |

Both output relays are **normally closed**: driving `OUT_SEL_HV` (K1, converter
output ↔ jacks) or `OUT_SEL_ISO` (K2, HV flyback path) high *opens* them. The
relay state of each mode is in `mode_table[]`; CHARGE's row is the PRECHARGE
state, and the relays are set *before* the converter starts
(`statemachine_enter_charge()`).

The duty relations behind this (see `charge_seq.h`): boost
`V_OUT_LV = D_prim·V_in/(1 − D_sek)`; charging (buck) direction current
`≈ (V_OUT_LV·(1 − D_sek) − D_prim·V_bat)/R`, so a *larger* SEK duty means *less*
charge current; the charge loop writes `D_sek = 0.985 − PI action`, bounded only
by the PI limits (the former 0.5 SEK duty ceiling was removed).

CC loop feedback: P part on the fast internal `i_out`, I part on the battery
current from the BMS (`CC2Current`, ~100 ms update, positive = charging), so
the device's own consumption does not reduce the current into the pack. Also
used in the recovery run.

**Bench debug build** (`CHARGE_DEBUG` in `debug.h`, off by
default): gates everything below and the
periodic/peak CLI printouts in `statemachine.c`, so none of it is in a normal
build. The single-step API itself (`charge_seq_set_manual()` etc.) stays
compiled in either way — inert with nothing to call it — so `charge_seq.c`
needs no `#ifdef` and stays host-testable as-is.

**Single-step bench test mode** (`charge_seq_set_manual()`): the CLI's
`chargeStep 1` makes CHARGE hold after PRECHARGE finishes (relay still open,
duty held) instead of continuing on its own, so open-loop boosting alone can
be checked (TP12, the periodic `V_IN`/`V_OUT` print) before `chargeNext`
releases it -- closing the relay and starting the current ramp. The flag
persists across CHARGE (re-)starts until `chargeStep 0`.

**OUT_LV (the converter output node behind K1) is not measured.** `V_OUT`,
`V_TERM` and `I_OUT` are all on the jack side of the relay, so with K1 open
`V_OUT` just reads the charger; only test point TP12 sees OUT_LV. PRECHARGE is
therefore open-loop and only as accurate as `V_IN` and the duty model, which is
why the target is `CTRL_PARAM_CHARGE_PRECHARGE_RATIO` (99 %) and not 100 %. A stuck-open relay is detected in
`CC_RAMP` (reference above `CTRL_PARAM_CHARGE_STUCK_OPEN_REF_mA` for
`…_ms` with less than `…_MEAS_mA` flowing) and aborts the charge.

The end-of-charge exit is only evaluated in `CC_RAMP`/`CV`; supply drop, BMS
fault, ESC and protection exits apply in every phase. ESC, protection, BMS-fault
and stuck-open exits set a **lockout** so the IDLE auto-entry does not restart
CHARGE on the next tick; it is cleared when the charger is removed
(`v_term_ext_mv` < `CTRL_PARAM_CHARGE_START_VIN_LOW_mV`). `charge_percentage`
is set to 100 % and the accumulator reset only on true completion.

The CC→CV latch and the cycle-termination check both key off the same
sensor, the BMS `StackVoltage`, against `CTRL_PARAM_CHARGE_END_VOLTAGE_mV` —
the latch additionally requires `StackVoltage` to be within a sanity range
(`CTRL_PARAM_STACK_VOLTAGE_MIN/MAX_VALID_mV`) before it fires, guarding
against latching on a bad BMS reading. This corrects an earlier version of
this document, which claimed the latch tested the internal `v_in` instead;
current code shows no such path.

**Deep-discharge / BMS undervoltage recovery charging.** If the pack is
discharged far enough that the BQ76905 latches a cell-undervoltage (CUV)
fault, it opens its DSG FET (Q5, `hardware.tex`) — the battery side (`V_IN`)
then reads ~0 V, so the boost-from-`V_IN` `PRECHARGE` phase above is
meaningless. The device can still be running at all in this state only
because K1 (`OUT_SEL_HV`) is normally closed and the MCU's own supply is fed
through it from the charger port into `OUT_LV` — a path independent of the
battery/DSG FET entirely. Every normal-charge exit path
(`statemachine_switchtoIdle()`) would otherwise open K1 as part of its usual
IDLE relay config, cutting that supply. **`statemachine_switchtoIdle()`
therefore keeps K1 closed whenever CUV (`BQ76905_SAFETY_STATUS_A_CUV`, bit 6
of `Safety Status A` per the BQ76905 TRM) is latched**, regardless of why it
was called — this single chokepoint fix covers boot, a BMS-fault exit, a
stuck-open abort, and ESC alike, rather than special-casing each one.

With K1 guaranteed closed, the IDLE auto-entry check (above) gets a second
branch: CUV latched (and no *other* fault bit), charger voltage plausible,
not locked out → `statemachine_enter_charge_low_current()`
(`statemachine.c`) instead of the normal `statemachine_enter_charge()`. This
skips `PRECHARGE`/`CLOSE` and enters `CC_RAMP` directly
(`charge_seq_init_low_current_recovery()`): PRIM is set straight to its
normal pass-through duty (`CTRL_PARAM_CONST_DUTY_HIGH`, no soft-start — `V_IN`
gives no usable feedback to ramp against), SEK is preloaded to the
zero-current duty the same way the normal `CLOSE → CC_RAMP` hand-over does,
and the current reference ramps 0 → `CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA`
(100 mA, ~10 % of the normal charge current, independently tunable) over the
usual `CTRL_PARAM_CHARGE_RAMP_s`. Physically, this current reaches the pack
through Q6 (the separate charge FET, gated directly by the BMS's
`BMS_ON_OFF.CHG`, independent of DSG) and Q5's body diode — the BQ76905 TRM
confirms CUV only opens DSG, "the CHG FET remains enabled if already
enabled".

While this recovery run is active (`charge_seq_t.low_current_recovery`), the
CHARGE fault-exit check masks out the CUV bit specifically — it's exactly
the condition the run exists to clear, so it must not itself abort the
charge — while any *other* fault bit still aborts immediately, same as a
normal charge. Once CUV clears (weakest cell recovers above threshold +
hysteresis — autonomous per the BQ76905, no manual recovery needed), the
current reference is ramped from the recovery target up to the full
`CTRL_PARAM_CHARGE_CURRENT_mA` in place, over the same `CTRL_PARAM_CHARGE_RAMP_s`
(`statemachine_step_charge()`, main context — `ctrl_main_handle.ramp`, the
ISR's own start-of-charge ramp, is already saturated by then, so this is a
second, independent step applied directly to `current_reference_mA`); no
relay recycle or restart. From there the run is indistinguishable from a
normal charge, including the full fault check. **Bench-validated on hardware.**

The CV tail is a cascade: the CC loop keeps running in the ISR, and the CV
PI (`ctrl_main_charge_cv_step()`, main context, every 50 Hz tick) regulates
`4 × highest cell voltage` (BMS, 10 Hz) and sets the CC loop's target
(`charge_cv_current_mA`), clamped to [0, `current_reference_mA`]. So the
current stays limited in CV (charger steps, load changes), and CV turns into
the taper on its own. At the CC→CV latch the ISR preloads the CV integrator
with the present current reference (no step). `setPI` uses each table row's
`rate_hz` (CTRL_FREQ, 50 Hz for ChargeVoltage).
**Residual concern**: the latch/termination logic still gates on real
`StackVoltage`, while the CV loop itself now regulates towards an
*extrapolated* `4 × max-cell` target — under significant cell imbalance
these two quantities diverge, which is untested. The cycle terminates once
`StackVoltage` ≥ end voltage, output current has tapered below
`CTRL_PARAM_CHARGE_TAPER_CURRENT_mA`, **and** the cell-voltage spread
(`max(cell) - min(cell)`) is ≤ `CTRL_PARAM_CHARGE_CELL_SPREAD_mV` (50 mV), or
after `CTRL_PARAM_CHARGE_BALANCE_TIMEOUT_s` (1 h) at end voltage/taper with a
larger spread ("Finished, cells unbalanced"). `CTRL_PARAM_CHARGE_MAX_TIME_s`
(5 h) ends it as "Charge timeout". These three stay in CHARGE on a static
finished screen (converter off, IDLE relay configuration), where the
inactivity power-off applies again; ESC goes to IDLE with the lockout. A BMS
safety fault or the supply dropping below `CTRL_PARAM_CHARGE_STOP_VIN_mV` exits
to IDLE immediately. Live ADC
values are printed on the CLI (~10 Hz, from `statemachine_step()`, not the ISR) throughout
charging for bench visibility, plus once the peak |`I_OUT`| in the 0.4 s after the
relay-close command, for tuning `PRECHARGE_RATIO` and `RELAY_SETTLE_ms`.

### EMI mode: EMC monitor (`emc_monitor.c`, `emc_param.h`)

Debug builds only (`EMI_DEBUG`). While an EMI run is active (`output_on`),
`statemachine_step_emi()` calls `emc_monitor_step()` every tick. It checks one
row per channel of the `EMC_CHANNELS` table in `emc_param.h` (name, read
expression, limits; the only place to edit criteria): V_TERM (setpoint +-2 %,
min +-0.5 V) and I_OUT (50..150 mA), both unfiltered ext ADC; V_HV (placeholder
range); the rails 3V3, 3V3A, 5V, 15V (+-5 %) and VCC (+-5 % around its value at the end of blanking, it follows the battery);
min/max cell (3.0..4.25 V); hottest NTC (< 70 C, an open NTC reads 1000); ext ADC
CRC errors, BMS-stale and I2C errors (`i2c_error_count()`, must not increase).

Per channel it latches FAIL, min/max, the number of excursions (entries into
out-of-spec) and the time of the first one. The first `EMC_BLANKING_ms` (2 s)
after the start are not checked. Start of a run and an OK click reset all
latches (the OK click also restarts time and blanking). Nothing is persisted.
Each new excursion prints (first per channel always, then at most one line per second) `EMC FAIL <name> <value> at mm:ss.mmm`; the CLI command
`emcReport` prints the per-channel summary. The EMI screen is the monitor list
(value, min/max, OK/FAIL per channel; status bar PASS or FAIL since mm:ss),
drawn one item per tick.

---

## 4. Peripheral Map

### ADC
| Instance | DMA | Role |
|----------|-----|------|
| ADC1 | DMA1_Channel2 | `I_OUT` — **carries the control loop** |
| ADC2 | DMA1_Channel3 | `V_TERM` |
| ADC3 | DMA1_Channel4 | `V_IN` |
| ADC4 | DMA1_Channel5 | `V_OUT` / `V_HV` (channel switched per mode; `V_OUT` also in 10A for the compliance limiter) |
| ADC5 | DMA1_Channel1 | 12-channel scan: rails, NTCs, VBAT, VREFINT; plus an injected group for `I_ISO` / `I_BAT` |

ADC5 uses 256× hardware oversampling with 4-bit right shift (effective 16-bit result).

`adc_configure_mode()` re-points triggers and channels per operating mode.
HRTIM ADC triggers: TRG1 ← Timer B (PRIM, ÷20), TRG5 ← Timer C (HV, ÷7),
TRG3 ← Timer E (SEK, ÷20), TRG7 ← Timer D (fixed 1 MHz, no outputs, ÷20 —
`ADC_TRIGGER_HRTIM_FIXED`, used by 60V/EMI, 10A and RESISTANCE: fixed phase in
the sync frame; configured in `bat_source.ioc`).

**Concurrency**: `adc_data.raw` is DMA-written and declared `volatile`.
`adc_data.converted` is computed in the ISR into a private instance and copied
into the globally visible one as a single snapshot under a critical section,
once per state-machine tick (`adc_snapshot_converted()`), so main-context
readers never see a torn mix of two sample instants. `ctrl_main_ctrl()` takes
`const ADC_CONVERTED_DATA *` so it cannot reach for raw or calibration fields
that the ISR-side instance does not populate.

**Ext ADC filtering**: `v_term_ext_mv`, `i_out_ext_mA`, `v_sens_ext_uv` and
`i_iso_ext_uA` each have a `*_filt` copy, low-passed at ~2 Hz
(`ADC_EXT_FILT_ALPHA`) right after the snapshot, at the fixed 50 Hz tick (the
ISR rate depends on the mode). Only displayed or printed readouts use
`*_filt`; control, protection, charge decisions/latches and calibration use
the unfiltered values.

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
`ERROR_IIC_OVF` when full rather than blocking. A transfer running longer than
50 ms is aborted by `i2c_check_timeout()` (per state-machine tick and in the
blocking helpers): DeInit, 9 SCL clocks + STOP, Init; the stuck entry is
reported via its error callback and the queue is kept. A failed DMA start is
reported as an error the same way.

### SPI
- **SPI3** (SCK/MISO/MOSI = PC10/11/12, CS=PA15 GPIO): ADS131M04 external ADC. DRDY_N interrupt on PD2; Sync/Reset on PD0. Every data frame is checked against its output CRC (CCITT); a bad frame keeps the old values and counts `crc_errors` (`printADC`).
- **SPI4** (PE2–6): LCD display — real hardware SPI peripheral, DMA-driven for
  bulk transfers (`lcd.c`). Only SCK/MISO/MOSI (PE2/PE5/PE6) are the SPI4
  peripheral pins; PE3 (`DISPLAY.WR`, the D/C line) and PE4 (`DISPLAY.CS`) are
  plain GPIO outputs toggled by hand around each transfer (`LCD_DC`/`LCD_CS`
  in `lcd.h`) — normal for an SPI LCD, not bit-banging. (This corrects an
  earlier version of this doc, which called it "software bit-bang due to a
  PCB layout constraint"; nothing in the code or `bat_source.ioc` supports
  that, and `spi.c`'s `MX_SPI4_Init()` configures SPI4 as an ordinary hardware
  peripheral.)

### HRTIM
| Macro | Timer | `sTimerxRegs[]` index | Role |
|-------|-------|----------------------|------|
| `HRTIM_CHANNEL_PRIM` | Timer B | 1 | Primary buck-boost |
| `HRTIM_CHANNEL_HV` | Timer C | 2 | HV flyback |
| `HRTIM_CHANNEL_SEK` | Timer E | 4 | Secondary (output-side) half-bridge |
| — | Timer D | 3 | No outputs: fixed 1 MHz ADC trigger (TRG7) and burst clock for the parked-leg sync |
| — | Master | — | 20 µs sync frame (resets B/D and E, burst trigger), `.ioc` |
| `HRTIM_CHANNEL_ALL` | — | 0xFF (sentinel) | "all channels", disable only |

`hrtim_set_freq()` and `hrtim_set_duty()` reject invalid channels and clamp the
period / compare value to `[0x60, 0xFFDF]`, the window the HRTIM imposes.

### UART (USART2: TX=PD5, RX=PD6)
CLI debug interface — calibration, diagnostics, parameter read/write. RX by
interrupt into a 256-byte ring buffer (`USART2_IRQHandler()` in `cli.c`,
`IRQ_PRIO_AUX`), so pasted input survives long main loop passes; TX blocking
(`printf`). Includes a keyboard-control mode (`control`) that drives the
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
| Output switching | `OUT_SEL_ISO`, `OUT_SEL_HV`, `SHUNT_EN`, `SHUNT_ISO_EN` | Route signal paths, driven per mode from `mode_table[]`. The two output relays are normally closed: `OUT_SEL_HV`=1 opens K1 (converter ↔ jacks), `OUT_SEL_ISO`=1 opens K2 (HV flyback ↔ jacks). CHARGE additionally drives `OUT_SEL_HV` itself, mid-sequence. **Exception**: `statemachine_switchtoIdle()` never drives `OUT_SEL_HV` high while a BMS CUV fault is latched, since K1 may be the device's only power path then — see §3, "Deep-discharge / BMS undervoltage recovery charging". |
| Protection | `OVP_N`, `OVP_RESET`, `OCP_N`, `DISCHARGE_N` | Latch reset and discharge control |
| BMS | `BMS_CTRL_WAKEUP` (PF9) | Wake BQ76905 from low-power state |
| UI inputs | `BUTTON_ESC` (PD4), `BUTTON_OUT` (PB9), `BUTTON_OK` (PE1), `ENCODER_A/B` | User controls |
| UI detect | `UI_PRESENT` | Detects whether display board is connected |

---

## 5. Configuration, Calibration and Protection

### Persistent config (`config_store.c`, `eeprom.c`)
Versioned layout (PSVN 3) on the 256-byte I2C EEPROM: an 8-byte header
(`psvn`), a 16-byte `hardware_data` block (serial number), a 96-byte
`calibration` block (ADC offset/gain trims, pack capacity, 1 A reference DAC
code; append-only) and a 96-byte `gains` block (P/I of every `ctrl_pid_table`
row), each with its own CRC32. A block is adopted only if its CRC checks out,
so a corrupt block falls back to defaults rather than leaving garbage in a live
value. Loaded at boot before `adc_init()` and `ctrl_main_init()`.

On a PSVN upgrade the calibration (and serial number) is kept and all gains
are reset to the table defaults; PSVN 2 (gains mixed into the calibration
block) is converted field by field. A newer PSVN (downgrade) uses defaults and
leaves the EEPROM untouched. Host test: `tools/test_config_store.sh`.

### Calibration (`calibration.c`)
Seven calibratable channels (`V_TERM`, `V_SENS`, `V_OUT`, `V_HV`, `I_OUT`,
`I_ISO`, `V_IN`), each with a zero step and an optional gain step. The
internal `I_ISO` channel defaults to a schematic-derived gain/offset
(`ADC_IISO_GAIN_UA`, offset 2048 = VREF/2); ISOMETER itself uses the external
ADC's `I_ISO`. Reachable from both
the Settings > Calibration screen and the CLI (`zeroCal`, `gainCal`,
`setOffset`, `setGain`). Persists immediately on each step.

### Protection (`protection.c`)
Polled once per state-machine tick. Classifies the four external NTC
temperatures into OK / WARNING / ERROR with hysteresis, and latches the `OVP_N`
and `OCP_N` hardware fault pins as ERROR. `OCP_N` is only evaluated in ISOMETER
mode, where it is meaningful. An open NTC (-50 °C table end) is an ERROR.
Per-mode limits on |V_TERM|, |I_OUT| and I_ISO (external ADC,
`PROTECTION_<mode>_MAX_*` in `protection_param.h`, debounced over
`PROTECTION_LIMIT_DEBOUNCE_TICKS`) trip as `LIMIT_V`/`LIMIT_I` ERRORs, latched
until OUT is released or the mode changes.

An ERROR-level fault forces the converter output off from `statemachine_step()`
and shows a red banner in the mode screen footer (cleared on the next start).
Button-gated modes require the OUT button to be released and pressed again
before resuming; RESISTANCE stays off until re-entered; CHARGE drops back to
IDLE and re-enters on its own if the supply is still present. AMPMETER and
VOLTMETER (no controller) leave to IDLE with the banner on the idle screen;
leaving AMPMETER also releases the SEK short.

`OVP_N` low (OVP latch) is a lockout until a user-confirmed reset
(`statemachine_ovp_lockout()`): converters off, SEK half-bridge shorted
(`hrtim_sek_force_short()`, `CONV_CTRL_EN` on, PRIM off), relays unchanged,
all mode handling skipped. The short takes over from the crowbar thyristor if
the internal converter caused the OVP and carries the current to blow the
fuse if an external source did. The banner asks to remove the external
voltage; an OK click (max 3 attempts) runs a tick-based reset: SEK short
released (`hrtim_sek_restore()`), 2 ticks, 1 ms `OVP_RESET` pulse, 2 ticks,
then `OVP_N` is read directly. High: back to IDLE. Low: lockout again,
attempt counter +1. After 3 failures only a power cycle resets.

BMS data older than `BQ76905_STALE_ms` (3 s, no complete async read cycle) is
`BMS_STALE`: ERROR in CHARGE (stops the charge), WARNING elsewhere; it also
blocks the CHARGE auto-entry. A read cycle stuck that long is restarted.

At boot `ADS131M04_selftest()` calibrates the external ADC's offsets on
shorted inputs and checks every channel with the internal ±test signal (2/15
of full scale, ±25 %, symmetric within 5 %). A failure is `EXT_ADC`: ERROR
until power cycle, so no output mode runs, VOLTMETER/AMPMETER leave to idle
and CHARGE is not auto-entered. `EXT_ADC_DEBUG` prints the values per channel.

---

## 6. Source Module Reference

| File (Core/Src/) | Role |
|------------------|------|
| `main.c` | Peripheral init, main event loop |
| `statemachine.c` | FSM over 12 modes (`statemachine_modes_t`; EMI only with `EMI_DEBUG`), UI navigation, protection interlock, OVP lockout |
| `mode_table.c` | Per-mode descriptor table — single source of truth for mode data |
| `ctrl_main.c` | Control mode dispatch, per-mode PI loops, PID registry, CHARGE phase handling in the ISR |
| `vout_region.c` | HAL-free full-range region choice, region transition steps, burst plan (host-tested) |
| `balancing.c` | Cell balancing mask written to the BMS every tick |
| `emc_monitor.c` | EMI-mode EMC immunity monitor (`EMI_DEBUG`); criteria in `emc_param.h` |
| `charge_seq.c` | HAL-free CHARGE start sequencing: duty formulas, PRECHARGE ramp, phase stepper, stuck-open detector (host-tested) |
| `ctrl_PID_control.c` | Generic PI with integrator-clamping anti-windup |
| `adc.c` | ADC config per mode, scaling, ISR conversion + snapshot |
| `protection.c` | OVP/OCP and temperature supervision |
| `config_store.c` | Versioned, CRC'd EEPROM configuration store |
| `eeprom.c` | ZD24C02B I2C EEPROM driver |
| `calibration.c` | Per-channel offset/gain calibration |
| `event.c` | 64-entry event queue |
| `timer.c` | Software millisecond timers on TIM2 |
| `list.c` | Fixed-pool list used by `timer.c` |
| `error.c` | Error code queue, printed on the CLI by `error_Work()` from the main loop (not persisted) |
| `fault.c` | `fault_halt()`: fault handlers and `Error_Handler` switch the power stage off and print the fault |
| `watchdog.c` | Independent watchdog (IWDG), register-level; started last in `main()`, kicked once per main-loop iteration |
| `bq76905.c` | BMS driver (I2C, voltage/current/temp, balancing, SoC) |
| `ads131m04.c` | External 4-ch ADC driver (SPI, CRC, gain config) |
| `hrtim.c` | HRTIM init, frequency/duty helpers, SEK short for AMPMETER |
| `dac.c` | DAC reference output, 1 Hz square wave for milliohm mode |
| `temp_conv.c` | NTC resistance → temperature conversion |
| `display.c` | Screen layout and rendering (calls µGUI) |
| `menu.c` | Menu tree: order, names, accent colours, procedural icons |
| `icon_store.c` | Menu icon bitmaps in QSPI flash, with procedural fallback |
| `input.c` | Encoder/button abstraction with debounce; HW or CLI-simulated; clamped (non-wrapping) encoder read for live setpoints |
| `version.h` | `FW_VERSION_MAJOR/MINOR` plus `GIT_HASH` (`git describe --always --dirty --abbrev=7`) and `GIT_DATE` (HEAD commit date) from the generated, git-ignored `Core/Inc/git_version.h`, written by `bat_source/makefile.defs` (included by the CubeIDE makefile; pure make functions, only rewritten on change). Shown on Settings > About and as a CLI line at boot. Falls back to `unknown` without git or the header (`__has_include`). |
| `splash.c` | Boot splash (logo, title, credit) drawn from fill primitives — no bitmap in flash |
| `ui_ctrl.c` | Status LEDs and ambient-adaptive backlight |
| `opt3004.c` | Ambient light sensor |
| `lp581x.c` | LP5816/LP5817 LED driver |
| `driver_pca9554.c` | I/O expander for hardware ID reading |
| `w25n01gv.c` | QSPI NAND flash driver |
| `cli.c` | UART command parser (calibration / diagnostics / keyboard control); `setGPIO`/`setDAC`/`setDuty`/`enPWM`/`wdtTest` only with `CLI_DEBUG`, `state` goes via IDLE |
| `debug.h` | All debug / bench flags (`CHARGE_DEBUG`, `EXT_ADC_DEBUG`, `EMI_DEBUG`, `CLI_DEBUG`, `CLI_ENABLED`, `VOUT_SYNC_BURST_DISABLE`, CLI flash test options) |
| `aux_io_ctrl.c` | Relay and GPIO output helpers |

---

## 7. Build and Verification

Built with STM32CubeIDE (managed build, `-Os`). The bundled ARM toolchain lives
under `/opt/st/stm32cubeide_*/plugins/…gnu-tools-for-stm32.*/tools/bin/`.

There is no general unit test harness and no host build of the whole firmware.
Three host tests compile a HAL-free module with host gcc (stub headers in `tools/stub/`):
- **`tools/test_charge_seq.sh`** — `charge_seq.c`: duty formulas, PRECHARGE
  ramp, phase stepper, deep-discharge recovery start.
- **`tools/test_vout_region.sh`** — `vout_region.c`: region choice, duty
  model, reachable PI limits, burst plan, three-tick region transition.
- **`tools/test_config_store.sh`** — `config_store.c`: PSVN 2 conversion,
  same-PSVN load, upgrade, downgrade and blank EEPROM.

The automated compile check is
**`sw/bat_source/tools/check.sh`**, which syntax-compiles every
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

Errors are queued (guarded against ISR / main-context races; a full queue
drops the oldest entry) and printed on the CLI by `error_Work()` from the main
loop. They are **not persisted** and not shown on screen.

**Fault handlers**: HardFault/MemManage/BusFault/UsageFault/NMI and
`Error_Handler()` call `fault_halt()` (`fault.c`): IRQs off,
`hrtim_emergency_off()` (register-only: gate-driver enables off, burst/IDLES1
cleared, all HRTIM outputs disabled), fault name and CFSR/HFSR/fault address on
USART2, then spin until the IWDG resets the MCU. Before `watchdog_init()` (boot)
it spins until power-off, with the power stage off. Name, address, CFSR and HFSR are also stored in a `.noinit` RAM
section (linker script, not zeroed at startup); `fault_report_reset()` prints
"Reset cause: <RCC flags>" and "Last reset: FAULT ..." at boot and clears them.
`fault_last_reset_text()` gives a screen text ("Restarted after fault (...)" /
"Restarted by watchdog" / NULL). IWDG flag plus a record = fault, IWDG flag
alone = hang.

**Watchdog** (`watchdog.c`): the 25 kHz control loop lives in the ADC ISR, so a
hung main loop would leave the converter running on stale setpoints. The IWDG
(LSI ÷ 32, `WATCHDOG_TIMEOUT_MS` = 2000) is started by `watchdog_init()` as the
last step before the main `while (1)` — it cannot be stopped once running, so
starting it after init means the boot sequence (splash, flash/BMS init, 100 ms
waits) needs no kicks. `watchdog_kick()` runs once per main-loop iteration and
inside `w25n01gv_wait_busy()` (flash erase / icon flashing chain many busy-waits
far longer than the timeout) and `gpio_power_off()`'s wait loop. **Rule: any new
blocking wait that can approach ~1 s must call `watchdog_kick()`.** The LSI
tolerance is wide, so the real timeout can be well below 2 s. The debugger
freezes the counter (`__HAL_DBGMCU_FREEZE_IWDG()`), so breakpoints don't reset
the chip. After a reset `fault_report_reset()` prints the reset cause and
clears the sticky reset flags. `wdtTest` (CLI)
hangs the main loop on purpose; bench-verified: the board resets and reboots (the power latch rides through, see known issue #15). The kick is
unconditional per loop iteration — a dead control ISR alone would not trip it
(see known issue #15).

**Hardware protection layers:**
1. **BMS** (BQ76905) — over/under-voltage, over-current, over-temperature per cell; watchdog
2. **OCP latch** — analog overcurrent detection via shunt; blocks gate driver via `OCP_N`
3. **OVP crowbar** — shorts converter secondary when output voltage exceeds threshold; latch requires explicit `OVP_RESET`. Firmware adds the SEK half-bridge short and drives `OVP_RESET` after an OK confirmation, max 3 attempts (§5)
4. **Active discharge** — MOSFET Q18 safely dissipates stored energy on shutdown
5. **Firmware** — `protection.c` plus the interlock in `statemachine_step()` (see §5)

---

## 9. UI & Display

- **Display**: 240×320 TFT LCD, used in landscape (`LCD_ROTATION 3`, so `LCD_WIDTH` = 320, `LCD_HEIGHT` = 240), driven via µGUI + custom LCD driver (`Library/LCD/`)
- **Boot splash**: `splash_show()` runs right after `LCD_init()` and is replaced by the first `display_show_idle()` (`display_init()` no longer wipes the screen, so the splash stays until the menu is drawn). The logo is drawn from 6 rectangles and 4 parallelograms (`splash.c`, ~0.6 KB) rather than stored as a bitmap (~23 KB at this size). `LCD_init()` + `splash_show()` run early in `main()` — right after the peripheral init and IRQ-priority overrides, before `ui_ctrl_init()` switches the backlight on — so the splash covers the BMS/ADC/EEPROM/flash init that follows. `gpio_turnOn()` (the `ON_REQ` power latch) moved to directly after `MX_GPIO_Init()` for the same reason: `LCD_init()` takes ~250 ms.
- **Backlight**: LP5816 LED driver, brightness faded toward an OPT3004-derived target
- **Controls**: Rotary encoder (quadrature) + 3 pushbuttons (ESC, OUT, OK), debounced in `input.c`. The encoder's hardware counter wraps at 0/127; live setpoints (60V_OUT / 10A_OUT references, ISOMETER test-voltage index) read it through `input_encoder_read_clamped()` so turning past either end holds the value instead of jumping to the other end. Menu/list navigation keeps the wrapping raw count on purpose.
- **Icons**: bitmaps from QSPI flash when available, procedural vector glyphs otherwise
- **Display boards**: Isolated supply from a forward converter on PS1-1BA; PS1-2BA carries the display, encoder, and LED driver ICs

---

## 10. Known Issues & Open Items

| # | Area | Issue |
|---|------|-------|
| 1 | Power-on | (Battery-side power-on inrush; the charge-start inrush across the output relay is separate, see #12.) Inrush current (~26 A) triggers BMS overcurrent at startup — precharge circuit needs redesign. Operational workaround in the meantime: holding the startup button during power-on slows the BMS's own FET switching, reducing inrush to ~0.7 A (see `modifications.tex`). |
| 2 | GaN drivers | Bootstrap capacitor discharge limits maximum converter on-time at low duty cycles |
| 3 | I2C | Level shifter outputs 2.4 V instead of 3.3 V — marginal for some slaves |
| 4 | Current meas. | ~100 mA noise floor on instrumentation amp output |
| 5 | Isolation meas. | 129 µA offset on isolation current channel (op-amp input leakage). Calibratable per unit (`zeroCal`); since `213986a` the internal `I_ISO` channel defaults to a schematic-derived gain/offset instead of 0, ISOMETER uses the external ADC channel. |
| 6 | Voltage ref | Reference rail requires output load capacitor for stability |
| 7 | PID tuning | Bench-tuned: Buck, Boost, FourSwitch (V_in ~13 V, light load) and Current (P > 0.01 oscillates, I > 3.5 unstable). Still initial values: BoostIoutLimit (limiter untested), ChargeCurrent (used in the validated charge runs, not tuned on its own), ChargeVoltage, HVVoltage, HVCurrent, 10AVoltLimit (= buck gains), and the EMI parameters. The I gains are limited by the ~246 Hz external ADC that feeds the I-terms (#19). Treat the first run of an untuned loop as a bench bring-up. |
| 8 | ISOMETER ADC | Fixed (`e5bde9a`): ISOMETER now names its trigger explicitly (`ADC_TRIGGER_HRTIM_HV`, see `mode_table.c`), so the loop rate no longer depends on mode history. The rate was ~11.7 kHz (post-scaler ÷15); TRG5 post-scaler ÷7 now gives exactly `CTRL_FREQ`, gains unchanged (integral action and ramp ~2.1× faster than before on the bench). |
| 9 | Charge thresholds | The CC→CV latch and cycle termination both key off the BMS `StackVoltage` against `CTRL_PARAM_CHARGE_END_VOLTAGE_mV` (consistent — this corrects an earlier version of this table, which claimed the latch tested `v_in`). The CV tail's own PI loop, however, now regulates towards `4 × highest cell voltage` rather than `StackVoltage` (see §3, Charging). Under significant cell imbalance, the quantity being latched/terminated on (real `StackVoltage`) and the quantity the CV loop is driving towards (extrapolated `4 × max-cell`) can diverge — untested. Termination also now requires the cell-voltage spread to be ≤ 50 mV, which should catch most of this in practice, but the two-sensor tension itself is unresolved. |
| 10 | Data logging | QSPI flash driver exists and holds the icon store, but logging is not wired into the application |
| 11 | Error log | Since `213986a` `error_Work()` runs in the main loop and `error_Write()` prints each queued error on the CLI; a full queue drops the oldest. Still not persisted and not shown on screen. |
| 12 | Charge start | The multi-phase CHARGE start (§3) has been run on the bench and works with the current tuning (`CTRL_PARAM_CHARGE_PRECHARGE_*`, `CTRL_PARAM_CHARGE_RAMP_s` — see §3 for the values). An audible oscillation/whine was observed while the voltage first rises, clearing after roughly 1 s; suspected but **not confirmed by a scope capture** to be the open-loop, unregulated `PRECHARGE` exciting the `OUT_LV` LC tank near its resonance (10 µH inductor, 33 µF `OUT_LV` bank C22–C36 → ≈8.8 kHz, audible), since nothing damps it while `V_OUT`/`I_OUT` are unmeasured there. Shortening the ramps and raising `PRECHARGE_RATIO` (now 0.99) reduced it to where it's no longer a problem in practice, but the mechanism itself is unconfirmed. Remaining open items: the relay settle time (`CTRL_PARAM_CHARGE_RELAY_SETTLE_ms`, datasheet not in the repo) and the duty model (unloaded synchronous boost, PRIM at full duty) are still bench assumptions, not measured. A future `OUT_LV` divider on a spare ADC channel would allow closing the PRECHARGE loop and would let `I_BAT` (already sampled during CHARGE) or a real voltage loop actively damp this instead of relying on tuning alone. |
| 13 | Deep-discharge charging | The CUV recovery path (§3) has been run on the bench and works — K1 stays closed, the BMS's CUV fault is masked correctly for the run, and recovery current reaches the pack through Q6/Q5's body diode as expected. The `CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA` (100 mA) target and the reuse of `CTRL_PARAM_CHARGE_RAMP_s` for both the initial 0→10 % ramp and the later 10→100 % handoff once CUV clears are first-pass values, not independently bench-tuned yet — revisit if either ramp turns out too fast/slow in practice. |
| 14 | LCD driver speed | Fixed (`515be99`): `SPI_PRESCALER_LCD_WRITE` (`lcd.h`) had been left at 16 (10 MHz) since the very first "getting the LCD running" bring-up commit (`317ef30`), a quarter of the 4/40 MHz that `spi.c`'s static `MX_SPI4_Init()` and `bat_source.ioc` (`SPI4.CalculateBaudRate=40.0 MBits/s`) both already specify — the define's own trailing `//4 // 40MHz` was that commit's leftover note of the value actually wanted. `LCD_ReadCmd()` (only called from `LCD_init()`'s panel-ID reads) is what leaves SPI4 at this rate for the rest of runtime, so every draw after boot — splash, menus, this driver's own bulk `LCD_Fill`/`LCD_WriteData` transfers — ran 4× slower than the hardware is configured for. Restored to 4/40 MHz. **Not yet verified on the bench**: nothing in the repo documents 16 as a deliberate signal-integrity fallback, but that possibility can't be ruled out without hardware; if 40 MHz proves unreliable, try 8 (20 MHz) before reverting. |
| 15 | Watchdog | Bench-verified: `wdtTest` resets the board and it reboots (the `ON_REQ` power latch rides through the reset). **Not yet checked**: that no legitimate long path (`saveEEPROM`, `flashIcons`, `eraseFlash Full`, long LCD redraws) trips the untuned 2000 ms timeout. Also open: the kick is unconditional per main-loop pass, so a dead control ISR alone would not trip it — a gated kick (also require `adc_interrupt_cnt` to have advanced) would close that. |
| 16 | Shoot-through at stop | Fixed (`934e5c5`, `de9a358`, `b37b0bf`): `IDLES1` (low side on in burst idle) was set at boot and never cleared, so every `hrtim_disable()` switched the low sides on without dead time. Now set only on the fast leg while the burst runs and cleared before any PWM disable (`hrtim_sync_stop()` before `hrtim_disable()`, also on the parked leg at a region change and in `hrtim_emergency_off()`); `CONV_CTRL_EN` drops before the PWM stops. Not yet scope-checked. |
| 17 | ISOMETER limiter | The `if` that guarded the duty write instead of the limit is fixed (`934e5c5`: duty always written). The limiter was disabled (`598b9da`, ISOMETER only reached ~102 V) because it started at 0 and its error was in A (tiny gains). It is active again: starts at its ceiling, error in mA (`CTRL_PARAM_HV_IOUT_LIMIT_uA` 1200, P 0.05 duty/mA, I 5/s). Pending bench verification (todo A4); `PROTECTION_ISO_MAX_uA` trip and `OCP_N` latch remain as backup. |
| 18 | Fault handlers | Fixed (`207265f`): see §8, power stage off before the fault handlers spin. |
| 19 | Ext ADC rate | The ADS131M04 runs at ~246 Hz (OSR 16256). The 60V 1 A limiter now uses the fast internal `i_out` (`78c6851`). The PI I-terms (60V regions, 10A, CHARGE CC, HV) still integrate the external value, which limits the usable I gains. Open. |
| 20 | Protection interlock | Fixed (`f6d31be`): per-mode V/I limits; AMPMETER/VOLTMETER (no controller) leave to IDLE on any protection ERROR, which also releases the SEK short. Open: AMPMETER has no runtime `V_TERM` limit, only the 500 mV entry check. |
| 21 | OVP | `84d906e`: OVP latch trip → SEK short (§5). `OVP_RESET` is pulsed after an OK confirmation (max 3 attempts, SEK short released first, `OVP_N` checked afterwards). Detection is still polled (20 ms tick). |
| 22 | Ext ADC integrity | Fixed (`fc92dc0`, `f0b8dfd`): every frame checked against its output CRC (bad frame keeps the old values, counted); boot self-test latches `EXT_ADC` on failure. |
| 23 | BMS data | Fixed (`de1c0a1`): BMS data older than 3 s is `BMS_STALE` (stops CHARGE, blocks its entry); poll 10 Hz. |
| 24 | Timers / I2C | Fixed (`eecbf20`, `5c0bd41`): software timer loads above 16 bit report an error instead of truncating; I2C transfers time out after 50 ms with bus recovery. |
| 25 | EEPROM layout | Fixed (`bac4387`): PSVN 3 with separate calibration and gains blocks, all 10 PI rows persisted, calibration kept across PSVN upgrades (§5). |
