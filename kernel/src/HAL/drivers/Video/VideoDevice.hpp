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

#ifndef _VIDEO_DEVICE_HPP
#define _VIDEO_DEVICE_HPP

#include <stdint.h>

#include <DataStructures/AVLTree.hpp>

#include <Graphics/Colour.hpp>

#include "../Device.hpp"

namespace Video {

    class Display;
    class FBDisplay;

    class VideoDevice : public Device {
    public:
        VideoDevice();
        virtual ~VideoDevice();

        virtual int Init() = 0; // Early init
        virtual int Init(DeviceManager::DeviceInfo* info) override = 0; // Full init

        virtual FBDisplay* GetFBDisplay(); // Get the FBDisplay, not required to be implemented

        void SetInfo(DeviceManager::DeviceInfo* info);
        DeviceManager::DeviceInfo* GetInfo();

        // Assign the display an ID, and add it to the list and fs
        int AddDisplay(Display* display);
        Display* GetDisplay(uint64_t id);
        int RemoveDisplay(uint64_t id);

        uint64_t GetID();
        void SetID(uint64_t id);

    private:
        DeviceManager::DeviceInfo* m_info;
        uint64_t m_id;

        uint64_t m_nextDisplayID;
        AVLTree::wAVLTree<uint64_t, Display*> m_displays;
    };

    // Assign the VideoDevice an ID and add to the list and fs
    int RegisterVideoDevice(VideoDevice* device);

    // Setup the new FB console. Creates a generic FB device using the boot framebuffer if no devices exist.
    int Video_FullInit(FBConsole** newConsole);

}

#endif /* _VIDEO_DEVICE_HPP */