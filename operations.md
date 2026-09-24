# Build, deployment and operation

Run commands from the KnurLogger checkout unless a command explicitly uses the remote host.
[SystemSetup/pi-headless-setup.md](SystemSetup/pi-headless-setup.md) owns host setup and its progress.

## Repository, deployment and where the code is built

Upstream is **https://github.com/chrumck/KnurLogger** (`origin`), and it is **public** — see the
note at the end of this section before adding anything host-specific.

The box has the full toolchain (`git 2.47.3`, `cmake 3.31.6`, `gcc`/`g++`, `libglib2.0-dev`), so
**the code is built on the Pi**, not cross-compiled. First time:

```bash
ssh KnurLogger 'git clone https://github.com/chrumck/KnurLogger.git'
```

Thereafter `git -C ~/KnurLogger pull` on the box. The `build/` directory is gitignored except for
its tracked `KnurLogger.ini`, which is the **template**, not the deployed file.

### The dev copy and the production copy are two different files

The logger is **built** in `~/KnurLogger/build/` and **run** from `~/bin/`, and the two are kept
apart deliberately (owner decision, 2026-09-10):

| | Path | Owned by | Carries |
|---|---|---|---|
| dev | `~/KnurLogger/build/KnurLogger.ini` | git, overwritten by every sync | the seed, and the **version-controlled backup** of whatever was last copied back |
| production | `~/bin/KnurLogger.ini` | the box and `--enroll` | the live ROM ID bindings and offsets |

The dev copy is not permanently empty: it carries the four ROM IDs enrolled on 2026-09-10, copied
back after the trip. That is what makes it a backup rather than only a template — a fresh box
seeded from it starts with the current bindings, which is right, since the probes it will read are
the same four.

**The reason is that the configuration and the source now share a file.** `KnurLogger.ini` carries
the DS18B20 bindings as well as the calibration offsets; the bindings can only be made at the car;
and the `tar`-over-ssh loop below overwrites everything under `~/KnurLogger`. With one copy, one
sync from the workstation silently discards a trip to the car — which is exactly the trap this
README used to document and ask you to remember your way around.

```bash
ssh KnurLogger 'bash ~/KnurLogger/SystemSetup/deploy-logger.sh --execute'
```

`deploy-logger.sh` **always replaces the binary and only ever creates the `.ini`, never updates
it.** Run it with no arguments first, like every script in `SystemSetup/`. `KnurLogger.service`
points at `/home/chrum/bin/KnurLogger`, not at the build tree.

> **⚠ A BINARY THAT ADDS A REQUIRED KEY CRASH-LOOPS THE PRODUCTION LOGGER.** `config.cxx` exits on
> any missing key, the deploy never touches `~/bin/KnurLogger.ini`, and the service restarts every
> 5 s forever (`Restart=always`). Before deploying a build that adds a key, hand-merge it into
> `~/bin/KnurLogger.ini` — the pre-flight's diff of the two files shows what is missing — then
> deploy and check `systemctl status KnurLogger`. The pressure correction of
> `../ndLouvers/pressure-testing.md` §2.4 step 4 will be such a build.

> **⚠ The production `.ini` is not in git, so nothing else backs up a calibration or a binding.**
> After enrolling or entering offsets at the car, copy `~/bin/KnurLogger.ini` into the repo as
> `build/KnurLogger.ini` and commit it. That is a deliberate act rather than a side effect, which
> is the point — but it is also the only thing standing between a wiped SD card and another trip:
>
> ```bash
> scp KnurLogger:bin/KnurLogger.ini build/KnurLogger.ini
> ```

**The box is fed from constant 12 V, not the accessory circuit** (as built, 2026-09-10), so the
logger runs the whole day and the fuse is the off switch. Two consequences worth having here:
`SystemSetup/KnurLogger.service` is **required** rather than convenient, because nothing
hand-starts the logger when the fuse goes in; and the hard cut the ~1 s `fsync` protects against
is now the fuse being pulled, or a cranking dip, rather than ignition-off. The operating rules below cover constant-feed power and shutdown.

**Do not put push credentials on the box.** Commit and push from the workstation; the box pulls
only. A logger in a wheel-well cavity is physically exposed — if the car is broken into or the SD
card is pulled, any PAT or writable deploy key on it becomes an attacker's write access to this
repo. A public repo needs no credential to clone or pull, so read-only costs nothing.

For a tight edit-build loop, round-tripping through GitHub is slow; copy the tree to the box
instead and keep GitHub for durable commits. The loop in use is:

