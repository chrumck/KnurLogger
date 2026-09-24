# Sensor interfaces and bus diagnostics

[SystemSetup/logger-perfboard-wiring.md](SystemSetup/logger-perfboard-wiring.md) owns the net
list, addresses, serial-to-channel allocation and assembly. Consult its §2 and §5 before
working on a physical channel. This file owns software-facing bus rules and diagnostic technique.

## Addressing and identity

1. The five SDP810s are hard-soldered; moving or isolating a sensor requires desoldering.
   Identify each by returned product number and serial, not its board position. Retain its
   returned scale factor; do not hard-code 60 counts/Pa for both products.
2. The sensors share fixed address `0x25`, so select one mux channel at a time. Use only the
   configured populated-channel list. Never probe channel 5: its pull-ups are not fitted.
3. The PCA9548A is strapped to `0x70`; the BME280 at `0x77` occupies the address a differently
   strapped mux could collide with. The BME280 chip ID is `0x60` at register `0xD0`.
4. Each populated downstream channel requires its own pull-ups. The build sheet owns values.
5. Read the mux control register back after writing it and verify the selected channel.
   A seemingly successful write to a faulty segment can otherwise hang the main bus.

## Idle-bus refusal

1. On some boots the first `I2C_RDWR` after an idle interval is refused with `EREMOTEIO`.
   The behavior is bimodal per boot. A clean boot does not close the physical-cause question
   in [ndLouvers open item 44](../ndLouvers/work-progress.md).
2. `i2cBus.cxx` owns the counted retry: up to ten attempts, separated by 500 µs.
   Do not add another retry layer in a sensor worker or hide recovered/exhausted transfers.
3. A clean `i2cdetect` is not proof that spaced transfers work. Check several spaced reads with
   `i2ctransfer -y 1 w1@0x77 0xd0 r1`; a single success or failure cannot establish the mode.
4. Check `present` before interpreting exhausted transfers. Errors while a device is absent
   are not evidence of a deteriorating connected bus. Counters are cumulative and shared;
   [session-records.md](session-records.md) explains the calculation.
5. The configured 10 Hz pressure loop still leaves the bus idle between cycles. Do not raise
   the sample rate to conceal the refusal; its physical cause and sampling choice are separate.

## A downstream segment hangs the main bus

1. First inspect the Pi side with `pinctrl get <n>` when a device is silent. Check wiring before
   trying to repair a physical reset fault in software.
2. A clean scan, both lines idle-high and failed transfers can indicate SDA shorted to SCL on
   the selected downstream segment. Check `SDA_CH<n>` to `SCL_CH<n>` continuity.
3. Recover using GPIO17: `pinctrl set 17 op dl`, pause, then `pinctrl set 17 op dh`.
   Reset between populated channels during diagnosis so one failed segment cannot mask the rest.

## SDP810 continuous mode and sampling

1. Use stop-continuous `0x3FF9`, identity `0x367C`/`0xE102`, then start `0x3615` once per
   sensor. Each measurement is a nine-byte frame with a CRC on each word.
2. Starting continuous mode again while already running is NAK'd. Mux switching does not stop
   that mode. Track `isContinuousStarted` per sensor; a failed read clears it so the next visit
   re-arms and pays `SDP810_START_SETTLE_US`. Do not re-arm unconditionally every cycle.
3. Keep `0x3615` differential-pressure temperature compensation. Do not switch to the mass-flow
   compensation variants as a substitute for the barometric correction specified in
   [pressure-testing.md §2.3a–§2.4](../ndLouvers/pressure-testing.md).
4. At this logger's read interval, `average till read` is exponential smoothing, not a full-cycle
   mean. Its roughly 10 ms time constant does not cover the configured 100 ms interval.
   Changing sampling or averaging requires the measurement decision in
   [open item 54](../ndLouvers/work-progress.md); the hardware retry is a separate question.
5. Pressure correction is specified but not implemented. Read
   [pressure-correction.md](pressure-correction.md) before changing conversion or configuration.

## BME280 quantities

1. `enclosurePressurePa` is enclosure pressure. It is unsuitable as an aerodynamic static
   reference, but supplies the pressure correction's absolute-pressure input. Log it for every
   bench and road session. Physical interpretations belong to
   [thermals-testing.md §1.4](../ndLouvers/thermals-testing.md).
2. `cavityTemperatureC` is the cavity thermometer. It is not the inlet-air temperature for
   density; use the measurement plan's ambient channel for that purpose.
3. `enclosureHumidityPct` supports dewpoint-margin interpretation. The measurement acceptance
   of a sealed enclosure belongs to the instrumentation specification.
4. Forced mode, ×1 oversampling and IIR off minimize self-heating of the cavity thermometer.
   A skipped temperature invalidates pressure and humidity through shared `t_fine`; successful
   transport does not make the resulting measurement valid.

## Other interfaces

1. [one-wire-probes.md](one-wire-probes.md) owns DS18B20 binding, powered-bus operation,
   `28-*` filtering, bulk-read permissions and trigger length, stale-probe behavior, offsets
   and the accepted 85 °C blind spot. Read it before changing that worker.
2. [racechrono-channels.md](racechrono-channels.md) owns packet meanings. The ESP32 test's
   synthetic `0x600`/`0x601` definitions do not describe the Pi's BME280 packets.
3. The four thermal probes are installed on the car. A phone hotspot can provide a local SSH
   path without internet if its NetworkManager profile is configured beforehand. Keep PSKs out
   of git. Enrollment and logging must also survive unattended power-on; BLE is the feedback
   path that shows what RaceChrono actually receives.
4. Wi-Fi gating is manual when diagnosing a BLE problem. Keep Bluetooth enabled and never use
   `rfkill block all`; see [operations.md](operations.md).

The diagnoses behind these rules are in [CLAUDE.history.md](CLAUDE.history.md).
