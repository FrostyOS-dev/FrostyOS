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

#ifndef _HAL_FBVIDEODEVICE_HPP
#define _HAL_FBVIDEODEVICE_HPP

#include <stdint.h>

#include "FBPager.hpp"

#include "../VideoDevice.hpp"

#include "../../DeviceManager.hpp"

namespace Video {
    class FBDisplay;

    class FBVideoDevice : public VideoDevice {
    public:
        FBVideoDevice();
        virtual ~FBVideoDevice() override;

        virtual int Init() override;
        virtual int Init(DeviceManager::DeviceInfo* info) override;

        virtual FBDisplay* GetFBDisplay() override;
        
        void SetDisplay(FBDisplay* display);
        FBDisplay* GetDisplay();

        FBDisplayPager* GetPager();

    private:
        FBDisplay* m_display;
        FBDisplayPager m_pager;
    };

}

#endif /* _HAL_FBVIDEODEVICE_HPP */