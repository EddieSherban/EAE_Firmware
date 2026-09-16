// state_machine.cpp

#include "state_machine.h"

SystemState StateMachine_NextState(SystemState current_state,
                                    bool ignition_on,
                                    bool safety_fault_active,
                                    bool cooldown_active)
{
    switch (current_state)
    {
        case STATE_INIT:
            return STATE_STANDBY;

        case STATE_STANDBY:
            if (ignition_on)
            {
                return safety_fault_active ? STATE_FAULT : STATE_RUNNING;
            }
            return STATE_STANDBY;

        case STATE_RUNNING:
            if (safety_fault_active)
            {
                return STATE_FAULT;
            }
            if (!ignition_on)
            {
                return STATE_COOLDOWN;
            }
            return STATE_RUNNING;

        case STATE_COOLDOWN:
            if (ignition_on)
            {
                return safety_fault_active ? STATE_FAULT : STATE_RUNNING;
            }
            if (safety_fault_active)
            {
                return STATE_FAULT;
            }
            if (!cooldown_active)
            {
                return STATE_STANDBY;
            }
            return STATE_COOLDOWN;

        case STATE_FAULT:
            if (!ignition_on)
            {
                return STATE_STANDBY;
            }
            return STATE_FAULT;

        default:
            // Should not happen with a valid enum value; fail safe
            // rather than let an unrecognized state fall through.
            return STATE_FAULT;
    }
}
