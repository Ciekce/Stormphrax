/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2026 Ciekce
 *
 * Stormphrax is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stormphrax is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stormphrax. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../types.h"

#include <functional>
#include <variant>

#include "../util/range.h"

namespace stormphrax::uci::option {
    enum class OptionType {
        kCheck,
        kSpin,
        kButton,
        kString,
    };

    template <typename T, typename View = T>
    struct OptionStorageBase {
        T* ptr;
        T defaultValue{};
        std::function<void(View)> callback;

        explicit OptionStorageBase(T* ptr_, View nullDefault, std::function<void(View)> callback_) :
                ptr{ptr_}, callback{std::move(callback_)} {
            if (ptr) {
                defaultValue = *ptr;
            } else {
                defaultValue = nullDefault;
            }
        }
    };

    using CheckOptionStorage = OptionStorageBase<bool>;
    using ButtonOptionStorage = OptionStorageBase<std::monostate>;
    using StringOptionStorage = OptionStorageBase<std::string, std::string_view>;

    struct SpinOptionStorage : OptionStorageBase<i32> {
        util::Range<i32> range;
        explicit SpinOptionStorage(
            i32* ptr_,
            i32 nullDefault,
            std::function<void(i32)> callback_,
            util::Range<i32> range_
        ) :
                OptionStorageBase{ptr_, nullDefault, std::move(callback_)}, range{range_} {}
    };

    using OptionStorage = std::variant<CheckOptionStorage, SpinOptionStorage, ButtonOptionStorage, StringOptionStorage>;

    class Option {
    public:
        Option(std::string_view name, bool* ptr, bool nullDefault, std::function<void(bool)> callback);

        Option(
            std::string_view name,
            i32* ptr,
            i32 nullDefault,
            std::function<void(i32)> callback,
            util::Range<i32> range
        );

        Option(std::string_view name, std::function<void()> callback);

        Option(
            std::string_view name,
            std::string* ptr,
            std::string_view nullDefault,
            std::function<void(std::string_view)> callback
        );

        [[nodiscard]] inline OptionType type() const {
            return m_type;
        }

        [[nodiscard]] inline std::string_view name() const {
            return m_name;
        }

        [[nodiscard]] inline std::string_view id() const {
            return m_id;
        }

        void set();
        void set(std::string_view value);

        void display() const;

        [[nodiscard]] static std::string toId(std::string_view name);

    private:
        OptionType m_type;

        std::string m_name;
        std::string m_id;

        OptionStorage m_storage;

        void setCheck(std::string_view value);
        void setSpin(std::string_view value);
        void setString(std::string_view value);

        [[nodiscard]] CheckOptionStorage& checkStorage() {
            return std::get<CheckOptionStorage>(m_storage);
        }

        [[nodiscard]] const CheckOptionStorage& checkStorage() const {
            return std::get<CheckOptionStorage>(m_storage);
        }

        [[nodiscard]] SpinOptionStorage& spinStorage() {
            return std::get<SpinOptionStorage>(m_storage);
        }

        [[nodiscard]] const SpinOptionStorage& spinStorage() const {
            return std::get<SpinOptionStorage>(m_storage);
        }

        [[nodiscard]] ButtonOptionStorage& buttonStorage() {
            return std::get<ButtonOptionStorage>(m_storage);
        }

        [[nodiscard]] const ButtonOptionStorage& buttonStorage() const {
            return std::get<ButtonOptionStorage>(m_storage);
        }

        [[nodiscard]] StringOptionStorage& stringStorage() {
            return std::get<StringOptionStorage>(m_storage);
        }

        [[nodiscard]] const StringOptionStorage& stringStorage() const {
            return std::get<StringOptionStorage>(m_storage);
        }
    };
} // namespace stormphrax::uci::option
