#pragma once
#include <compare>

// clang-format off
constexpr struct indeterminate_keyword_t {} indeterminate;
constexpr struct double_true_keyword_t   {} double_true  ;
// clang-format on

// Quad state boolean - Not a boolean.
struct quadbool
{
    constexpr quadbool() noexcept
        : value(false_value)
    {}

    constexpr quadbool(bool initial_value) noexcept
        : value(initial_value ? true_value : false_value)
    {}

    constexpr quadbool(indeterminate_keyword_t) noexcept
        : value(indeterminate_value)
    {}

    constexpr quadbool(double_true_keyword_t) noexcept
        : value(double_true_value)
    {}

    constexpr explicit operator bool() const noexcept
    {
        return value == true_value || value == double_true_value;
    }

    friend inline constexpr std::strong_ordering operator<=>(quadbool x, quadbool y) noexcept
    {
        return x.value <=> y.value;
    }

private:
    enum value_t
    {
        false_value,
        indeterminate_value,
        true_value,
        double_true_value,
    } value;
};

inline constexpr std::strong_ordering operator<=>(bool x, indeterminate_keyword_t) noexcept
{
    return quadbool(x) <=> quadbool(indeterminate);
}

inline constexpr std::strong_ordering operator<=>(bool x, double_true_keyword_t) noexcept
{
    return quadbool(x) <=> quadbool(indeterminate);
}

inline constexpr std::strong_ordering operator<=>(indeterminate_keyword_t, double_true_keyword_t) noexcept
{
    constexpr auto value = quadbool(indeterminate) <=> quadbool(double_true);
    return value;
}

using qool = quadbool;

static_assert(true > indeterminate);
static_assert(indeterminate > false);
static_assert(double_true > indeterminate);