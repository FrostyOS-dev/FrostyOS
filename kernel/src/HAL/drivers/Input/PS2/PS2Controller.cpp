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

#include "PS2Controller.hpp"

#include <bit>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <HAL/HAL.hpp>

#ifdef __x86_64__
#include <arch/x86_64/8042PS2Controller.hpp>
#endif

/* Define all the commands */

#define PS2_CMD_READ_CONFIG_BYTE 0x20
#define PS2_CMD_WRITE_CONFIG_BYTE 0x60
#define PS2_CMD_DISABLE_PORT_1 0xAD
#define PS2_CMD_DISABLE_PORT_2 0xA7
#define PS2_CMD_ENABLE_PORT_1 0xAE
#define PS2_CMD_ENABLE_PORT_2 0xA8
#define PS2_CMD_TEST_PORT_1 0xAB
#define PS2_CMD_TEST_PORT_2 0xA9
#define PS2_CMD_SELF_TEST 0xAA
#define PS2_CMD_TEST_CONTROLLER 0xA9
#define PS2_CMD_WRITE_PORT_1 0xD1
#define PS2_CMD_WRITE_PORT_2 0xD3
#define PS2_CMD_READ_PORT_1 0xD0
#define PS2_CMD_READ_PORT_2 0xD2
#define PS2_CMD_WRITE_PORT_2_INPUT 0xD4

/* Define the generic device commands */

#define PS2_DEVICE_CMD_ECHO 0xEE
#define PS2_DEVICE_CMD_IDENTIFY 0xF2
#define PS2_DEVICE_CMD_ENABLE_SCANNING 0xF4
#define PS2_DEVICE_CMD_DISABLE_SCANNING 0xF5
#define PS2_DEVICE_CMD_RESET_ENABLE 0xF6
#define PS2_DEVICE_CMD_ALL_MAKE_RELEASE 0xF8
#define PS2_DEVICE_CMD_ALL_MAKE 0xF9
#define PS2_DEVICE_CMD_SINGLE_MAKE_RELEASE 0xFC
#define PS2_DEVICE_CMD_SINGLE_BREAK 0xFD
#define PS2_DEVICE_CMD_RESEND 0xFE
#define PS2_DEVICE_CMD_RESET 0xFF

/* Define the keyboard responses */

#define PS2_DEVICE_RESPONSE_ACK 0xFA
#define PS2_DEVICE_RESPONSE_ECHO 0xEE
#define PS2_DEVICE_RESPONSE_RESEND 0xFE
#define PS2_DEVICE_RESPONSE_RESET 0xAA

#define PS2_RESPONSE_SELF_TEST_PASS 0x55

struct [[gnu::packed]] PS2Config {
    bool port1IRQEnabled : 1;
    bool port2IRQEnabled : 1;
    bool systemFlag : 1;
    bool unused0 : 1;
    bool port1ClockDisabled : 1;
    bool port2ClockDisabled : 1;
    bool port1TranslationEnabled : 1;
    bool unused1 : 1;
};

struct [[gnu::packed]] PS2Status {
    bool outputFull : 1;
    bool inputFull : 1;
    bool systemFlag : 1;
    bool controllerData : 1;
    bool unknown0 : 1;
    bool dataFromPort2 : 1;
    uint8_t unknown1 : 2;
};

PS2Controller::PS2Controller() {

}

PS2Controller::~PS2Controller() {

}

