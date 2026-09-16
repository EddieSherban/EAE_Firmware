// can_protocol.h
//
// Frame formats for this firmware's two CAN messages, and the
// encode/decode functions between them and the controller's plain
// structs. Manual bit/byte packing -- no serialization library.

#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include "can_bus.h"
#include "cooling_controller.h"

#define CAN_ID_COOLING_STATUS  0x100u
#define CAN_ID_SETPOINT_UPDATE 0x200u

// CAN_ID_COOLING_STATUS payload layout (broadcast by this firmware
// once per scan cycle so other ECUs / a diagnostic tool can observe
// the cooling loop's state):
//   byte 0-1 : coolant temp, signed 16-bit, big-endian, 0.1 C / bit
//   byte 2   : fan duty percent, 0-100
//   byte 3   : bit0 = pump_on, bit1 = derate_request, bit2 = system_fault
//   byte 4   : system state (SystemState enum value)
void CanProtocol_EncodeStatus(const CoolingInputs* inputs,
                               const CoolingOutputs* outputs,
                               CanFrame* out_frame);

// Unpack a CAN_ID_COOLING_STATUS frame. Returns false if the frame's
// id doesn't match (nothing is written to the outputs in that case).
bool CanProtocol_DecodeStatus(const CanFrame* frame,
                               float* out_temp_c,
                               unsigned char* out_fan_duty_percent,
                               bool* out_pump_on,
                               bool* out_derate_request,
                               bool* out_system_fault,
                               SystemState* out_state);

// CAN_ID_SETPOINT_UPDATE payload layout (as if sent by a diagnostic
// tool or another ECU to change the PID target temperature at
// runtime):
//   byte 0-1 : target temp, signed 16-bit, big-endian, 0.1 C / bit
void CanProtocol_EncodeSetpoint(float target_temp_c, CanFrame* out_frame);

// Returns false if the frame's id doesn't match CAN_ID_SETPOINT_UPDATE.
bool CanProtocol_DecodeSetpoint(const CanFrame* frame, float* out_target_temp_c);

#endif // CAN_PROTOCOL_H
