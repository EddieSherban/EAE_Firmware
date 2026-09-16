// test_cooling_controller.cpp

#include <gtest/gtest.h>
#include "core/cooling_controller.h"

namespace
{
    CoolingConfig MakeTestConfig()
    {
        CoolingConfig config;
        config.target_temp_c = 50.0f;
        config.critical_temp_c = 75.0f;
        config.sensor_min_valid_c = -20.0f;
        config.sensor_max_valid_c = 120.0f;
        config.pump_cooldown_ticks = 3;
        config.pid_kp = 6.0f;
        config.pid_ki = 0.0f; // no integral term, keeps single-cycle tests predictable
        config.pid_kd = 0.0f;
        return config;
    }

    CoolingInputs MakeInputs(float temp, bool ignition, bool level_ok)
    {
        CoolingInputs inputs;
        inputs.coolant_temp_c = temp;
        inputs.ignition_on = ignition;
        inputs.coolant_level_ok = level_ok;
        return inputs;
    }
}

TEST(CoolingController, FirstCycleGoesToStandbyRegardlessOfIgnition)
{
    CoolingConfig config = MakeTestConfig();
    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CoolingInputs inputs = MakeInputs(25.0f, true, true);
    CoolingOutputs outputs;
    CoolingController_Update(&state, &config, &inputs, &outputs);

    EXPECT_EQ(outputs.state, STATE_STANDBY);
    EXPECT_FALSE(outputs.pump_on);
    EXPECT_FLOAT_EQ(outputs.fan_duty_percent, 0.0f);
}

TEST(CoolingController, IgnitionOnStartsThePumpAndPidControlsTheFan)
{
    CoolingConfig config = MakeTestConfig();
    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CoolingInputs inputs = MakeInputs(60.0f, true, true); // 10C above target
    CoolingOutputs outputs;

    CoolingController_Update(&state, &config, &inputs, &outputs); // INIT -> STANDBY
    CoolingController_Update(&state, &config, &inputs, &outputs); // STANDBY -> RUNNING

    EXPECT_EQ(outputs.state, STATE_RUNNING);
    EXPECT_TRUE(outputs.pump_on);
    EXPECT_FLOAT_EQ(outputs.fan_duty_percent, 60.0f); // kp(6) * error(10)
}

TEST(CoolingController, SensorOutOfRangeForcesFaultWithFanAtMax)
{
    CoolingConfig config = MakeTestConfig();
    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CoolingInputs bad_inputs = MakeInputs(500.0f, true, true); // outside valid range
    CoolingOutputs outputs;

    CoolingController_Update(&state, &config, &bad_inputs, &outputs); // INIT -> STANDBY
    CoolingController_Update(&state, &config, &bad_inputs, &outputs); // STANDBY -> FAULT (blocked start)

    EXPECT_EQ(outputs.state, STATE_FAULT);
    EXPECT_TRUE(outputs.pump_on);
    EXPECT_FLOAT_EQ(outputs.fan_duty_percent, 100.0f);
    EXPECT_TRUE(outputs.derate_request);
    EXPECT_TRUE(outputs.system_fault);
}

TEST(CoolingController, FaultStaysLatchedUntilIgnitionIsCycledEvenAfterSensorRecovers)
{
    CoolingConfig config = MakeTestConfig();
    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CoolingInputs bad_inputs = MakeInputs(500.0f, true, true);
    CoolingOutputs outputs;
    CoolingController_Update(&state, &config, &bad_inputs, &outputs); // INIT -> STANDBY
    CoolingController_Update(&state, &config, &bad_inputs, &outputs); // STANDBY -> FAULT

    // Sensor reading recovers, but ignition stays on.
    CoolingInputs good_inputs = MakeInputs(50.0f, true, true);
    CoolingController_Update(&state, &config, &good_inputs, &outputs);
    EXPECT_EQ(outputs.state, STATE_FAULT); // still latched

    // Ignition cycles off: fault clears to STANDBY.
    CoolingInputs ignition_off = MakeInputs(50.0f, false, true);
    CoolingController_Update(&state, &config, &ignition_off, &outputs);
    EXPECT_EQ(outputs.state, STATE_STANDBY);
    EXPECT_FALSE(outputs.pump_on);

    // Ignition back on with a good reading: runs normally.
    CoolingController_Update(&state, &config, &good_inputs, &outputs);
    EXPECT_EQ(outputs.state, STATE_RUNNING);
}

TEST(CoolingController, LowCoolantAssertsDerateWithoutForcingFaultState)
{
    CoolingConfig config = MakeTestConfig();
    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CoolingInputs inputs = MakeInputs(50.0f, true, true);
    CoolingOutputs outputs;
    CoolingController_Update(&state, &config, &inputs, &outputs); // INIT -> STANDBY
    CoolingController_Update(&state, &config, &inputs, &outputs); // STANDBY -> RUNNING

    CoolingInputs low_coolant_inputs = MakeInputs(50.0f, true, false);
    CoolingController_Update(&state, &config, &low_coolant_inputs, &outputs);

    EXPECT_EQ(outputs.state, STATE_RUNNING); // not forced to FAULT
    EXPECT_TRUE(outputs.pump_on);            // pump not stopped
    EXPECT_TRUE(outputs.derate_request);     // but derate is still requested
}

TEST(CoolingController, IgnitionOffEntersCooldownThenReturnsToStandbyAfterTimer)
{
    CoolingConfig config = MakeTestConfig(); // pump_cooldown_ticks = 3
    CoolingControllerState state;
    CoolingController_Init(&state, &config);

    CoolingInputs running_inputs = MakeInputs(50.0f, true, true);
    CoolingOutputs outputs;
    CoolingController_Update(&state, &config, &running_inputs, &outputs); // INIT -> STANDBY
    CoolingController_Update(&state, &config, &running_inputs, &outputs); // STANDBY -> RUNNING

    CoolingInputs ignition_off = MakeInputs(50.0f, false, true);

    CoolingController_Update(&state, &config, &ignition_off, &outputs); // -> COOLDOWN (tick 1/3)
    EXPECT_EQ(outputs.state, STATE_COOLDOWN);
    EXPECT_TRUE(outputs.pump_on);

    CoolingController_Update(&state, &config, &ignition_off, &outputs); // tick 2/3
    EXPECT_EQ(outputs.state, STATE_COOLDOWN);

    CoolingController_Update(&state, &config, &ignition_off, &outputs); // tick 3/3
    EXPECT_EQ(outputs.state, STATE_COOLDOWN);

    CoolingController_Update(&state, &config, &ignition_off, &outputs); // timer expired
    EXPECT_EQ(outputs.state, STATE_STANDBY);
    EXPECT_FALSE(outputs.pump_on);
}
