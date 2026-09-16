// can_bus.cpp

#include "can_bus.h"
#include <cstring>

void CanBus_Init(CanBus* bus)
{
    bus->head = 0;
    bus->tail = 0;
    bus->count = 0;
    // Frame contents are left uninitialized until written by Send;
    // Receive only ever reads slots that Send has populated, since
    // count tracks exactly how many valid frames are queued.
}

bool CanBus_IsEmpty(const CanBus* bus)
{
    return bus->count == 0;
}

bool CanBus_IsFull(const CanBus* bus)
{
    return bus->count == CAN_BUS_CAPACITY;
}

bool CanBus_Send(CanBus* bus, const CanFrame* frame)
{
    if (CanBus_IsFull(bus))
    {
        return false;
    }

    bus->frames[bus->tail] = *frame;
    bus->tail = (bus->tail + 1) % CAN_BUS_CAPACITY;
    bus->count++;
    return true;
}

bool CanBus_Receive(CanBus* bus, CanFrame* out_frame)
{
    if (CanBus_IsEmpty(bus))
    {
        return false;
    }

    *out_frame = bus->frames[bus->head];
    bus->head = (bus->head + 1) % CAN_BUS_CAPACITY;
    bus->count--;
    return true;
}
