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

#include "IO.h"
#include "Serial.hpp"

#include "interrupts/IRQ.hpp"
#include "interrupts/ISR.hpp"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <HAL/drivers/SerialDevice.hpp>

#include <tty/TTY.hpp>

#include <tty/backends/SerialBackend.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <uacpi/resources.h>
#include <uacpi/status.h>
#include <uacpi/types.h>
#include <uacpi/utilities.h>

#pragma GCC diagnostic pop

#define PORT_RX 0
#define PORT_TX 0
#define PORT_INTERRUPT_ENABLE 1
#define PORT_DIVISOR_LOW 0
#define PORT_DIVISOR_HIGH 0
#define PORT_INTERRUPT_ID 2
#define PORT_FIFO_CONTROL 2
#define PORT_LINE_CONTROL 3
#define PORT_MODEM_CONTROL 4
#define PORT_LINE_STATUS 5
#define PORT_MODEM_STATUS 6

#define LINE_CONTROL_CHARLEN_5     0x00
#define LINE_CONTROL_CHARLEN_6     0x01
#define LINE_CONTROL_CHARLEN_7     0x02
#define LINE_CONTROL_CHARLEN_8     0x03
#define LINE_CONTROL_STOP_BITS     0x04
#define LINE_CONTROL_PARITY_NONE   0x00
#define LINE_CONTROL_PARITY_ODD    0x08
#define LINE_CONTROL_PARITY_EVEN   0x18
#define LINE_CONTROL_PARITY_MARK   0x28
#define LINE_CONTROL_PARITY_SPACE  0x38
#define LINE_CONTROL_BREAK_ENABLE  0x40
#define LINE_CONTROL_DIVISOR_LATCH 0x80

#define INTERRUPT_ENABLE_NONE 0
#define INTERRUPT_ENABLE_DATA_AVAILABLE 1
#define INTERRUPT_ENABLE_TX_EMPTY 2

#define MODEM_CONTROL_DTR 1 /* Data Terminal Ready */
#define MODEM_CONTROL_RTS 2 /* Request to Send */
#define MODEM_CONTROL_OUT1 4
#define MODEM_CONTROL_OUT2 8

#define LINE_STATUS_DATA_READY 1
#define LINE_STATUS_TX_EMPT 0x20

uint8_t g_x86_64_SerialPortID = 0;

struct x86_64_SerialIRQ {
    bool edgeTrigger;
    uint8_t polarity; // 0 = high, 1 = low, 2 = both
    bool shared;
    bool wakeCapable;
    uint8_t irq0;
};

void x86_64_SerialIRQHandler(x86_64_ISR_Frame* frame, void* ctx) {
    x86_64_SerialPort* port = static_cast<x86_64_SerialPort*>(ctx);
    port->HandleIRQ();
}

x86_64_SerialPort::x86_64_SerialPort(uint8_t id) : m_id(id), m_ioBase(0), m_irq(nullptr), m_currentDivisor(0), m_loopback(true) {

}

x86_64_SerialPort::~x86_64_SerialPort() {
    if (m_irq != nullptr)
        delete m_irq;
}

int x86_64_SerialPort::Init() {
    if (m_irq == nullptr)
        return -ENODEV;

    WritePort(PORT_INTERRUPT_ENABLE, INTERRUPT_ENABLE_NONE);
    SetDivisor(SERIAL_DEFAULT_DIVISOR);
    WritePort(PORT_LINE_CONTROL, LINE_CONTROL_CHARLEN_8);
    WritePort(PORT_INTERRUPT_ENABLE, INTERRUPT_ENABLE_DATA_AVAILABLE);
    WritePort(PORT_MODEM_CONTROL, MODEM_CONTROL_DTR | MODEM_CONTROL_RTS | MODEM_CONTROL_OUT1 | MODEM_CONTROL_OUT2);

    if (!x86_64_RegisterGSIHandler(m_irq->irq0, x86_64_SerialIRQHandler, this)) {
        WritePort(PORT_INTERRUPT_ENABLE, INTERRUPT_ENABLE_NONE);
        return -ENODEV;
    }

    x86_64_UnmaskGSI(m_irq->irq0);

    printf("COM%d: %d baud, 8 bit charlen, 1 stop bit, no parity\n", m_id, SERIAL_DEFAULT_BAUD);

    if (m_id == 0)
        g_defaultSerialDevice = this;

    return ESUCCESS;
}

void x86_64_SerialPort::SetDivisor(uint16_t divisor) {
    uint8_t old = ReadPort(PORT_LINE_CONTROL);
    WritePort(PORT_LINE_CONTROL, LINE_CONTROL_DIVISOR_LATCH);

    WritePort(PORT_DIVISOR_LOW, divisor & 0xff);
    WritePort(PORT_DIVISOR_HIGH, (divisor >> 8) & 0xff);

    WritePort(PORT_LINE_CONTROL, old);

    m_currentDivisor = divisor;
}

