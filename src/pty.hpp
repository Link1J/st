#pragma once

#include <string>

#if !defined(_WIN32)
#include <pwd.h>
#if defined(__linux)
#include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <libutil.h>
#endif

using pipe_t = int;

#else
#include <Windows.h>
#include <winrt/base.h>

using pipe_t = winrt::handle;
using pid_t  = winrt::handle;

namespace winrt
{
    struct hpcon_traits
    {
        using type = HPCON;

        static void close(type value) noexcept
        {
            ClosePseudoConsole(value);
        }

        static constexpr type invalid() noexcept
        {
            return nullptr;
        }
    };

    using hpcon = handle_type<hpcon_traits>;
} // namespace winrt

#endif

struct pty
{
    pid_t  process;
    pipe_t output;

#if defined(_WIN32)
    pipe_t       input;
    winrt::hpcon pc;
#endif

    static pty create(std::string shell, char const** args);

    void resize(int row, int col, int tw, int th);
};