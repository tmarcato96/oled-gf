#pragma once

#include <iostream>
#include <variant>
#include <functional>
#include <concepts>
#include <Eigen/Core>
#include <map>


//concepts
using idx_t = std::ptrdiff_t; //default underlying type of Eigen::Index, used for pointer arithmetic

template <typename T>
concept HasRow = requires(T t, idx_t i){ //WE NEED TO GET RID OF SIZE_T FOR INDICES. BIG MISTAKE.
    t.row(i);
};

template <typename T>
concept HasCol = requires(T t, idx_t i){
    t.col(i);
};

template<typename T>
concept HasBlock = requires(T t, idx_t r, idx_t c, idx_t rr, idx_t cc) {
    t.block(r,c,rr,cc); 
};


//fwd declarations and aliases
template<typename Values>
struct KeySelectionProxy;

template <typename... Ts>
struct Types {
    using map_types = std::variant<Ts...>;
};

//main data structure
template<typename Values>
struct PolyValMap{
    std::map<std::string, PolyValMap> polyMap;

    using value_type = typename Values::map_types;
    value_type value;

    template <typename T>
    auto& operator=(T&& v){
        static_assert(std::is_constructible_v<value_type, T>, "undefined assignment");
        value = std::forward<T>(v); //conserves value category
        return *this;
    }

    template <typename T>
    auto& operator[](const T key){
        return polyMap[key];
    }

    template <typename T>
    auto& get() const {
        static_assert(std::is_constructible_v<value_type, T>, "wrong getter called");

        return std::get<T>(value);
    }

    template <typename... Keys>
    auto operator()(Keys&&... keys) const {
        return KeySelectionProxy<Values>(*this, { std::forward<Keys>(keys)... });
    }
};

template <typename Values>
struct KeySelectionProxy {
    using value_type = typename Values::map_types;
    using Operation = std::function<void(value_type&)>;

    const PolyValMap<Values> &source;
    std::vector<std::string> keys;
    std::vector<Operation> operations;

    KeySelectionProxy(const PolyValMap<Values>& src, std::vector<std::string> keys_)
        : source(src), 
          keys(std::move(keys_))
          {}

    KeySelectionProxy(const PolyValMap<Values>& src, std::vector<std::string> keys_,
                      std::vector<Operation> ops)
        : source(src),
          keys(std::move(keys_)),
          operations(std::move(ops)) 
          {}

