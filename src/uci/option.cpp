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

#include "option.h"

#include "../util/parse.h"

namespace stormphrax::uci::option {
    Option::Option(std::string_view name, bool* ptr, bool nullDefault, std::function<void(bool)> callback) :
            m_type{OptionType::kCheck},
            m_name{name},
            m_id{toId(m_name)},
            m_storage{CheckOptionStorage{ptr, nullDefault, std::move(callback)}} {
        if (!checkStorage().ptr && !checkStorage().callback) {
            eprintln("Redundant check option {}!", name);
        }
    }

    Option::Option(
        std::string_view name,
        i32* ptr,
        i32 nullDefault,
        std::function<void(i32)> callback,
        util::Range<i32> range
    ) :
            m_type{OptionType::kSpin},
            m_name{name},
            m_id{toId(m_name)},
            m_storage{SpinOptionStorage{ptr, nullDefault, std::move(callback), range}} {
        if (!spinStorage().ptr && !spinStorage().callback) {
            eprintln("Redundant spin option {}!", name);
        }
    }

    Option::Option(std::string_view name, std::function<void()> callback) :
            m_type{OptionType::kButton},
            m_name{name},
            m_id{toId(m_name)},
            m_storage{ButtonOptionStorage{nullptr, std::monostate{}, [callback = std::move(callback)](std::monostate) {
                                              callback();
                                          }}} {
        if (!buttonStorage().callback) {
            eprintln("Redundant button option {}!", name);
        }
    }

    Option::Option(
        std::string_view name,
        std::string* ptr,
        std::string_view nullDefault,
        std::function<void(std::string_view)> callback
    ) :
            m_type{OptionType::kString},
            m_name{name},
            m_id{toId(m_name)},
            m_storage{StringOptionStorage{ptr, nullDefault, std::move(callback)}} {
        if (!stringStorage().ptr && !stringStorage().callback) {
            eprintln("Redundant string option {}!", name);
        }
    }

    void Option::set() {
        if (m_type != OptionType::kButton) {
            eprintln("Missing value");
            return;
        }

        auto& storage = buttonStorage();
        storage.callback(std::monostate{});
    }

    void Option::set(std::string_view value) {
        if (m_type == OptionType::kButton) {
            if (!value.empty()) {
                eprintln("Extraneous button value");
            }
            set();
            return;
        }

        if (value.empty()) {
            eprintln("Missing value");
            return;
        }

        switch (m_type) {
            case OptionType::kCheck:
                setCheck(value);
                break;
            case OptionType::kSpin:
                setSpin(value);
                break;
            case OptionType::kString:
                setString(value);
                break;
            default:
                __builtin_unreachable();
        }
    }

    void Option::display() const {
        print("option name {} type ", m_name);

        switch (m_type) {
            case OptionType::kCheck:
                println("check default {}", checkStorage().defaultValue);
                break;
            case OptionType::kSpin:
                println(
                    "spin default {} min {} max {}",
                    spinStorage().defaultValue,
                    spinStorage().range.min(),
                    spinStorage().range.max()
                );
                break;
            case OptionType::kButton:
                println("button");
                break;
            case OptionType::kString:
                println("string default {}", stringStorage().defaultValue);
                break;
        }
    }

    std::string Option::toId(std::string_view name) {
        std::string id{};
        id.reserve(name.size());

        std::ranges::transform(name, std::back_inserter(id), [](auto c) { return std::tolower(c); });

        if (id.starts_with("uci_")) {
            id = id.substr(4);
        }

        return id;
    }

    void Option::setCheck(std::string_view value) {
        if (const auto parsed = util::tryParseBool(value)) {
            const auto v = *parsed;

            auto& storage = checkStorage();

            if (storage.ptr) {
                *storage.ptr = v;
            }

            if (storage.callback) {
                storage.callback(v);
            }
        } else {
            println("Invalid value \"{}\"", value);
        }
    }

    void Option::setSpin(std::string_view value) {
        if (const auto parsed = util::tryParse<i32>(value)) {
            const auto v = *parsed;

            auto& storage = spinStorage();

            const auto clamped = storage.range.clamp(v);

            if (storage.ptr) {
                *storage.ptr = clamped;
            }

            if (storage.callback) {
                storage.callback(clamped);
            }
        } else {
            println("Invalid value \"{}\"", value);
        }
    }

    void Option::setString(std::string_view value) {
        auto& storage = stringStorage();

        if (storage.ptr) {
            *storage.ptr = value;
        }

        if (storage.callback) {
            storage.callback(value);
        }
    }
} // namespace stormphrax::uci::option