void x86_64_SerialPort::SetIOBase(uint16_t base) {
    m_ioBase = base;
}

bool x86_64_SerialPort::SetIRQ(void* irq) {
    uacpi_resource_irq* res = static_cast<uacpi_resource_irq*>(irq);
    if (res->num_irqs != 1)
        return false;
    m_irq = new x86_64_SerialIRQ;
    m_irq->edgeTrigger = res->triggering == UACPI_TRIGGERING_EDGE;
    m_irq->polarity = res->polarity;
    m_irq->shared = res->sharing == UACPI_SHARED;
    m_irq->wakeCapable = res->wake_capability == UACPI_WAKE_CAPABLE;
    m_irq->irq0 = res->irqs[0];
    return true;
}

void x86_64_SerialPort::HandleIRQ() {
    ProcessData();
}

bool x86_64_SerialPort::ReadByte(uint8_t& out, bool block) {
    bool rc = m_rxBuffer.pop(out);
    if (rc || !block)
        return rc;
    do {
        m_rxSemaphore.Wait();
        rc = m_rxBuffer.pop(out);
    } while (!rc);
    return rc;
}

bool x86_64_SerialPort::WriteByte(uint8_t byte, bool block) {
    while (!CanTransmit()) {
        if (!block)
            return false;
        PAUSE();
    }
    WritePort(PORT_TX, byte);
    return true;
}

void x86_64_SerialPort::EnableLoopback() {
    m_loopback = true;
}

void x86_64_SerialPort::DisableLoopback() {
    m_loopback = false;
}

bool x86_64_SerialPort::isLoopbackEnabled() const {
    return m_loopback;
}

bool x86_64_SerialPort::HasData() {
    return (ReadPort(PORT_LINE_STATUS) & LINE_STATUS_DATA_READY) > 0;
}

bool x86_64_SerialPort::CanTransmit() {
    return (ReadPort(PORT_LINE_STATUS) & LINE_STATUS_TX_EMPT) > 0;
}

void x86_64_SerialPort::ProcessData() {
    while (HasData()) {
        uint8_t c = ReadPort(PORT_RX);
        if (m_rxBuffer.push(c))
            m_rxSemaphore.Signal();
        if (m_loopback) {
            WriteByte(c);
            g_CurrentTTY->Write((char*)&c, 1, true);
            if (c == 0x08) { // backspace
                WriteByte(' ');
                WriteByte(c);
            }
        }
    }
}

void x86_64_SerialPort::WritePort(uint8_t offset, uint8_t value) {
    x86_64_outb(m_ioBase + offset, value);
}

uint8_t x86_64_SerialPort::ReadPort(uint8_t offset) {
    return x86_64_inb(m_ioBase + offset);
}

uacpi_iteration_decision x86_64_SetSerialResource(void* data, uacpi_resource* resource) {
    x86_64_SerialPort* port = static_cast<x86_64_SerialPort*>(data);
    switch (resource->type) {
    case UACPI_RESOURCE_TYPE_IRQ:
        port->SetIRQ(&resource->irq);
        break;
    case UACPI_RESOURCE_TYPE_IO:
        port->SetIOBase(resource->io.minimum);
        break;
    default:
        break;
    }
    
    return UACPI_ITERATION_DECISION_CONTINUE;
}

uacpi_iteration_decision x86_64_InitSerialPort(void*, uacpi_namespace_node* node, unsigned int) {
    uacpi_resources* res;

    x86_64_SerialPort* port = new x86_64_SerialPort(g_x86_64_SerialPortID);
    g_x86_64_SerialPortID++;

    uacpi_status rc = uacpi_get_current_resources(node, &res);
    if (uacpi_unlikely_error(rc)) {
        delete port;
        return UACPI_ITERATION_DECISION_NEXT_PEER;
    }

    rc = uacpi_for_each_resource(res, x86_64_SetSerialResource, port);
    if (uacpi_unlikely_error(rc)) {
        delete port;
        return UACPI_ITERATION_DECISION_NEXT_PEER;
    }

    uacpi_free_resources(res);

    if (port->Init() < 0)
        delete port;

    return UACPI_ITERATION_DECISION_CONTINUE;
}

int x86_64_InitSerial() {
    uacpi_status rc = uacpi_find_devices("PNP0500", x86_64_InitSerialPort, nullptr);
    if (uacpi_unlikely_error(rc))
        return -ENODEV;

    rc = uacpi_find_devices("PNP0501", x86_64_InitSerialPort, nullptr);
    if (uacpi_unlikely_error(rc))
        return -ENODEV;

     if (g_defaultSerialDevice != nullptr) {
        TTYBackendSerial* serialBackend = new TTYBackendSerial(g_defaultSerialDevice);
        g_CurrentTTY->SetInputBackend(serialBackend);
    }

    return ESUCCESS;
}