    //basic operations
    template <typename T>
    KeySelectionProxy& operator+(const T& increment) const {
        auto newOps = operations;

        newOps.emplace_back(makeSumOp<T>(increment));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template <typename T>
    KeySelectionProxy& operator+=(const T& increment) {
        operations.emplace_back(makeSumOp<T>(increment));
        evaluate();
        operations.clear();
        return *this;
    }

    template <typename T>
    KeySelectionProxy& operator-(const T& decrement) const {
        auto newOps = operations;

        newOps.emplace_back(makeSubOp<T>(decrement));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template <typename T>
    KeySelectionProxy& operator-=(const T& decrement) {
        operations.emplace_back(makeSubOp<T>(decrement));
        evaluate();
        operations.clear();
        return *this;
    }

    template <typename T>
    KeySelectionProxy& operator*(const T& factor) const {
        auto newOps = operations;

        newOps.emplace_back(makeMultOpGen<T>(factor));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template <typename T>
    KeySelectionProxy& operator*=(const T& factor) {
        operations.emplace_back(makeMultOp<T>(factor));
        evaluate();
        operations.clear();
        return *this;
    }

    template <typename T>
    KeySelectionProxy& operator/(const T& denominator) const {
        auto newOps = operations;

        newOps.emplace_back(makeDivOp<T>(denominator));
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    template <typename T>
    KeySelectionProxy& operator/=(const T& denominator) {
        operations.emplace_back(makeDivOp<T>(denominator));
        evaluate();
        operations.clear();
        return *this;
    }

    operator PolyValMap<Values>() const {
        return evaluate();
    }

    //matrix operations
    KeySelectionProxy col(idx_t i) const {
        using value_type = typename Values::map_types;

        std::vector<std::function<void(value_type&)>> newOps;
        for (const auto& op : operations) {
            newOps.emplace_back([=](value_type& val) {
                std::visit([&](auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (HasCol<V>) {
                        op(v.col(i));  // now this *should* mutate v's column
                    }
                }, val);
            });
        }
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    KeySelectionProxy row(idx_t i) const {
        using value_type = typename Values::map_types;

        std::vector<std::function<void(value_type&)>> newOps;
        for (const auto& op : operations) {
            newOps.emplace_back([=](value_type& val) {
                std::visit([&](auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (HasRow<V>) {
                        op(v.row(i)); 
                    }
                }, val);
            });
        }
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    KeySelectionProxy block(idx_t i, idx_t j, idx_t rr, idx_t cc) const {
        using value_type = typename Values::map_types;

        std::vector<std::function<void(value_type&)>> newOps;
        for (const auto& op : operations) {
            newOps.emplace_back([=](value_type& val) {
                std::visit([&](auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (HasRow<V>) {
                        op(v.block(i, j, rr, cc));
                    }
                }, val);
            });
        }
        return KeySelectionProxy(source, keys, std::move(newOps));
    }

    private:
    //operation function generators
    template<typename T>
    auto makeSumOp(const T& operand) { //nested lambda here avoids headaches. Other approaches weren't as effective
        return [=](typename Values::map_types& val) {
            std::visit([&](auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (requires(V a, T b) { a += b; }) {
                    v += operand;
                } else {
                    throw std::runtime_error("Incompatible type for += operation");
                }
            }, val);
        };
    }

    template<typename T>
    auto makeSubOp(const T& operand) {
        return [=](typename Values::map_types& val) {
            std::visit([&](auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (requires(V a, T b) { a -= b; }) {
                    v -= operand;
                } else {
                    throw std::runtime_error("Incompatible type for += operation");
                }
            }, val);
        };
    }

    template<typename T>
    auto makeMultOp(const T& operand) {
        return [=](typename Values::map_types& val) {
            std::visit([&](auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (requires(V a, T b) { a *= b; }) {
                    v *= operand;
                } else {
                    throw std::runtime_error("Incompatible type for += operation");
                }
            }, val);
        };
    }

    template<typename T>
    auto makeDivOp(const T& operand) {
        return [=](typename Values::map_types& val) {
            std::visit([&](auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (requires(V a, T b) { a /= b; }) {
                    v /= operand;
                } else {
                    throw std::runtime_error("Incompatible type for += operation");
                }
            }, val);
        };
    }

PolyValMap<Values> evaluate() const {
    auto& mutable_source = const_cast<PolyValMap<Values>&>(source);

        for (const auto& key : keys) {
            auto it = mutable_source.polyMap.find(key);
            if (it == mutable_source.polyMap.end()) continue;

            for (const auto& op : operations) {
                op(it->second.value);  // apply directly to the original map value
            }
        }

        return mutable_source;
    }
};


//out of class operator definitions to allow for lhs operations
template <typename T, typename Values>
auto operator+(const T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs + lhs; // Uses the existing member operator
}

template <typename T, typename Values>
auto operator+=(T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs += lhs;
}

template <typename T, typename Values>
auto operator-(const T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs - lhs; 
}

template <typename T, typename Values>
auto operator-=(T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs -= lhs;
}

template <typename T, typename Values>
auto operator*(const T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs * lhs;
}

template <typename T, typename Values>
auto operator*=(T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs *= lhs;
}

template <typename T, typename Values>
auto operator/(const T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs / lhs; 
}

template <typename T, typename Values>
auto operator/=(T& lhs, const KeySelectionProxy<Values>& rhs) {
    return rhs /= lhs;
}