```bash
tar czf - --exclude=.git --exclude=build/CMakeCache.txt --exclude=build/CMakeFiles . | ssh KnurLogger 'tar xzf - -C ~/KnurLogger && cd ~/KnurLogger && cmake --build build -j4'
```

**That sync overwrites `~/KnurLogger/build/KnurLogger.ini`, and that is now harmless** — it is the
template, and the running logger does not read it. Nothing under `~/bin/` is touched until
`deploy-logger.sh` is run, and even then only the binary. This is what the dev/production split
above bought; before it, one sync destroyed a hand-entered offset silently.

**The `../ndLouvers/...` links throughout this repository are broken on github.com and that is
deliberate.** `ndLouvers` is a separate repository that happens to sit alongside this one on disk.
The links resolve locally, which is where the work happens. Do not "fix" them by copying
requirements across — a duplicated requirement is one that will drift, and `CLAUDE.md` says so.

## Getting on the box

SSH alias `KnurLogger` (`192.168.118.52`, user `chrum`), key-only.

```bash
ssh KnurLogger
```

## Enrolling the four DS18B20s

`temp0`–`temp3` mean nothing until a ROM ID is bound to each. Binding is a deliberate mode and
never happens during a logging run, so a probe that drops out and comes back mid-session cannot
re-label a channel.

**Stop the service first — it is installed and enabled, so the logger is always already running:**

```bash
ssh -t KnurLogger 'sudo systemctl stop KnurLogger'
```

Two instances both poll the bus and both advertise, and nothing warns. See Operating constraints below.

```bash
ssh KnurLogger '~/bin/KnurLogger --enroll'
```

1. Start with **no probe connected**, then plug them in **one at a time, lowest channel first** —
   the installed order the plan fixes is `temp0` = `T_ambient`, `temp1` = `T_core_in`,
   `temp2` = `T_core_out`, `temp3` = `T_aft`.
2. **Every probe stays plugged in once it is in. Do not unplug one to make room for the next.**
   Binding works either way, which is what makes this easy to get wrong — it was got wrong at the
   car on 2026-09-10 — but unplugging as you go costs three things that binding does not:
   1. **The four-probe star is never loaded**, so the run proves nothing the ESP32 bench rig had
      not already proved with single probes. All four together on the 4 × 5 m bus, enumerating
      with CRC-clean reads, is the acceptance criterion of plan thermal item 1 (closed, rev 77),
      and it is only met with all four connected at once.
   2. **Step 6's map check becomes impossible**, because warming one probe and watching one
      channel move needs four live channels.
   3. **Every unplug leaves a ~100 s tail** of a channel that is present in sysfs and answering
      with nothing — see `one-wire-probes.md` on `w1_slave_ttl`.
3. **Allow ~10 s per probe.** `w1_master_timeout` is 10 s, so that is the hot-plug latency; nothing
   can shorten it without root.
4. Each bind prints `OneWire: BOUND temp0 <- 28-…, reading 21.50 C` and writes an `enrollment`
   record. Check the ROM ID against the lead you just connected. **A bind whose reading comes back
   `INVALID` is a warning, not a failure** — the binding is still written, but the probe answered
   badly on the very read that is meant to confirm it, so re-seat that lead and watch the channel
   before trusting it.
5. **Two unbound probes in one scan are refused, not guessed.** sysfs order is not arrival order,
   so there is no fact available that says which came first. Unplug one and re-seat it alone.
6. **Enrollment keeps sampling after the fourth bind.** With all four bound, warm one probe by hand
   and watch one channel move on the phone — four warmings confirm the whole map in situ and double
   as a liveness test. Stop it with `pkill -TERM -x KnurLogger`.
7. `--enroll --reset` discards every binding and starts from `temp0`. The bindings it throws away
   are written into the session file first, because they are otherwise unrecoverable.

The bindings are written into the `[thermal]` section of the **`KnurLogger.ini` beside the
binary**, alongside the calibration offsets, as `temp<N>RomId` / `temp<N>BoundTaiUs` /
`temp<N>BoundIso` (owner decision, 2026-09-10 — this replaced a separate `channels.ini` in the data
directory; see `CLAUDE.history.md` §2.7 and do not rebuild that file). An empty `temp<N>RomId` is
an unbound channel and a perfectly good state. Three consequences:

1. **Enrollment rewrites a file you hand-maintain**, so it does it line by line: only the twelve
   binding lines are touched and every other byte, comments included, is copied through. It is not
   written through GLib's key-file serialiser, which destroys non-ASCII characters in comments —
   measured 2026-09-10, this file's em-dashes came back as `?`.
