// cooling_controller.cpp

#include "cooling_controller.h"

void CoolingController_Init(CoolingControllerState* state, const CoolingConfig* config)
{
    state->state = STATE_INIT;
    state->cooldown_ticks_remaining = 0;

    state->faults.sensor_out_of_range = false;
    state->faults.over_temperature = false;
    state->faults.low_coolant = false;

    Pid_Init(&state->pid, config->pid_kp, config->pid_ki, config->pid_kd, 0.0f, 100.0f);

    state->pump_on = false;
    state->fan_duty_percent = 0.0f;
}

void CoolingController_Update(CoolingControllerState* state,
                               const CoolingConfig* config,
                               const CoolingInputs* inputs,
                               CoolingOutputs* outputs)
{
    // ---- 1. Sensor validation ----
    bool sensor_valid = (inputs->coolant_temp_c >= config->sensor_min_valid_c) &&
                         (inputs->coolant_temp_c <= config->sensor_max_valid_c);
    state->faults.sensor_out_of_range = !sensor_valid;

    // ---- 2. Over-temperature fault (only meaningful if the sensor
    //         reading itself is trustworthy) ----
    if (sensor_valid)
    {
        if (!state->faults.over_temperature)
        {
            state->faults.over_temperature = (inputs->coolant_temp_c >= config->critical_temp_c);
        }
        else if (inputs->coolant_temp_c <= (config->critical_temp_c - OVER_TEMP_CLEAR_MARGIN_C))
        {
            state->faults.over_temperature = false;
        }
    }
    // If the sensor is invalid, leave over_temperature at whatever it
    // last was -- we cannot evaluate it meaningfully either way, and
    // sensor_out_of_range already forces a fault state on its own.

    // ---- 3. Low coolant (level switch). Independent of the state
    //         machine -- see cooling_controller.h / state_machine.h
    //         for why this is a "derate" condition rather than one
    //         that forces STATE_FAULT: running the pump against a
    //         confirmed-empty reservoir risks damaging the pump, so
    //         forcing the same "keep everything running at max"
    //         response used for a sensor/over-temp fault is not
    //         obviously the safer choice here. Flagged for review
    //         against the actual pump's dry-run rating. ----
    state->faults.low_coolant = !inputs->coolant_level_ok;

    // ---- 4. State machine ----
    bool safety_fault_active = state->faults.sensor_out_of_range || state->faults.over_temperature;
    bool cooldown_active_before = (state->cooldown_ticks_remaining > 0);

    SystemState next_state = StateMachine_NextState(state->state,
                                                      inputs->ignition_on,
                                                      safety_fault_active,
                                                      cooldown_active_before);

    // Cooldown timer side effects: always fully re-armed while
    // actively running (so it starts fresh whenever ignition next
    // goes off), and ticked down once per cycle while cooling down.
    if (next_state == STATE_RUNNING)
    {
        state->cooldown_ticks_remaining = config->pump_cooldown_ticks;
    }
    else if (next_state == STATE_COOLDOWN)
    {
        if (state->cooldown_ticks_remaining > 0)
        {
            state->cooldown_ticks_remaining--;
        }
    }

    // ---- 5. Outputs per state ----
    switch (next_state)
    {
        case STATE_INIT:
        case STATE_STANDBY:
            state->pump_on = false;
            state->fan_duty_percent = 0.0f;
            // Keep the PID primed at zero so RUNNING starts without a bump.
            Pid_Reset(&state->pid);
            break;

        case STATE_RUNNING:
        case STATE_COOLDOWN:
            state->pump_on = true;
            state->fan_duty_percent = Pid_Update(&state->pid,
                                                  config->target_temp_c,
                                                  inputs->coolant_temp_c,
                                                  SCAN_PERIOD_SECONDS);
            break;

        case STATE_FAULT:
            state->pump_on = true;         // keep circulating through the fault
            state->fan_duty_percent = 100.0f; // full cooling effort, PID bypassed
            Pid_Reset(&state->pid);         // avoid windup while forced
            break;
    }

    state->state = next_state;

    // ---- 6. Build outputs ----
    outputs->pump_on = state->pump_on;
    outputs->fan_duty_percent = state->fan_duty_percent;
    outputs->state = state->state;
    outputs->derate_request = (state->state == STATE_FAULT) || state->faults.low_coolant;

    // Deliberately includes (state->state == STATE_FAULT) explicitly,
    // not just the raw fault flags: the FAULT state is latched (see
    // state_machine.h) and can persist after sensor_out_of_range /
    // over_temperature have themselves cleared. Without this, the
    // fault indicator could read "ok" while the system is still
    // sitting in FAULT waiting on an ignition cycle -- which would be
    // a misleading status to show an operator or an HMI.
    outputs->system_fault = (state->state == STATE_FAULT) ||
                             state->faults.sensor_out_of_range ||
                             state->faults.over_temperature ||
                             state->faults.low_coolant;
}
