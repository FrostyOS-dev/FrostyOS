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

#include "KBDInputSource.hpp"
#include "KBDTranslator.hpp"

#include "../TTY.hpp"

#include <HAL/drivers/Input/InputEvent.hpp>

KeyboardInputSource::KeyboardInputSource(Input::EventSourceID source) : m_source(source), m_eventListener(&KeyboardInputSource::GenericEventHandler, this), m_translator() {
    using namespace Input;
    if (g_EventManager == nullptr)
        return;
    g_EventManager->RegisterEventHandler(m_source, m_eventListener);
}

KeyboardInputSource::~KeyboardInputSource() {
    if (Input::g_EventManager != nullptr)
        Input::g_EventManager->UnregisterEventHandler(m_eventListener);
}

bool KeyboardInputSource::GenericEventHandler(const Input::Frame* f, void* ctx) {
    if (ctx == nullptr || f == nullptr)
        return false;
    return static_cast<KeyboardInputSource*>(ctx)->EventHandler(f);
}

bool KeyboardInputSource::EventHandler(const Input::Frame* f) { // consumes all valid events
    TTY* tty = g_CurrentTTY;
    bool graphical = tty != nullptr && tty->GetType() == TTYType::Graphical;
    if (graphical)
        m_translator.SetApplicationCursorKeys(tty->ApplicationCursorKeys());

    bool sawKey = false;
    for (uint32_t i = 0; i < f->count; i++) {
        const Input::Event& e = f->events[i];
        if (e.type != Input::ET_KEY)
            continue;
        sawKey = true;

        KeyAction action = e.value == 0 ? KeyAction::Release : (e.value == 2 ? KeyAction::Repeat : KeyAction::Press);

        char seq[KT_MAX_SEQ];
        size_t n = m_translator.HandleKey(static_cast<Key>(e.code), action, seq);
        if (n > 0 && graphical)
            tty->InputBytes(seq, n);
    }
    return sawKey;
}
