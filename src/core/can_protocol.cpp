// can_protocol.cpp

#include "can_protocol.h"

// Pack a signed 16-bit value into two bytes, big-endian.
static void EncodeInt16BE(int value, unsigned char* out_bytes)
{
    out_bytes[0] = (unsigned char)((value >> 8) & 0xFF);
    out_bytes[1] = (unsigned char)(value & 0xFF);
}

// Unpack a big-endian signed 16-bit value from two bytes.
static int DecodeInt16BE(const unsigned char* bytes)
{
    int value = (int)((unsigned short)((bytes[0] << 8) | bytes[1]));
    if (value > 32767)
    {
        value -= 65536; // sign-extend
    }
    return value;
}

void CanProtocol_EncodeStatus(const CoolingInputs* inputs,
                               const CoolingOutputs* outputs,
                               CanFrame* out_frame)
{
    out_frame->id = CAN_ID_COOLING_STATUS;
    out_frame->dlc = 5;

    int temp_scaled = (int)(inputs->coolant_temp_c * 10.0f);
    EncodeInt16BE(temp_scaled, &out_frame->data[0]);

    unsigned char fan_duty = (unsigned char)outputs->fan_duty_percent;
    if (fan_duty > 100)
    {
        fan_duty = 100;
    }
    out_frame->data[2] = fan_duty;

    unsigned char flags = 0;
    if (outputs->pump_on)        flags |= 0x01;
    if (outputs->derate_request) flags |= 0x02;
    if (outputs->system_fault)   flags |= 0x04;
    out_frame->data[3] = flags;

    out_frame->data[4] = (unsigned char)outputs->state;

    out_frame->data[5] = 0;
    out_frame->data[6] = 0;
    out_frame->data[7] = 0;
}

bool CanProtocol_DecodeStatus(const CanFrame* frame,
                               float* out_temp_c,
                               unsigned char* out_fan_duty_percent,
                               bool* out_pump_on,
                               bool* out_derate_request,
                               bool* out_system_fault,
                               SystemState* out_state)
{
    if (frame->id != CAN_ID_COOLING_STATUS)
    {
        return false;
    }

    int temp_scaled = DecodeInt16BE(&frame->data[0]);
    *out_temp_c = (float)temp_scaled / 10.0f;

    *out_fan_duty_percent = frame->data[2];

    unsigned char flags = frame->data[3];
    *out_pump_on = (flags & 0x01) != 0;
    *out_derate_request = (flags & 0x02) != 0;
    *out_system_fault = (flags & 0x04) != 0;

    *out_state = (SystemState)frame->data[4];

    return true;
}

void CanProtocol_EncodeSetpoint(float target_temp_c, CanFrame* out_frame)
{
    out_frame->id = CAN_ID_SETPOINT_UPDATE;
    out_frame->dlc = 2;

    int temp_scaled = (int)(target_temp_c * 10.0f);
    EncodeInt16BE(temp_scaled, &out_frame->data[0]);

    for (int i = 2; i < CAN_FRAME_DATA_BYTES; ++i)
    {
        out_frame->data[i] = 0;
    }
}

bool CanProtocol_DecodeSetpoint(const CanFrame* frame, float* out_target_temp_c)
{
    if (frame->id != CAN_ID_SETPOINT_UPDATE)
    {
        return false;
    }

    int temp_scaled = DecodeInt16BE(&frame->data[0]);
    *out_target_temp_c = (float)temp_scaled / 10.0f;
    return true;
}
