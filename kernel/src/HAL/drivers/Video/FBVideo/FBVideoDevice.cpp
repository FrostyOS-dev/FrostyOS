/*
Copyright (©) 2024-2026  Frosty515

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

#include "FBDisplay.hpp"
#include "FBVideoDevice.hpp"

#include <errno.h>

namespace Video {

    FBVideoDevice::FBVideoDevice() : VideoDevice(), m_display(nullptr) {

    }

    FBVideoDevice::~FBVideoDevice() {

    }

    int FBVideoDevice::Init() {
        if (m_display == nullptr)
            return -ENODEV;
        return m_display->Init();
    }

    int FBVideoDevice::Init(DeviceManager::DeviceInfo* info) {
        SetInfo(info);
        if (m_display == nullptr)
            return -ENODEV;
        return m_display->Init();
    }

    FBDisplay* FBVideoDevice::GetFBDisplay() {
        return m_display;
    }

    void FBVideoDevice::SetDisplay(FBDisplay* display) {
        m_display = display;
        if (display != nullptr)
            display->SetDevice(this);
    }

    FBDisplay* FBVideoDevice::GetDisplay() {
        return m_display;
    }

    FBDisplayPager* FBVideoDevice::GetPager() {
        return &m_pager;
    }

}
