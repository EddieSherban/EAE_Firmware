// test_pid.cpp

#include <gtest/gtest.h>
#include "core/pid.h"

TEST(Pid, ZeroErrorGivesZeroOutputFromRest)
{
    PidController pid;
    Pid_Init(&pid, 6.0f, 0.8f, 0.0f, 0.0f, 100.0f);

    float output = Pid_Update(&pid, 50.0f, 50.0f, 1.0f);

    EXPECT_FLOAT_EQ(output, 0.0f);
}

TEST(Pid, PositiveErrorIncreasesOutput)
{
    PidController pid;
    Pid_Init(&pid, 6.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    // Measurement above setpoint -> positive error -> more output.
    float output = Pid_Update(&pid, 50.0f, 60.0f, 1.0f);

    EXPECT_FLOAT_EQ(output, 60.0f); // kp * error = 6.0 * 10.0
}

TEST(Pid, NegativeErrorClampsAtOutputMin)
{
    PidController pid;
    Pid_Init(&pid, 6.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    // Measurement well below setpoint would give a large negative
    // output; it must clamp to output_min, not go negative.
    float output = Pid_Update(&pid, 50.0f, 10.0f, 1.0f);

    EXPECT_FLOAT_EQ(output, 0.0f);
}

TEST(Pid, OutputClampsAtOutputMax)
{
    PidController pid;
    Pid_Init(&pid, 6.0f, 0.0f, 0.0f, 0.0f, 100.0f);

    // error of 50 * kp(6) = 300, must clamp to 100.
    float output = Pid_Update(&pid, 50.0f, 100.0f, 1.0f);

    EXPECT_FLOAT_EQ(output, 100.0f);
}

TEST(Pid, IntegralAccumulatesOverRepeatedCycles)
{
    PidController pid;
    Pid_Init(&pid, 0.0f, 1.0f, 0.0f, 0.0f, 100.0f);

    // Constant error of 5 for 3 cycles of 1 second: integral should
    // accumulate to 15, and with ki=1 that's the output too.
    float output = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        output = Pid_Update(&pid, 50.0f, 55.0f, 1.0f);
    }

    EXPECT_FLOAT_EQ(output, 15.0f);
}

TEST(Pid, AntiWindupStopsIntegratingWhilePinnedHigh)
{
    PidController pid;
    // Large error with only integral gain: output pins at max
    // immediately, so continued cycles must not keep growing the
    // integral term without bound.
    Pid_Init(&pid, 0.0f, 50.0f, 0.0f, 0.0f, 100.0f);

    Pid_Update(&pid, 0.0f, 100.0f, 1.0f); // error 100, would give huge output
    float integral_after_one = pid.integral;

    Pid_Update(&pid, 0.0f, 100.0f, 1.0f); // still pinned, should not integrate further
    float integral_after_two = pid.integral;

    EXPECT_FLOAT_EQ(integral_after_one, integral_after_two);
}

TEST(Pid, ResetClearsIntegralAndPreviousError)
{
    PidController pid;
    Pid_Init(&pid, 0.0f, 1.0f, 0.0f, 0.0f, 100.0f);

    Pid_Update(&pid, 50.0f, 55.0f, 1.0f);
    ASSERT_NE(pid.integral, 0.0f);

    Pid_Reset(&pid);

    EXPECT_FLOAT_EQ(pid.integral, 0.0f);
    EXPECT_FLOAT_EQ(pid.prev_error, 0.0f);
}