int PS2Controller::Init() {
    // Disable the controller
    int rc = SendCommand(PS2_CMD_DISABLE_PORT_1);
    if (rc < 0)
        return rc;
    printf("Port 1 disabled\n");
    rc = SendCommand(PS2_CMD_DISABLE_PORT_2);
    if (rc < 0)
        return rc;

    printf("Ports disabled!\n");

    // Flush the output buffer
    Flush();

    uint8_t data = 0;

    // Set the controller configuration byte
    rc = SendCommand(PS2_CMD_READ_CONFIG_BYTE);
    if (rc < 0)
        return rc;
    rc = ReadData(&data);
    if (rc < 0)
        return rc;
    PS2Config config = std::bit_cast<PS2Config>(data);
    config.port1IRQEnabled = false;
    config.port2IRQEnabled = false;
    config.port1TranslationEnabled = false;

    bool isDualChannel = config.port2ClockDisabled;

    rc = SendCommand(PS2_CMD_WRITE_CONFIG_BYTE);
    if (rc < 0)
        return rc;
    rc = WriteData(std::bit_cast<uint8_t>(config));
    if (rc < 0)
        return rc;

    printf("Config wrote\n");

    // Test the controller
    rc = SendCommand(PS2_CMD_SELF_TEST);
    if (rc < 0)
        return rc;
    rc = ReadData(&data);
    if (rc < 0)
        return rc;
    if (data != PS2_RESPONSE_SELF_TEST_PASS) {
        printf("Self test failed! Received: %x\n", data);
        return -ENODEV;
    }

    printf("Self test passed\n");

    // write the config again
    rc = SendCommand(PS2_CMD_WRITE_CONFIG_BYTE);
    if (rc < 0)
        return rc;
    rc = WriteData(std::bit_cast<uint8_t>(config));
    if (rc < 0)
        return rc;

    printf("Config re-wrote\n");

    // check if it is actually dual channel
    if (isDualChannel) {
        printf("Checking for dual channel\n");
        rc = SendCommand(PS2_CMD_ENABLE_PORT_2);
        if (rc < 0)
            return rc;
        rc = SendCommand(PS2_CMD_READ_CONFIG_BYTE);
        if (rc < 0)
            return rc;
        rc = ReadData(&data);
        if (rc < 0)
            return rc;
        PS2Config config = std::bit_cast<PS2Config>(data);
        isDualChannel = !config.port2ClockDisabled;

        if (isDualChannel) {
            rc = SendCommand(PS2_CMD_DISABLE_PORT_2);
            if (rc < 0)
                return rc;
        }
    }

    printf("Testing channels\n");

    // test channels

    bool channel1Works = false;
    bool channel2Works = false;

    for (int i = 0; i < (isDualChannel ? 2 : 1); i++) {
        rc = SendCommand(i == 0 ? PS2_CMD_TEST_PORT_1 : PS2_CMD_TEST_PORT_2);
        if (rc < 0)
            return rc;
        rc = ReadData(&data);
        if (rc < 0)
            return rc;

        if (data == 0x00) {
            if (i == 0)
                channel1Works = true;
            else
                channel2Works = true;
        }
    }

    printf("channel1: %s, channel2: %s\n", channel1Works ? "true" : "false", channel2Works ? "true" : "false");

    

    return 0;
}

int PS2Controller::Destroy() {
    return 0;
}

int PS2Controller::InitialiseDevice(bool channel) { // false = 1, true = 2
    int rc = SendCommand(channel ? PS2_CMD_ENABLE_PORT_2 : PS2_CMD_ENABLE_PORT_1);
    if (rc < 0)
        return rc;

    rc = SendCommandToDevice(channel, PS2_DEVICE_CMD_DISABLE_SCANNING, false);
    if (rc < 0)
        return rc;

    while (true) {
        uint8_t out = 0;
        rc = ReadData(&out, 100);
        if (rc < 0 || out == PS2_DEVICE_RESPONSE_RESEND)
            break;
    }
    Flush();

    rc = SendCommandToDevice(channel, PS2_DEVICE_CMD_RESET, false);
    if (rc < 0)
        return rc;

    while (true) {
        
    }
}

int PS2Controller::SendCommandToDevice(bool channel, uint8_t command, bool expectAck) {
    uint64_t resendCounter = 3;
    int rc = 0;
    bool resend;

    do {
        if (channel) {
            rc = SendCommand(PS2_CMD_WRITE_PORT_2_INPUT);
            if (rc < 0)
                return rc;
        }

        rc = WriteData(command);
        if (rc < 0)
            return rc;

        resendCounter--;
        rc = ShouldResend();
        if (rc < 0)
            return rc;
        resend = rc > 0;
    } while (expectAck && resendCounter > 0 && resend);

    if (expectAck && resendCounter == 0)
        return -ETIME;

    return ESUCCESS;
}

int PS2Controller::ShouldResend() {
    while (true) {
        uint8_t data = 0;
        int rc = ReadData(&data);
        if (rc < 0)
            return rc;

        if (data == PS2_DEVICE_RESPONSE_ACK)
            return false;

        if (data == PS2_DEVICE_RESPONSE_RESEND)
            return true;
    }
}

#ifdef __x86_64__

int PS2Controller::SendCommand(uint8_t command) {
    size_t attempts = 100000;

    while (std::bit_cast<PS2Status>(GetStatus()).inputFull && --attempts)
        PAUSE();

    if (attempts == 0)
        return -ETIME;

    x86_64_8042_WriteCommand(command);
    return 0;
}

int PS2Controller::ReadData(uint8_t* out, size_t attempts) {
    while (!std::bit_cast<PS2Status>(GetStatus()).outputFull && --attempts)
        PAUSE();

    if (attempts == 0)
        return -ETIME;

    *out = x86_64_8042_ReadData();
    return 0;
}

int PS2Controller::WriteData(uint8_t data) {
    size_t attempts = 100000;

    while (std::bit_cast<PS2Status>(GetStatus()).inputFull && --attempts)
        PAUSE();

    if (attempts == 0)
        return -ETIME;

    x86_64_8042_WriteData(data);
    return 0;
}

uint8_t PS2Controller::GetStatus() {
    return x86_64_8042_ReadStatusRegister_Raw();
}

void PS2Controller::Flush() {
    while(std::bit_cast<PS2Status>(GetStatus()).outputFull)
        x86_64_8042_ReadData();
}

#endif /* __x86_64__ */


PS2Controller* g_PS2Controller = nullptr;
