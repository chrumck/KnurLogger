# Session records and interpretation

This file owns local-record interpretation. Measurement methods and admissible physical
comparisons live in the ndLouvers pressure and thermal companions.

## Counters and validity

1. `readErrors`, `crcFailures`, `i2cFirstAttemptFailures`, `i2cRecovered` and `i2cExhausted`
   are cumulative totals, including counters inside `channels[]`. Use the last record for
   a session total and differences between adjacent records for increments. Never sum totals.
2. Count invalid cycles using `valid`/`reason` or `validMask`, not a nonzero cumulative counter.
   The rule applies to pressure, enclosure and temperature records.
3. `appData.i2c.*` is shared across workers. An I2C count in a pressure record includes other
   bus users; the record containing it does not identify which sensor failed.
4. Check device presence before interpreting exhausted transfers. A missing device is different
   from a present device whose transfers fail.
5. A skipped BME280 measurement invalidates the enclosure values without necessarily incrementing
   the transfer-error count. Read status and validity rather than errors alone.

## Clocks and alignment

1. Offline wall time and filenames can be wrong. NTP may step `taiUs` inside a session when
   the box regains a network. Compare `bootUs` across the same records before declaring data lost.
2. Use `bootUs` and `sessionUs` for elapsed timing. Do not carry an absolute-time offset between
   boots, or select a session solely by its wall-clock filename. Confirm its actual contents.
3. Match RaceChrono to the local file using the recorded cycle counter when available.
   For enclosure and temperature, `0x601`/`0x603` bytes 4–5 correspond to local `cycle`;
   account for counter wrap and separate recording fragments.
4. Without a usable link, use the thermal companion's physical alignment method:
   cross-correlate cavity temperature with GPS speed low-passed at approximately 180 s, and
   require substantial overlap to avoid spurious fits on a few edge bins. That is a fallback,
   not a universal clock offset.
5. Do not restore GPS-time fetching — see [CLAUDE.history.md](CLAUDE.history.md) §2.5.
6. A hard-cut file can simply end without a closing record or `BLE stopped` event. A clean
   shutdown records the BLE stop. Connection-event interpretation before the GLib context fix
   is limited as described in [ble-protocol.md](ble-protocol.md).

Record mounting state and measurement roles for the session. Neither a plausible value nor a
correctly decoded packet establishes where the sensor was or whether its pressure path was qualified.
