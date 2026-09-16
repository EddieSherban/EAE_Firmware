// test_state_machine.cpp

#include <gtest/gtest.h>
#include "core/state_machine.h"

TEST(StateMachine, InitAlwaysGoesToStandby)
{
    EXPECT_EQ(StateMachine_NextState(STATE_INIT, true, true, true), STATE_STANDBY);
    EXPECT_EQ(StateMachine_NextState(STATE_INIT, false, false, false), STATE_STANDBY);
}

TEST(StateMachine, StandbyGoesToRunningOnIgnitionWithNoFault)
{
    EXPECT_EQ(StateMachine_NextState(STATE_STANDBY, true, false, false), STATE_RUNNING);
}

TEST(StateMachine, StandbyRefusesToStartWithAFaultPresent)
{
    EXPECT_EQ(StateMachine_NextState(STATE_STANDBY, true, true, false), STATE_FAULT);
}

TEST(StateMachine, StandbyStaysWhileIgnitionOff)
{
    EXPECT_EQ(StateMachine_NextState(STATE_STANDBY, false, false, false), STATE_STANDBY);
}

TEST(StateMachine, RunningTripsToFaultWhenFaultBecomesActive)
{
    EXPECT_EQ(StateMachine_NextState(STATE_RUNNING, true, true, false), STATE_FAULT);
}

TEST(StateMachine, RunningGoesToCooldownWhenIgnitionTurnsOff)
{
    EXPECT_EQ(StateMachine_NextState(STATE_RUNNING, false, false, false), STATE_COOLDOWN);
}

TEST(StateMachine, RunningStaysRunningWithIgnitionOnAndNoFault)
{
    EXPECT_EQ(StateMachine_NextState(STATE_RUNNING, true, false, false), STATE_RUNNING);
}

TEST(StateMachine, CooldownReturnsToRunningIfIgnitionComesBackOnWithNoFault)
{
    EXPECT_EQ(StateMachine_NextState(STATE_COOLDOWN, true, false, true), STATE_RUNNING);
}

TEST(StateMachine, CooldownGoesToFaultIfIgnitionComesBackOnWithFault)
{
    EXPECT_EQ(StateMachine_NextState(STATE_COOLDOWN, true, true, true), STATE_FAULT);
}

TEST(StateMachine, CooldownGoesToFaultIfFaultAppearsWhileCoolingDown)
{
    EXPECT_EQ(StateMachine_NextState(STATE_COOLDOWN, false, true, true), STATE_FAULT);
}

TEST(StateMachine, CooldownGoesToStandbyOnceTimerExpires)
{
    EXPECT_EQ(StateMachine_NextState(STATE_COOLDOWN, false, false, false), STATE_STANDBY);
}

TEST(StateMachine, CooldownContinuesWhileTimerStillRunning)
{
    EXPECT_EQ(StateMachine_NextState(STATE_COOLDOWN, false, false, true), STATE_COOLDOWN);
}

TEST(StateMachine, FaultStaysLatchedWhileIgnitionRemainsOnEvenIfFaultClears)
{
    // Fault condition itself has cleared (safety_fault_active = false)
    // but ignition is still on: the state machine's own latch keeps
    // it in FAULT regardless.
    EXPECT_EQ(StateMachine_NextState(STATE_FAULT, true, false, false), STATE_FAULT);
}

TEST(StateMachine, FaultClearsToStandbyOnceIgnitionGoesOff)
{
    EXPECT_EQ(StateMachine_NextState(STATE_FAULT, false, true, false), STATE_STANDBY);
    EXPECT_EQ(StateMachine_NextState(STATE_FAULT, false, false, false), STATE_STANDBY);
}
