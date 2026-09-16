// cooling_controller.h
//
// Ties the state machine and PID controller together into the
// cooling loop's control logic for the Section 5 loop (pump ->
// orifice -> filter -> radiator/fan -> temp sensor -> inverter |
// DC-DC in parallel -> reservoir w/ level switch -> pump).
//
// Same discipline as the Section 7 submission: plain structs and
// free functions, no dynamic allocation, no STL containers, no
// exceptions, so this is representative of code that could run
// unmodified on the target embedded controller. Only main.cpp (the
// demo harness) and the GTest test files use host-side conveniences.

#ifndef COOLING_CONTROLLER_H
#define COOLING_CONTROLLER_H

#include "state_machine.h"
#include "pid.h"

// Tunable setpoints and gains. Populated at startup from CLI
// arguments (see main.cpp) and may also be updated at runtime, e.g.
// from a received CAN setpoint-update frame.
struct CoolingConfig
{
    float target_temp_c;      // PID setpoint: desired coolant supply temp
    float critical_temp_c;    // Over-temperature trip point (latched fault)
    float sensor_min_valid_c; // Below this, the sensor reading is untrusted
    float sensor_max_valid_c; // Above this, the sensor reading is untrusted

    unsigned int pump_cooldown_ticks; // Post-ignition-off pump run time, in scan cycles

    float pid_kp;
    float pid_ki;
    float pid_kd;
};

// Fixed scan period assumed for the PID's integral/derivative terms.
// A real PLC would use its actual scan time here.
#define SCAN_PERIOD_SECONDS 1.0f

// How far below critical_temp_c the reading must fall before the
// over-temperature condition is allowed to clear (hysteresis on the
// fault detection itself, independent of the state machine's own
// ignition-cycle latch).
#define OVER_TEMP_CLEAR_MARGIN_C 5.0f

struct CoolingFaults
{
    bool sensor_out_of_range;
    bool over_temperature;
    bool low_coolant;
};

struct CoolingInputs
{
    float coolant_temp_c;
    bool  ignition_on;
    bool  coolant_level_ok;
};

struct CoolingOutputs
{
    bool        pump_on;
    float       fan_duty_percent;  // 0.0 - 100.0
    bool        derate_request;    // To inverter/DC-DC: reduce power
    bool        system_fault;      // Any fault flag set (for HMI/telemetry)
    SystemState state;             // Current state, for telemetry/CAN broadcast
};

struct CoolingControllerState
{
    SystemState state;
    unsigned int cooldown_ticks_remaining;
    CoolingFaults faults;
    PidController pid;

    // Cached so output-building code can read them without
    // recomputing; also what actually feeds CoolingOutputs each cycle.
    bool  pump_on;
    float fan_duty_percent;
};

// Call once at startup with the tuned config to initialize state.
void CoolingController_Init(CoolingControllerState* state, const CoolingConfig* config);

// Call once per scan cycle.
void CoolingController_Update(CoolingControllerState* state,
                               const CoolingConfig* config,
                               const CoolingInputs* inputs,
                               CoolingOutputs* outputs);

#endif // COOLING_CONTROLLER_H
