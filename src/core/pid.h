// pid.h
//
// Minimal fixed-point-friendly PID controller. Plain struct + free
// functions, no dynamic allocation, no STL. Designed to be called
// once per fixed-period scan cycle.

#ifndef PID_H
#define PID_H

struct PidController
{
    float kp;
    float ki;
    float kd;

    float integral;     // Accumulated integral term
    float prev_error;   // Error from the previous cycle, for the derivative term

    float output_min;
    float output_max;
};

// Initialize gains and output clamp, and zero the running state.
void Pid_Init(PidController* pid,
              float kp, float ki, float kd,
              float output_min, float output_max);

// Clear the running state (integral and previous error) without
// touching the gains or output limits. Call this whenever control is
// handed back to the PID after a period where it was bypassed (e.g.
// leaving a forced fault state), so the integrator doesn't "wake up"
// already wound up and cause a bump in output.
void Pid_Reset(PidController* pid);

// Run one control cycle. setpoint and measurement are in the same
// units (deg C here); dt_seconds is the fixed scan period. Returns
// the clamped output.
//
// error is defined as (measurement - setpoint), so a temperature
// above the setpoint produces a positive error and therefore a
// positive (more cooling) output.
float Pid_Update(PidController* pid, float setpoint, float measurement, float dt_seconds);

#endif // PID_H
