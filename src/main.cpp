// main.cpp
//
// Demo harness only -- not part of the embedded logic. Parses CLI
// setpoints, drives the cooling controller through a scripted
// sequence of emulated field readings (one "scan cycle" per loop
// iteration), and exercises the simulated CAN bus in both
// directions: sending a status frame out each cycle, and receiving a
// setpoint-update frame partway through as if a diagnostic tool had
// written one onto the bus.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/cooling_controller.h"
#include "core/can_bus.h"
#include "core/can_protocol.h"

static const char* StateName(SystemState state)
{
    switch (state)
    {
        case STATE_INIT:     return "INIT";
        case STATE_STANDBY:  return "STANDBY";
        case STATE_RUNNING:  return "RUNNING";
        case STATE_COOLDOWN: return "COOLDOWN";
        case STATE_FAULT:    return "FAULT";
        default:              return "UNKNOWN";
    }
}

// Parses "--flag <value>" at argv[index]. Returns false (leaving
// *out_value untouched) if there's no following token or it isn't
// numeric.
static bool ParseFloatArg(int argc, char** argv, int index, float* out_value)
{
    if (index + 1 >= argc)
    {
        return false;
    }
    char* endptr = 0;
    double parsed = strtod(argv[index + 1], &endptr);
    if (endptr == argv[index + 1])
    {
        return false;
    }
    *out_value = (float)parsed;
    return true;
}

int main(int argc, char** argv)
{
    float target_temp_c = 50.0f;
    float critical_temp_c = 75.0f;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--target-temp") == 0)
        {
            float value;
            if (!ParseFloatArg(argc, argv, i, &value))
            {
                fprintf(stderr, "error: --target-temp requires a numeric value\n");
                return 1;
            }
            target_temp_c = value;
            ++i;
        }
        else if (strcmp(argv[i], "--critical-temp") == 0)
        {
            float value;
            if (!ParseFloatArg(argc, argv, i, &value))
            {
                fprintf(stderr, "error: --critical-temp requires a numeric value\n");
                return 1;
            }
            critical_temp_c = value;
            ++i;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            printf("usage: firmware_demo [--target-temp C] [--critical-temp C]\n");
            printf("  --target-temp    PID setpoint for coolant supply temp (default 50.0)\n");
            printf("  --critical-temp  Over-temperature trip point (default 75.0)\n");
            return 0;
        }
        else
        {
            fprintf(stderr, "error: unknown argument '%s' (try --help)\n", argv[i]);
            return 1;
        }
    }

    CoolingConfig config;
    config.target_temp_c = target_temp_c;
    config.critical_temp_c = critical_temp_c;
    config.sensor_min_valid_c = -20.0f;
    config.sensor_max_valid_c = 120.0f;
    config.pump_cooldown_ticks = 5;
    config.pid_kp = 6.0f;
    config.pid_ki = 0.8f;
    config.pid_kd = 0.0f;

    printf("startup: target_temp=%.1fC critical_temp=%.1fC\n\n", config.target_temp_c, config.critical_temp_c);

    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CanBus bus;
    CanBus_Init(&bus);

    // Scripted emulated field data, one entry per scan cycle. Walks
    // the controller through: boot delay, normal PID ramp-up, a
    // mid-run CAN setpoint change, a sensor fault that blocks restart
    // until ignition is cycled, a critical over-temp that likewise
    // requires an ignition cycle to clear even after the reading
    // recovers, a low-coolant warning that does NOT stop the system,
    // and a normal shutdown into cooldown and back to standby.
    const int NUM_TICKS = 26;

    const float temp_readings[NUM_TICKS] =
    {
        25.0f, 35.0f, 45.0f, 55.0f, 999.0f, 999.0f, 999.0f, 999.0f,
        60.0f, 60.0f, 45.0f, 55.0f, 78.0f, 60.0f, 40.0f, 40.0f,
        50.0f, 50.0f, 50.0f, 50.0f, 45.0f, 40.0f, 38.0f, 36.0f,
        35.0f, 35.0f
    };

    const bool ignition_readings[NUM_TICKS] =
    {
        true, true, true, true, true, true, false, true,
        true, false, true, true, true, true, false, true,
        true, true, true, false, false, false, false, false,
        false, false
    };

    const bool level_ok_readings[NUM_TICKS] =
    {
        true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true,
        true, false, true, true, true, true, true, true,
        true, true
    };

    printf("tick  temp_C   ign  lvl  | state     pump  fan%%   derate  fault\n");
    printf("---------------------------------------------------------------\n");

    for (int i = 0; i < NUM_TICKS; ++i)
    {
        // Simulate a diagnostic tool writing a new setpoint onto the
        // bus partway through the run.
        if (i == 2)
        {
            CanFrame setpoint_frame;
            CanProtocol_EncodeSetpoint(55.0f, &setpoint_frame);
            CanBus_Send(&bus, &setpoint_frame);
            printf("      [CAN] setpoint-update frame queued (target -> 55.0C)\n");
        }

        // Drain any pending inbound frames before running this
        // cycle's control logic, same as a PLC would process its
        // receive mailbox at the start of a scan.
        CanFrame received_frame;
        while (CanBus_Receive(&bus, &received_frame))
        {
            float new_target;
            if (CanProtocol_DecodeSetpoint(&received_frame, &new_target))
            {
                config.target_temp_c = new_target;
                printf("      [CAN] received setpoint update: target_temp=%.1fC\n", new_target);
            }
        }

        CoolingInputs inputs;
        inputs.coolant_temp_c = temp_readings[i];
        inputs.ignition_on = ignition_readings[i];
        inputs.coolant_level_ok = level_ok_readings[i];

        CoolingOutputs outputs;
        CoolingController_Update(&state, &config, &inputs, &outputs);

        // Broadcast the status for other ECUs / diagnostics to see.
        CanFrame status_frame;
        CanProtocol_EncodeStatus(&inputs, &outputs, &status_frame);
        CanBus_Send(&bus, &status_frame);

        printf("%4d  %6.1f   %3s  %3s  | %-8s  %4s  %5.1f  %6s  %s\n",
               i,
               inputs.coolant_temp_c,
               inputs.ignition_on ? "ON" : "off",
               inputs.coolant_level_ok ? "ok" : "LOW",
               StateName(outputs.state),
               outputs.pump_on ? "ON" : "off",
               outputs.fan_duty_percent,
               outputs.derate_request ? "YES" : "no",
               outputs.system_fault ? "FAULT" : "ok");

        // on the very first cycle, prove the status frame we just
        // queued round-trips through the bus and decodes back to the
        // same values, as a sanity check on the CAN encode/decode path.
        if (i == 0)
        {
            CanFrame check_frame;
            if (CanBus_Receive(&bus, &check_frame))
            {
                float decoded_temp;
                unsigned char decoded_fan;
                bool decoded_pump, decoded_derate, decoded_fault;
                SystemState decoded_state;
                CanProtocol_DecodeStatus(&check_frame, &decoded_temp, &decoded_fan,
                                          &decoded_pump, &decoded_derate, &decoded_fault, &decoded_state);
                printf("      [CAN] round-trip check: decoded temp=%.1fC fan=%u%% pump=%s state=%s\n",
                       decoded_temp, decoded_fan, decoded_pump ? "on" : "off", StateName(decoded_state));
            }
        }
    }

    return 0;
}
