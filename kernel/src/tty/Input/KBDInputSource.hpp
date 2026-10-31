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

#ifndef _TTY_KBD_INPUT_SOURCE_HPP
#define _TTY_KBD_INPUT_SOURCE_HPP

#include "KBDTranslator.hpp"

#include <HAL/drivers/Input/InputEvent.hpp>

class KeyboardInputSource {
public:
    KeyboardInputSource(Input::EventSourceID source);
    ~KeyboardInputSource();

private:
    static bool GenericEventHandler(const Input::Event* e, void* ctx);

    bool EventHandler(const Input::Event* e);

    Input::EventSourceID m_source;
    Input::EventListener m_eventListener;

    KBDTranslator m_translator;
};

#endif /* _TTY_KBD_INPUT_SOURCE_HPP */