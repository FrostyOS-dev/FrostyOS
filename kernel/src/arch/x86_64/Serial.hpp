/*
Copyright (©) 2026  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _x86_64_SERIAL_HPP
#define _x86_64_SERIAL_HPP

#include <stdint.h>

#include <DataStructures/Buffer.hpp>

#include <HAL/drivers/SerialDevice.hpp>

#include <Scheduling/Semaphore.hpp>

#define SERIAL_RX_BUFFER_SIZE 1024

#define SERIAL_BASE_BAUD 115200

#define SERIAL_DEFAULT_BAUD 38400
#define SERIAL_DEFAULT_DIVISOR (SERIAL_BASE_BAUD / SERIAL_DEFAULT_BAUD)

struct x86_64_SerialIRQ;

class x86_64_SerialPort : public SerialDevice {
public:
    x86_64_SerialPort(uint8_t ID);
    ~x86_64_SerialPort();

    int Init() override;

    void SetDivisor(uint16_t divisor);

    void SetIOBase(uint16_t base);
    bool SetIRQ(void* irq);

    void HandleIRQ();

    bool ReadByte(uint8_t& out, bool block = true) override;
    bool WriteByte(uint8_t byte, bool block = true) override;

    void EnableLoopback() override;
    void DisableLoopback() override;
    bool isLoopbackEnabled() const override;
    
private:
    bool HasData();
    bool CanTransmit();
    void ProcessData();

    void WritePort(uint8_t offset, uint8_t value);
    uint8_t ReadPort(uint8_t offset);

    uint8_t m_id;
    uint16_t m_ioBase;
    x86_64_SerialIRQ* m_irq;
    
    uint16_t m_currentDivisor;

    RingBuffer<uint8_t, SERIAL_RX_BUFFER_SIZE> m_rxBuffer;
    Semaphore m_rxSemaphore;

    bool m_loopback;
};

int x86_64_InitSerial();

#endif /* _x86_64_SERIAL_HPP */