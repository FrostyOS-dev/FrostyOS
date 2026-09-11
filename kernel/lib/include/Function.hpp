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

#ifndef _KLIB_FUNCTION_HPP
#define _KLIB_FUNCTION_HPP

#include <type_traits>
#include <utility>

template <typename Signature>
class FunctionRef;

template <typename ReturnType, typename... Args>
class FunctionRef<ReturnType(Args...)> {
    void* instance_;
    ReturnType (*callback_)(void*, Args...);

public:
    template <typename F>
        requires (!std::is_same_v<std::remove_cvref_t<F>, FunctionRef>) &&
            requires(F& f, Args... args) {
                { f(std::forward<Args>(args)...) };
            }
    FunctionRef(F&& callable) : instance_(const_cast<void*>(static_cast<const void*>(&callable))) {
        callback_ = [](void* inst, Args... args) -> ReturnType {
            auto* ptr = static_cast<std::remove_reference_t<F>*>(inst);
            return (*ptr)(std::forward<Args>(args)...);
        };
    }

    ReturnType operator()(Args... args) const {
        return callback_(instance_, std::forward<Args>(args)...);
    }
};

#endif /* _KLIB_FUNCTION_HPP */