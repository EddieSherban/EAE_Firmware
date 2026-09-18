// pid.cpp

#include "pid.h"

void Pid_Init(PidController* pid,
              float kp, float ki, float kd,
              float output_min, float output_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->output_min = output_min;
    pid->output_max = output_max;
    Pid_Reset(pid);
}

void Pid_Reset(PidController* pid)
{
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}

float Pid_Update(PidController* pid, float setpoint, float measurement, float dt_seconds)
{
    float error = measurement - setpoint;

    float derivative = 0.0f;
    if (dt_seconds > 0.0f)
    {
        derivative = (error - pid->prev_error) / dt_seconds;
    }

    // Tentative output using the integral term as it stands before
    // this cycle's accumulation, to decide whether integrating
    // further would just push us deeper into saturation.
    float trial_output = (pid->kp * error) + (pid->ki * pid->integral) + (pid->kd * derivative);

    // Conditional integration (simple anti-windup): only accumulate
    // more integral error while doing so wouldn't drive the output
    // further past a limit it has already reached.
    bool pinned_high = (trial_output >= pid->output_max) && (error > 0.0f);
    bool pinned_low  = (trial_output <= pid->output_min) && (error < 0.0f);

    if (!pinned_high && !pinned_low)
    {
        pid->integral += error * dt_seconds;
    }

    float output = (pid->kp * error) + (pid->ki * pid->integral) + (pid->kd * derivative);

    if (output > pid->output_max)
    {
        output = pid->output_max;
    }
    if (output < pid->output_min)
    {
        output = pid->output_min;
    }

    pid->prev_error = error;

    return output;
}
