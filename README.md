# EAE_Firmware -- Cooling loop controller (firmware)

Section 7.1 deliverable. Extends the Section 7 (`EAE_Coding`) cooling
loop controller with the optional firmware requirements: a PID loop,
a formal state machine, simulated CANBUS send/receive, CLI setpoints,
a CMake build with an externally fetched dependency, and GTest unit
tests.

This is the same design lineage as `EAE_Coding`, not a divergent
rewrite: same inputs (temp sensor, ignition switch, level switch),
same outputs (pump, fan, a derate signal to the inverter/DC-DC), same
embedded-appropriate style. What's different is called out below.

## Build & run

Requires a C++ compiler, CMake >= 3.14, and network access the first
time (to fetch googletest). Tested with g++ 13 / CMake 3.28 on Linux;
also builds under MSYS2 (mingw-w64 g++ + cmake).

```
./build.sh
```

This configures, builds, runs the unit tests via ctest, then runs the
demo. To pass setpoints through to the demo:

```
./build.sh --target-temp 55 --critical-temp 80
```

Or manually:

```
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/firmware_demo --target-temp 55 --critical-temp 80
```

## Layout

- `src/core/` -- the embedded-style control logic: `pid`,
  `state_machine`, `can_bus`, `can_protocol`, `cooling_controller`.
  Plain structs and free functions, no dynamic allocation, no STL
  containers, no exceptions -- written as if it were going to run
  unmodified on the target controller.
- `src/main.cpp` -- demo harness. Parses CLI setpoints, drives the
  controller through a scripted 26-cycle scenario of emulated sensor
  data, and exercises the simulated CAN bus in both directions.
- `tests/` -- GTest unit tests for the PID, the state machine, the
  CAN bus/protocol, and the integrated controller. GTest itself uses
  the STL internally; that's the test framework, not the logic under
  test.
- `CMakeLists.txt` -- builds `cooling_core` as a static library
  shared by the demo and the tests, fetches googletest via
  `FetchContent` (nothing vendored in this repo), and statically
  links our own binaries' C++/GCC runtime (`-static-libgcc
  -static-libstdc++`) so nothing extra needs to be shipped alongside
  them.

## What changed from `EAE_Coding`, and why

- **Fan control is now a PID loop**, not staged on/off hysteresis.
  The fan duty (0-100%) is driven continuously off the error between
  the coolant supply temperature and a target setpoint, with
  conditional-integration anti-windup so the integral term doesn't
  wind up while the output is already pinned at a limit.
- **The state machine is now explicit and formalized**
  (`INIT / STANDBY / RUNNING / COOLDOWN / FAULT`, see
  `state_machine.h` for the full transition table), rather than
  logic that just directly computes outputs each cycle. Two
  behaviors worth calling out because they're a deliberate tightening
  versus the simpler Section 7 version:
  - **A fault now requires the ignition to be cycled off before it
    clears**, even if the condition that caused it (an over-temp
    reading, say) has since recovered. Section 7's version
    auto-cleared once the temperature dropped back below a hysteresis
    margin. This version treats that as a flickering-fault risk and
    requires an explicit reset instead.
  - **The system refuses to enter RUNNING if a fault is already
    present at ignition-on** (goes straight to FAULT instead), rather
    than only checking for faults once already running.
- **Low coolant still does not force the FAULT state or stop the
  pump** -- same reasoning as Section 7: forcing max response for a
  confirmed-empty reservoir could mean running the pump dry, which
  may be worse than the condition it's reacting to. It still asserts
  `derate_request` so the inverter/DC-DC can back off. This trade-off
  is still flagged as something to confirm against the pump's actual
  dry-run rating before trusting it on real hardware.
- **CANBUS is simulated**, not real hardware I/O: `can_bus.h/.cpp` is
  a fixed-capacity FIFO frame queue standing in for the bus, and
  `can_protocol.h/.cpp` packs/unpacks the two frame types this
  firmware uses (a status broadcast, and a setpoint-update it can
  receive). The demo sends a status frame every cycle and, partway
  through, simulates receiving a setpoint-update frame as if a
  diagnostic tool had written one onto the bus -- demonstrating both
  directions.
- **Setpoints are now CLI arguments** (`--target-temp`,
  `--critical-temp`) instead of compiled-in constants.

All thresholds and PID gains are placeholder values for demonstration
and would need real tuning against the actual radiator/pump/fan
hardware and the inverter/DC-DC thermal limits before use.
