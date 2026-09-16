// state_machine.h
//
// The cooling system's operating states and the transition table
// between them. Kept as a separate, side-effect-free module (the
// transition function takes plain values in and returns the next
// state, with no pointers or mutation) so it can be read and unit
// tested independently of the PID/CAN/sensor code around it.

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

enum SystemState
{
    STATE_INIT = 0,   // Power-up, one scan cycle before normal operation
    STATE_STANDBY,    // Ignition off (or not yet on): pump and fan off
    STATE_RUNNING,    // Ignition on, no fault: PID controls the fan
    STATE_COOLDOWN,   // Ignition just went off: pump keeps circulating briefly
    STATE_FAULT       // Sensor or over-temp fault: forced safe outputs
};

// Pure transition function: given the current state and this cycle's
// relevant conditions, returns the next state. No side effects.
//
//   ignition_on         - ignition switch input, this cycle
//   safety_fault_active - true if a sensor-out-of-range or
//                          over-temperature condition is active
//                          this cycle (NOT low coolant -- see
//                          cooling_controller.h for why that fault
//                          is handled separately and does not affect
//                          the state machine)
//   cooldown_active      - true if the post-run cooldown timer still
//                          has ticks remaining
//
// Transition summary:
//   INIT     -> STANDBY                                  (always, one-shot)
//   STANDBY  -> RUNNING   if ignition on and no fault
//   STANDBY  -> FAULT     if ignition on and a fault is already present
//                          (refuses to start with a known fault)
//   RUNNING  -> FAULT     if a fault becomes active
//   RUNNING  -> COOLDOWN  if ignition turns off
//   COOLDOWN -> RUNNING   if ignition turns back on and no fault
//   COOLDOWN -> FAULT     if ignition turns back on with a fault, or
//                          a fault appears while still cooling down
//   COOLDOWN -> STANDBY   once the cooldown timer runs out
//   FAULT    -> STANDBY   only when ignition is off (latched: a fault
//                          does not clear itself just because the
//                          reading that caused it recovers -- it
//                          requires the ignition to be cycled)
SystemState StateMachine_NextState(SystemState current_state,
                                    bool ignition_on,
                                    bool safety_fault_active,
                                    bool cooldown_active);

#endif // STATE_MACHINE_H
