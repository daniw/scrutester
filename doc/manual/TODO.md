# Manual — open points

Status: first draft (2026-10-24), content taken from firmware `sw_rework`
(`8fe45d2`), `sw/architecture.md` and `doc/src/requirements.tex`.
Every point here also has an inline `\todo` in the LaTeX source (EN + DE).
Values marked `\tbc{}` (orange `?` in the PDF) are design/firmware values
that have not been verified on the device.

## Information needed (from you)

### Device / hardware
- [ ] Power-on procedure (which button, hold time) and emergency off via ESC
- [ ] Front panel: photo, jack labels, which jack is used in which mode
      (source, sense, HV, measurement, charging)
- [ ] Function of the status LEDs
- [ ] Scope of delivery (test leads, Kelvin clips, charger, case)
- [ ] Fuse: location, rating, type, replacement procedure
- [ ] Battery replacement procedure (with pictures), cell order number
- [ ] Dimensions, weight
- [ ] Measurement category (CAT III 600 V?), IP rating (IP54?), temperature ranges
- [ ] Is the DUT actively discharged after the isolation test? Waiting time?
- [ ] Ampmeter: max. permissible circuit voltage, fuse

### Charging
- [ ] Charging jacks and polarity, required supply current, delivered charger?
- [ ] Charging while switched off? Does the device switch off after charging?
- [ ] Charging temperature range
- [ ] Charging time from empty (requirement 4 h vs. 4.5 Ah at 1 A ~ 4.5 h)
- [ ] Re-start after ESC: is disconnecting the supply enough?

### Measurement data (to measure on the bench)
- [ ] Accuracy of all modes (after calibration)
- [ ] Milliohmmeter: range, resolution, update rate, open-lead indication
- [ ] Ohmmeter: range (10 MOhm at 1 mA?), open-circuit voltage
- [ ] Isolation test: range, accuracy, max. test current, settling time
- [ ] Voltmeter: input resistance, resolution, AC behaviour
- [ ] Runtime per mode, standby/self-discharge when off
- [ ] Where is the battery state of charge shown during normal use?

### Service
- [ ] Calibration per channel: reference source, connection, reference value,
      active mode; required equipment; acceptance limits
- [ ] CLI: connector/adapter, which commands are in the release build
      (`CLI_ENABLED` / `CLI_DEBUG`), arguments
- [ ] Firmware update procedure (tools, steps, is calibration kept?)
- [ ] When `flashIcons` / `flash_icon_seed.py` is needed
- [ ] Repair contact, spare parts

### Formal
- [ ] Required user qualification (intended use)
- [ ] Manufacturer address, conformity (CE?), disposal notes (WEEE / VREG)
- [ ] Manual / firmware / hardware revision on the title page
- [ ] Photo of the finished device for the title page

## Decisions / firmware findings
- [ ] Voltage Source: encoder allows 63.5 V (127 x 0.5 V). Clamp to 60 V or
      document 63.5 V?
- [ ] Current Source: encoder allows 12.7 A (127 x 100 mA), limit trips at
      13 A. Clamp to 10 A?
- [ ] Voltage Source: adjustable current limit or only the fixed 1.5 A trip?
- [ ] Isolation test: 1000 V released? (concept/requirements say 500 V)
- [ ] Isolation screen shows the debug line `Duty` — remove for release?
- [ ] Temperature warning (70 °C): shown to the user? If yes, add to messages
- [ ] Manual brightness setting / further settings planned?
- [ ] Menu order on the device vs. appendix menu tree
- [ ] Main menu name for IDLE is still "BatSource" in `menu.c` (not shown?)

## Content still to create
- [ ] Screenshots of all screens (main menu, every mode, settings, charging)
- [ ] Connection diagrams (4-wire, isolation test, current source, ampmeter)
- [ ] Example procedures for FS inspection (BSPD test, IMD test, equipotential
      bonding, insulation test of the tractive system)
- [ ] More troubleshooting cases from bench/field use

## Tooling
- [ ] German babel (`texlive-lang-german`) is not installed in the VM: the DE
      build falls back to hand-set captions and English hyphenation. With the
      package installed `manual.sty` loads babel automatically.
- [ ] Keep `src/en/` and `src/de/` in sync on every change
