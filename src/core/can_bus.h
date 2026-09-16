// can_bus.h
//
// A software stand-in for a physical CAN bus: a fixed-capacity FIFO
// queue of frames. This does not touch real hardware/sockets -- it
// models the send/receive queueing behavior so the rest of the
// firmware can be exercised without a real CAN transceiver.
//
// Fixed-size array, no dynamic allocation, no STL containers.

#ifndef CAN_BUS_H
#define CAN_BUS_H

#define CAN_BUS_CAPACITY 16
#define CAN_FRAME_DATA_BYTES 8

struct CanFrame
{
    unsigned int id;                          // CAN identifier
    unsigned char dlc;                        // Number of valid bytes in data (0-8)
    unsigned char data[CAN_FRAME_DATA_BYTES];  // Payload
};

struct CanBus
{
    CanFrame frames[CAN_BUS_CAPACITY];
    int head;   // Index of the oldest queued frame
    int tail;   // Index where the next frame will be written
    int count;  // Number of frames currently queued
};

void CanBus_Init(CanBus* bus);

// Queues a frame. Returns false (and queues nothing) if the bus is
// already full.
bool CanBus_Send(CanBus* bus, const CanFrame* frame);

// Dequeues the oldest frame into *out_frame. Returns false (and
// leaves *out_frame untouched) if the bus is empty.
bool CanBus_Receive(CanBus* bus, CanFrame* out_frame);

bool CanBus_IsEmpty(const CanBus* bus);
bool CanBus_IsFull(const CanBus* bus);

#endif // CAN_BUS_H
