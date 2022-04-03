#pragma once
#include <ljh/color.hpp>
#include <charconv>
#include <tuple>
#include <vector>
#include <bitset>

template<typename C>
[[nodiscard]] C take_first(std::basic_string_view<C>& data)
{
    auto out = data[0];
    data.remove_prefix(1);
    return out;
}

template<typename T>
[[nodiscard]] T take_from_chars(std::string_view& data)
{
    T    output;
    auto res = std::from_chars(data.data(), data.data() + data.size(), output);
    data.remove_prefix(res.ptr - data.data());
    return output;
}

template<typename T>
[[nodiscard]] T take_from_chars(std::string_view& data, std::nullptr_t)
{
    return take_from_chars<T>(data);
}

template<typename T>
[[nodiscard]] T take_from_chars(std::string_view& data, char end)
{
    auto output = take_from_chars<T>(data);
    if (take_first(data) != end)
        throw 1;
    return output;
}

template<typename... T, typename... A>
[[nodiscard]] std::enable_if_t<sizeof...(T) == sizeof...(A), std::tuple<T...>> take_from_chars(std::string_view& data, A&&... args)
{
    return {take_from_chars<T>(data, std::forward<A>(args))...};
}

using color = uint32_t;
color make_color(auto r, auto g, auto b)
{
    // return std::make_tuple(r / 255., g / 255., b / 255.);
    return (r << 24 | g << 16 | b << 8) >> 8;
}

auto parse_sixel(std::string_view data)
{
    std::vector<color>     pixels;
    std::array<color, 256> colors;
    uint8_t                sel_color = 0;
    size_t                 line      = 0;
    size_t                 col       = 0;
    size_t                 width;
    size_t                 height;

    auto draw = [&](std::bitset<6> bits) {
        for (size_t i = 0; i < std::size(bits); i++)
            if (bits[i])
                pixels[(line + i) * width + col] = colors[sel_color];
        col++;
    };

    while (!data.empty())
    {
        if (auto action = take_first(data); '?' <= action && action <= '~')
        {
            draw(action - '?');
        }
        else if (action == '"')
        {
            auto next = take_from_chars<size_t, size_t, size_t, size_t>(data, ';', ';', ';', nullptr);
            size_t a, b; // std::ignore
            std::tie(a, b, width, height) = next;
            pixels.resize(height * width);
        }
        else if (action == '#')
        {
            sel_color = take_from_chars<decltype(sel_color)>(data);
            if (data[0] != ';')
                continue;
            data.remove_prefix(1);

            auto [cc, r, g, b] = take_from_chars<size_t, uint32_t, uint32_t, uint32_t>(data, ';', ';', ';', nullptr);
            switch (cc)
            {
            case 1: // HSL
                std::tie(r, g, b) = ljh::hsl_to_rgb(r / 360., b / 100., g / 100.);
                break;

            case 2: // RGB
                r = uint32_t(std::round(r / 100. * 255)) & 0xFF;
                g = uint32_t(std::round(g / 100. * 255)) & 0xFF;
                b = uint32_t(std::round(b / 100. * 255)) & 0xFF;
                break;

            case 3: // True Color
                break;

            default:
                throw 1;
            }

            colors[sel_color] = make_color(r, g, b);
        }
        else if (action == '!')
        {
            auto count  = take_from_chars<size_t>(data);
            char letter = take_first(data) - '?';
            for (size_t i = 0; i < count; i++)
                draw(letter);
        }
        else if (action == '-')
        {
            line += 6;
            col = 0;
        }
        else if (action == '$')
        {
            col = 0;
        }
    }

    return std::make_tuple(pixels, width, height);
}