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

#include "Display.hpp"
#include "VideoDevice.hpp"
#include "VideoFS.hpp"

#include "../DeviceManager.hpp"

#include "FBVideo/FBDisplay.hpp"
#include "FBVideo/FBVideoDevice.hpp"

#include <atomic>

#include <DataStructures/AVLTree.hpp>

#include <fs/DevTempFS/DevTempFS.hpp>

#include <Graphics/Framebuffer.hpp>

#include <tty/FBConsole.hpp>

namespace Video {

    std::atomic<uint64_t> g_lastVideoDeviceID;
    AVLTree::wAVLTree<uint64_t, VideoDevice*> g_videoDevices;

    VideoDevice::VideoDevice() : m_info(nullptr), m_nextDisplayID(0) {

    }

    VideoDevice::~VideoDevice() {

    }

    FBDisplay* VideoDevice::GetFBDisplay() {
        return nullptr;
    }

    void VideoDevice::SetInfo(DeviceManager::DeviceInfo* info) {
        m_info = info;
    }

    DeviceManager::DeviceInfo* VideoDevice::GetInfo() {
        return m_info;
    }

    // Add the display to the list, fs and assign it an ID
    int VideoDevice::AddDisplay(Display* display) {
        if (m_info == nullptr || m_info->manager == nullptr)
            return -ENODEV;

        m_displays.lock();
        uint64_t id = m_nextDisplayID;
        display->SetID(id);
        m_nextDisplayID++;
        m_displays.unlock();

        char name[32];
        snprintf(name, 32, "display%lu", id);

        DisplayVNode* vnode = display->CreateVNode(m_info->manager->GetDevFS());
        if (vnode == nullptr)
            return -ENOMEM;

        return m_info->manager->CreateSubFile(m_info, ".", name, false, vnode);
    }

    Display* VideoDevice::GetDisplay(uint64_t id) {
        m_displays.lock();
        Display* display = m_displays.Find(id);
        m_displays.unlock();
        return display;
    }

    int VideoDevice::RemoveDisplay(uint64_t id) {
        m_displays.lock();
        AVLTree::wAVLTreeNode* node = m_displays.FindNode(id);
        if (node == nullptr) {
            m_displays.unlock();
            return -EINVAL;
        }

        Display* display = reinterpret_cast<Display*>(node->value);

        m_displays.RemoveNode(node);
        m_displays.unlock();

        display->SetID(UINT64_MAX);

        // TODO: remove from fs once vnode removal is supported

        return 0;
    }

    uint64_t VideoDevice::GetID() {
        return m_id;
    }

    void VideoDevice::SetID(uint64_t id) {
        m_id = id;
    }

    // Assign the VideoDevice an ID and add to the list and fs
    int RegisterVideoDevice(VideoDevice* device) {
        if (device == nullptr)
            return -ENODEV;

        DeviceManager::DeviceInfo* info = device->GetInfo();
        if (info == nullptr || info->manager == nullptr)
            return -ENODEV;

        uint64_t id = g_lastVideoDeviceID.fetch_add(1);
        device->SetID(id);

        g_videoDevices.lock();
        g_videoDevices.Insert(id, device);
        g_videoDevices.unlock();

        char name[16];
        snprintf(name, 16, "gpu%lu", id);

        return info->manager->CreateRootDir(info, name);
    }

    FBVideoDevice* Video_CreateFBDevice() {
        if (g_KFBConsole == nullptr)
            return nullptr;

        FrameBuffer* fb = g_KFBConsole->GetFrameBuffer();
        if (fb == nullptr)
            return nullptr;

        FBVideoDevice* dev = new FBVideoDevice();
        FBDisplay* display = new FBDisplay(UINT64_MAX, fb);
        if (dev == nullptr || display == nullptr) {
            if (dev != nullptr)
                delete dev;
            if (display != nullptr)
                delete display;
            return nullptr;
        }

        dev->SetDisplay(display);
        
        DeviceManager::DeviceInfo* info = nullptr;
        int rc = g_DeviceManager->AddDevice(dev, &info);
        if (rc < 0) {
            delete display;
            delete dev;
            return nullptr;
        }

        rc = dev->Init(info);
        if (rc < 0) {
            delete display; // no need to remove the display as the device is being destroyed anyway
            delete dev;
            return nullptr;
        }

        rc = RegisterVideoDevice(dev);
        if (rc < 0) {
            delete display;
            delete dev;
            return nullptr;
        }

        rc = dev->AddDisplay(display);
        if (rc < 0) {
            // TODO: remove video device
            delete display;
            delete dev;
            return nullptr;
        }

        return dev;
    }

    // Setup the new FB console. Creates a generic FB device using the boot framebuffer if no devices exist.
    int Video_FullInit(FBConsole** newConsole) {
        FBDisplay* display = nullptr;
        g_videoDevices.lock();
        if (!g_videoDevices.isEmpty()) {
            g_videoDevices.forEach([&](uint64_t id, VideoDevice* device) -> bool {
                if (device == nullptr)
                    return true;
                
                display = device->GetFBDisplay();
                return display == nullptr;
            });
        }
        g_videoDevices.unlock();

        if (display == nullptr) { // no device was able to provide one, so we must create one with the boot framebuffer
            FBVideoDevice* device = Video_CreateFBDevice();
            if (device == nullptr)
                return -ENODEV;

            display = device->GetFBDisplay();
            if (display == nullptr)
                return -ENODEV;
        }

        return FBConsole_FullInit(display, newConsole);
    }
    
}
