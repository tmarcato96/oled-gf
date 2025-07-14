#pragma once

#include <Eigen/Core>
#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <variant>

// Placeholders
struct All_t
{
};

// fwd declarations and aliases
template<typename Values> struct KeySelectionProxy;

template<typename... Ts> struct Types
{
  using map_types = std::variant<Ts...>;
};

template<typename Values> 
struct VariantHolder
{
  using value_type = typename Values::map_types;
  value_type value;

  VariantHolder() = default;
  VariantHolder(value_type v) :
    value(std::move(v))
  {}

  VariantHolder& operator=(value_type&& v)
  {
    value = std::forward(v);
    return *this;
  }
};

template<typename Values> 
struct PolyMap {

  using value_type = typename Values::map_types;
  std::map<std::string, VariantHolder<Values>> polyMap;
  static inline constexpr All_t All{};

  auto& operator[](const std::string& key) 
  { return polyMap[key]; }

  template<typename... Keys> 
  auto operator()(Keys&&... keys) const 
  {
    return KeySelectionProxy<Values>(*this, {std::forward<Keys>(keys)...});
  }

  // Perform operation on all keys
  auto operator()(All_t) {
    std::vector<std::string> allKeys;
    for (const auto& [key, _] : allKeys) { allKeys.push_back(key); }
    return KeySelectionProxy<Values>(*this, allKeys);
  }
};

template<typename Values> 
struct KeySelectionProxy {

    using value_type = typename Values::map_types;
    using Operation = std::function<void(value_type&)>;

    const PolyMap<Values>& source;
    std::vector<std::string> keys;
    std::vector<Operation> operations;

    KeySelectionProxy(const PolyMap<Values>& src, std::vector<std::string> keys_) :
    source(src),
    keys(std::move(keys_))
    {}

    KeySelectionProxy(const PolyMap<Values>& src, std::vector<std::string> keys_, std::vector<Operation> ops) :
    source(src),
    keys(std::move(keys_)),
    operations(std::move(ops))
    {}

    // basic operations

    template<typename T> 
    KeySelectionProxy& operator+(const T& increment) const {
        auto newOps = operations;

        newOps.emplace_back(makeSumOp<T>(increment));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template<typename T> 
    KeySelectionProxy& operator+=(const T& increment) {

        operations.emplace_back(makeSumOp<T>(increment));
        evaluate();
        operations.clear();
        return *this;
    }

    template<typename T> 
    KeySelectionProxy& operator-(const T& decrement) const {
        auto newOps = operations;

        newOps.emplace_back(makeSubOp<T>(decrement));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template<typename T> 
    KeySelectionProxy& operator-=(const T& decrement) {

        operations.emplace_back(makeSubOp<T>(decrement));
        evaluate();
        operations.clear();
        return *this;
    }

    template<typename T> 
    KeySelectionProxy& operator*(const T& factor) const {

        auto newOps = operations;

        newOps.emplace_back(makeMultOp<T>(factor));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template<typename T> 
    KeySelectionProxy& operator*=(const T& factor) {

        operations.emplace_back(makeMultOp<T>(factor));
        evaluate();
        operations.clear();
        return *this;
    }

    template<typename T> 
    KeySelectionProxy& operator/(const T& denominator) const {
    auto newOps = operations;

        newOps.emplace_back(makeDivOp<T>(denominator));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template<typename T> 
    KeySelectionProxy& operator/=(const T& denominator) {

        operations.emplace_back(makeDivOp<T>(denominator));
        evaluate();
        operations.clear();
        return *this;
    }

    operator PolyMap<Values>() const { return evaluate(); }

    KeySelectionProxy& setZero() {

    auto zeroOp = [](value_type& val) {
        std::visit(
            [](auto& v) {
                if constexpr (requires { v.setZero(); }) {
                    v.setZero();
                }
                else if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                    v = 0;
                }
                else {
                    throw std::runtime_error("underlying type does not support setZero()");
                }
            },
        val);
    };

    operations.emplace_back(zeroOp);
    evaluate();
    operations.clear();
    return *this;
    }

    private:

    // operation function generators
    template<typename T> 
    auto makeSumOp(const T& operand) { // nested lambda here avoids headaches. Other approaches weren't as effective
    return [=](typename value_type& val) {
        std::visit(
        [&](auto& v) {
            using V = std::decay_t<decltype(v)>;
            if constexpr (requires(V a, T b) { a += b; }) { v += operand; }
            else {
            throw std::runtime_error("Incompatible type for += operation");
            }
        },
        val);
    };
    }

    template<typename T> 
    auto makeSubOp(const T& operand) {
    return [=](typename Values::map_types& val) {
        std::visit(
        [&](auto& v) {
            using V = std::decay_t<decltype(v)>;
            if constexpr (requires(V a, T b) { a -= b; }) { v -= operand; }
            else {
            throw std::runtime_error("Incompatible type for -= operation");
            }
        },
        val);
    };
    }

    template<typename T> 
    auto makeMultOp(const T& operand) {
    return [=](typename Values::map_types& val) {
        std::visit(
        [&](auto& v) {
            using V = std::decay_t<decltype(v)>;
            if constexpr (requires(V a, T b) { a *= b; }) { v *= operand; }
            else {
            throw std::runtime_error("Incompatible type for *= operation");
            }
        },
        val);
    };
    }

    template<typename T> 
    auto makeDivOp(const T& operand) {
        return [=](typename Values::map_types& val) {
            std::visit(
            [&](auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (requires(V a, T b) { a /= b; }) { v /= operand; }
                else {
                throw std::runtime_error("Incompatible type for /= operation");
                }
            },
            val);
        };
    }

    PolyMap<Values> evaluate() const {
    auto& mutable_source = const_cast<PolyMap<Values>&>(source);

        for (const auto& key : keys) {
            auto it = mutable_source.polyMap.find(key);
            if (it == mutable_source.polyMap.end()) continue;

            for (const auto& op : operations) { op(it->second.value); }
        }
        return mutable_source;
    }
};

// out of class operator definitions to allow for lhs operations
template<typename T, typename Values> 
auto operator+(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs + lhs; // Uses the existing member operator
}

template<typename T, typename Values> 
auto operator+=(T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs += lhs;
}

template<typename T, typename Values>
auto operator-(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs - lhs;
}

template<typename T, typename Values>
auto operator-=(T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs -= lhs;
}

template<typename T, typename Values>
auto operator*(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs * lhs;
}

template<typename T, typename Values>
auto operator*=(T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs *= lhs;
}

template<typename T, typename Values>
auto operator/(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs / lhs;
}

template<typename T, typename Values> 
auto operator/=(T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs /= lhs;
}