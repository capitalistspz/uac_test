#pragma once

#include <format>
#include <coreinit/debug.h>
#include <meta>

namespace cz {
    template <typename TEnum> requires std::is_enum_v<TEnum>
    std::string_view enum_identifier(TEnum value) {
        template for (constexpr std::meta::info info : std::define_static_array(enumerators_of(^^TEnum))) {
            if (value == [: info :])
                return identifier_of(info);
        }
        return "Unknown";
    }

    template <typename ...Args>
        void os_report(std::format_string<Args...> fmt, Args&&... args) {
        const auto result = std::format(fmt, static_cast<Args&&>(args)...);
        OSReport("%s", result.c_str());
    }

    template <typename ...Args>
    void os_reportln(std::format_string<Args...> fmt, Args&&... args) {
        const auto result = std::format(fmt, static_cast<Args&&>(args)...);
        OSReport("%s\n", result.c_str());
    }
}