2. **The offsets are now in front of you when you change a binding**, which is the point: an offset
   is keyed to the slot, so re-enrolling in a different order strands it. See
   §[Calibration offsets](#calibration-offsets).
3. **This is the file `SystemSetup/deploy-logger.sh` refuses to overwrite.** The production copy is
   `~/bin/KnurLogger.ini` and the git-tracked template is `build/KnurLogger.ini`; see
   §[Repository, deployment and where the code is built](#repository-deployment-and-where-the-code-is-built).

**Editing a ROM ID by hand is allowed and guarded.** Anything that is not a 15-character `28-…` is
refused and logged as an event, and one ROM ID appearing on two channels is refused on the second
— a duplicate would otherwise produce two channels tracking each other perfectly, which is a
mislabelling that looks like agreement.

`SystemSetup/logger-perfboard-wiring.md` §5a is the human record of the resulting table, and it is
**filled** as of 2026-09-10. **Channel → role is a per-session record, never a channel name** — it
is in every session file's `thermalBaseline`, which is where an analysis should take it from.

## Calibration offsets

Hand-edited in `KnurLogger.ini`, one per channel, applied to the value sent to RaceChrono:

```ini
[thermal]
temp0OffsetC=0.0
temp1OffsetC=-0.375
temp2OffsetC=0.0
temp3OffsetC=0.0
```

**How to get the numbers.** Park the car long enough to reach equilibrium — engine cold, no sun, no
residual heat — and run one stationary logging session. At equilibrium all four probes sit in the
same air, so their differences are sensor error and nothing else: read the four values out of the
session file's 1 Hz `temp` records and enter each probe's deviation from the four-probe mean. Only
*relative* offsets mean anything, because there is no reference thermometer on the car and
ΔT_preheat depends on the probes' differences rather than their absolute accuracy. **Entering
nothing is a legitimate outcome** if the differences are small against a ~6 K signal.

Five things to know:

1. **They are keyed to the channel slot, not to the probe.** An offset is really a property of one
   particular DS18B20, so if the probes are ever re-enrolled in a different order, or one is
   swapped, the offsets stay with the slots and no longer describe the parts in them — **re-check
   them after any re-enrollment.** In exchange they live in the config beside every other setting,
   including the bindings that say which probe each slot holds.
2. **The session file records all three values** — `centiC` (raw), `offsetC` (in force) and
   `sentCentiC` (what went on the air) — so a mistyped offset costs a reprocess, not the session.
3. **All four keys must be present.** A missing one is a startup failure, not a silent zero: an
   offset that quietly stopped being applied would be invisible in the data it corrupts.
4. **Past ±5 °C it warns** about a likely decimal-point slip and applies the value anyway; **past
   ±50 °C it refuses to start.** The offsets in force are printed at startup and recorded in the
   `thermalBaseline` record.
5. **The ROM ID → channel bindings are in this same section**, as `temp<N>RomId`, machine-written
   by `--enroll` (2026-09-10). That is deliberate rather than incidental: item 1's consequence —
   an offset keyed to a slot stops describing the part in it after a re-enrollment — is only
   noticeable if the two are read together. A changed `temp2RomId` is the signal to re-check
   `temp2OffsetC` three lines above it.

**The logger takes no automatic session-start sample and makes no judgement about whether the car
was settled** — see `CLAUDE.history.md` §3.1 for why that was tried, measured failing, and
dropped.

## Testing the 1-Wire path without probes

The probes are on the car and the phantom `00-*` devices stopped once `R11` terminated the line, so
neither a real reading nor the family filter can be exercised on this box. The whole path is
nonetheless testable, because the code reads sysfs and sysfs can be replaced:

```bash
unshare -Urm --map-root-user /bin/bash -c 'mount --bind /tmp/fake-w1 /sys/bus/w1/devices && ...'
```

An unprivileged user namespace gives a private mount namespace, so a fake tree can be bind-mounted
over `/sys/bus/w1/devices` with **no root and no risk to the real box**. Populate it with a
`w1_bus_master1/` directory, `28-…/w1_slave` files carrying the kernel's two-line format, and a
`00-…` entry to prove the filter drops it. This is how enrollment, the ambiguous-step refusal, the
store's own family and duplicate guards, bound-but-absent, CRC failure,
out-of-range rejection and the application of a hand-entered offset were all verified.

**Its one limitation, found the hard way:** a plain file cannot represent a sysfs attribute whose
read value differs from what was written, and `therm_bulk_read` is exactly that — you write
`trigger` and read back `-1`, `0` or `1`. In the fake tree it reads back `trigger`, so that state
machine is unexercised. Two things fill the gap and are worth reaching for before concluding
something is untestable:

1. **A FIFO in place of `w1_slave`** makes a read block for a controlled time, which is how the
   `conversionMs` fallback was verified — a read forced to 385 ms produced `conversionMs 385`
   where the old code produced 0.
2. **`w1_master_add`** attaches a real family-0x28 slave with no hardware present, so `w1_therm`
   binds and its master attributes appear. This is how the udev rule was verified. It needs root,
   `w1_master_remove` undoes it, and the id is parsed as `%02x-%012llx` — the hyphen matters.

## Session files are the only copy until you take one

`filesDir` on the box is the sole home of every session until it is copied off. The workstation
backup lives at `C:\_claude\KnurLoggerData\sessions\` — **deliberately outside both git
repositories, because this one is public and session files are data.** It is **current as of
2026-09-21**: 73 files, including the 324 MB two-day track file. **The 2026-09-22 ladder sessions
are not yet copied** and exist only on the card.

```bash
scp -r KnurLogger:KnurLoggerData/sessions/*.ndjson /c/_claude/KnurLoggerData/sessions/
```

**Three things to know before reading one.** A session filename is stamped from the box's wall
clock, which in the car is hours wrong — see `session-records.md`. And
`taiUs` can jump mid-file when `timesyncd` corrects the clock; `bootUs` and `sessionUs` are the
axes that survive it. And **copy a session only once the logger has stopped writing it** — a
mid-write copy is a truncated file that parses perfectly and says the session ended early. One
lived in the backup that way until it was replaced; `systemctl is-active KnurLogger` is the check.


## Operating constraints

1. The logger is fed from constant 12 V. It remains powered through engine-off periods and
   cranking. `KnurLogger.service` must start it unattended; remove power at the end of use.
   Prefer `sudo poweroff` before pulling the fuse to avoid repeated abrupt SD-card shutdowns.
2. Do not treat battery-draw estimates as measurements. No draw measurement is scheduled;
   leaving the fuse fitted for an extended parking period can flatten the battery.
3. Before running the binary manually, run `sudo systemctl stop KnurLogger`. There is no
   single-instance guard: concurrent instances can both advertise and poll the probe bus,
   corrupting timing observations and making the phone's selected instance ambiguous.
4. Restart log mode after enrollment; bindings are loaded at worker start and are not re-read.
   Session names include the mode, and binding writes use temp-file, rename and directory fsync.
5. Build-tree and production configurations are separate, as described above. Never point the
   service or another production launcher at the build tree. Back up the production configuration
   after enrollment or calibration, and merge required new keys before deploying new code.
6. The car has no internet. A preconfigured phone hotspot can provide local Wi-Fi and SSH.
   Prepare its NetworkManager profile before the trip; do not commit the PSK. Enrollment and
   logging must also operate unattended after power-on, with BLE providing phone-visible feedback.
7. Bluetooth stays enabled. Wi-Fi gating is manual, only when diagnosing BLE link problems:
   use `nmcli radio wifi off` or `rfkill block wifi`, never `rfkill block all` or a permanent
   `disable-wifi` overlay. Restore Wi-Fi after diagnosis to collect artifacts.
8. Keep `wpa_supplicant.service`: NetworkManager drives it over D-Bus. Disabling it can lose the
   only access path. Host services, module setup and tool paths belong to
   [SystemSetup/pi-headless-setup.md](SystemSetup/pi-headless-setup.md).
9. Run SystemSetup scripts in pre-flight mode before `--execute`. Mutations require a terminal
   for sudo. Do not run `ssh-harden.sh` under sudo; it must read the login user's authorized keys.
10. `harden-headless.sh` masks retired units so package upgrades cannot silently re-enable them.
    Roll back using `sudo systemctl unmask <unit>`. For units without `[Install]`, issue disable,
    stop and mask separately: `disable --now` can refuse before stopping anything.
11. Session filenames and wall clocks can be wrong offline. Follow [session-records.md](session-records.md)
    for time alignment and validity; record the physical mounting state separately for each run.
12. CAN ambient and fan state arrive through box 1 and RaceChrono, not this logger's SD card.
    Define the phone channels before recording. A BLE gap can lose the alignment needed to interpret
    box-2 data against those inputs; local logging does not reconstruct an undefined CAN channel.

Electrical qualification is retired by owner decision; do not rebuild it from the old bring-up
checklist. The fuse remains fitted, harness routing and logged supply telemetry remain required.
See [ndLouvers risk 14](../ndLouvers/risk-register.md) and plan history rev 105.
