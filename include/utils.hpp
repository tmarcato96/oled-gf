#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include <Eigen/Core>

// Always false trait for static_assert
template<class...> struct always_false : std::false_type
{
};
template<class... Ts> inline constexpr bool always_false_v = always_false<Ts...>::value;

// Casting function from size_t to Eigen::Index and viceversa

// Just static_cast at compile time. To be used when you know it fits
template<class To, class From> constexpr To narrow_cast(From x) noexcept { return static_cast<To>(x); }

// Checked version of cast that throws at runtime if overflow is possible
template<class To, class From> To checked_narrow_cast(From x)
{
  if constexpr (std::is_integral_v<From> && std::is_integral_v<To>) {
    if constexpr (std::is_signed_v<From> == std::is_signed_v<To>) {
      if constexpr (sizeof(To) < sizeof(From)) {
        if (x < std::numeric_limits<To>::lowest() || x > std::numeric_limits<To>::max())
          throw std::runtime_error("checked_narrow_cast: overflow");
      }
    }
    else if constexpr (std::is_signed_v<From>) { // signed -> unsigned
      if (x < 0 || static_cast<std::make_unsigned_t<From>>(x) > std::numeric_limits<To>::max())
        throw std::runtime_error("checked_narrow_cast: overflow");
    }
    else { // unsigned -> signed
      if (x > static_cast<std::make_unsigned_t<To>>(std::numeric_limits<To>::max()))
        throw std::runtime_error("checked_narrow_cast: overflow");
    }
    return static_cast<To>(x);
  }
  else if constexpr (std::is_floating_point_v<From> && std::is_integral_v<To>) {
    // floating -> integral
    if (!std::isfinite(x)) throw std::runtime_error("checked_narrow_cast: Nan/Inf not allowed");

    const long double lx = static_cast<long double>(x);
    const long double lo = std::is_signed_v<To> ? static_cast<long double>(std::numeric_limits<To>::lowest()) : 0.0L;
    const long double hi = static_cast<long double>(std::numeric_limits<To>::max());

    if (lx < lo || lx > hi) throw std::runtime_error("checked_narrow_cast: out of range (float -> int)");

    // Reject if fractional
    if (std::trunc(lx) != lx) throw std::runtime_error("checked_narrow_cast: fractional value (float -> int)");

    return static_cast<To>(x);
  }
  else {
    static_assert(always_false_v<To, From>, "checked_narrow_cast: unsupported type combination");
  }
}

inline Eigen::Index toIndex(size_t n) { return narrow_cast<Eigen::Index>(n); }

inline size_t toSize(Eigen::Index i) { return narrow_cast<size_t>(i); }

inline Eigen::Index toIndexChecked(size_t n) { return checked_narrow_cast<Eigen::Index>(n); }

inline size_t toSizeChecked(Eigen::Index i) { return checked_narrow_cast<size_t>(i); }