// test_can_bus.cpp

#include <gtest/gtest.h>
#include "core/can_bus.h"
#include "core/can_protocol.h"

TEST(CanBus, ReceiveOnEmptyBusFails)
{
    CanBus bus;
    CanBus_Init(&bus);

    CanFrame frame;
    EXPECT_FALSE(CanBus_Receive(&bus, &frame));
}

TEST(CanBus, SendThenReceiveRoundTripsSameFrame)
{
    CanBus bus;
    CanBus_Init(&bus);

    CanFrame sent;
    sent.id = 0x123;
    sent.dlc = 3;
    sent.data[0] = 0xAA;
    sent.data[1] = 0xBB;
    sent.data[2] = 0xCC;

    ASSERT_TRUE(CanBus_Send(&bus, &sent));

    CanFrame received;
    ASSERT_TRUE(CanBus_Receive(&bus, &received));

    EXPECT_EQ(received.id, sent.id);
    EXPECT_EQ(received.dlc, sent.dlc);
    EXPECT_EQ(received.data[0], sent.data[0]);
    EXPECT_EQ(received.data[1], sent.data[1]);
    EXPECT_EQ(received.data[2], sent.data[2]);
}

TEST(CanBus, IsFifoOrdered)
{
    CanBus bus;
    CanBus_Init(&bus);

    CanFrame a; a.id = 1; a.dlc = 0;
    CanFrame b; b.id = 2; b.dlc = 0;

    ASSERT_TRUE(CanBus_Send(&bus, &a));
    ASSERT_TRUE(CanBus_Send(&bus, &b));

    CanFrame out;
    ASSERT_TRUE(CanBus_Receive(&bus, &out));
    EXPECT_EQ(out.id, 1u);
    ASSERT_TRUE(CanBus_Receive(&bus, &out));
    EXPECT_EQ(out.id, 2u);
}

TEST(CanBus, SendFailsWhenBusIsFull)
{
    CanBus bus;
    CanBus_Init(&bus);

    CanFrame frame; frame.id = 0; frame.dlc = 0;

    for (int i = 0; i < CAN_BUS_CAPACITY; ++i)
    {
        ASSERT_TRUE(CanBus_Send(&bus, &frame));
    }

    EXPECT_TRUE(CanBus_IsFull(&bus));
    EXPECT_FALSE(CanBus_Send(&bus, &frame));
}

TEST(CanProtocol, SetpointEncodeDecodeRoundTrips)
{
    CanFrame frame;
    CanProtocol_EncodeSetpoint(55.5f, &frame);

    EXPECT_EQ(frame.id, (unsigned int)CAN_ID_SETPOINT_UPDATE);

    float decoded_temp;
    ASSERT_TRUE(CanProtocol_DecodeSetpoint(&frame, &decoded_temp));
    EXPECT_NEAR(decoded_temp, 55.5f, 0.05f);
}

TEST(CanProtocol, DecodeSetpointFailsOnWrongId)
{
    CanFrame frame;
    frame.id = CAN_ID_COOLING_STATUS; // wrong id for this decode
    frame.dlc = 2;
    frame.data[0] = 0;
    frame.data[1] = 0;

    float decoded_temp;
    EXPECT_FALSE(CanProtocol_DecodeSetpoint(&frame, &decoded_temp));
}

TEST(CanProtocol, StatusEncodeDecodeRoundTrips)
{
    CoolingInputs inputs;
    inputs.coolant_temp_c = 62.3f;
    inputs.ignition_on = true;
    inputs.coolant_level_ok = true;

    CoolingOutputs outputs;
    outputs.pump_on = true;
    outputs.fan_duty_percent = 77.0f;
    outputs.derate_request = false;
    outputs.system_fault = false;
    outputs.state = STATE_RUNNING;

    CanFrame frame;
    CanProtocol_EncodeStatus(&inputs, &outputs, &frame);

    float decoded_temp;
    unsigned char decoded_fan;
    bool decoded_pump, decoded_derate, decoded_fault;
    SystemState decoded_state;

    ASSERT_TRUE(CanProtocol_DecodeStatus(&frame, &decoded_temp, &decoded_fan,
                                          &decoded_pump, &decoded_derate, &decoded_fault, &decoded_state));

    EXPECT_NEAR(decoded_temp, 62.3f, 0.05f);
    EXPECT_EQ(decoded_fan, 77);
    EXPECT_TRUE(decoded_pump);
    EXPECT_FALSE(decoded_derate);
    EXPECT_FALSE(decoded_fault);
    EXPECT_EQ(decoded_state, STATE_RUNNING);
}
