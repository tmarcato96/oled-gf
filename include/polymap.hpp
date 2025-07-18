#pragma once

#include <Eigen/Core>
#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <variant>

// Placeholders
struct All_t {};

// fwd declarations and aliases
template<typename Values> struct KeySelectionProxy;

template<typename... Ts> struct Types {
  using map_types = std::variant<Ts...>;
};

//function generators
template<typename T, typename Values>
auto makeSumOp(const T& operand) {
  using value_type = typename Values::map_types;
  return [&operand](value_type& val) {
    std::visit([&](auto& v) {
      using V = std::decay_t<decltype(v)>;
      if constexpr (requires(V a, T b) { a += b; }) { 
        v += operand;
      } 
      else {
        throw std::runtime_error("Incompatible type for += operation");
      }
    }, val);
  };
}

template<typename T, typename Values>
auto makeSubOp(const T& operand) {
  using value_type = typename Values::map_types;
  return [&operand](value_type& val) {
    std::visit([&](auto& v) {
      using V = std::decay_t<decltype(v)>;
      if constexpr (requires(V a, T b) { a -= b; }) {
        v -= operand;
      } 
      else {
        throw std::runtime_error("Incompatible type for -= operation");
      }
    }, val);
  };
}

template<typename T, typename Values>
auto makeMultOp(const T& operand, std::string const* key=nullptr) {
  using value_type = typename Values::map_types;
  return [&operand](value_type& val) {
    std::visit([&](auto& v) {
      using V = std::decay_t<decltype(v)>;
      if constexpr (requires(V a, T b) { a *= b; }) {
        v *= operand;
      } 
      else {
        throw std::runtime_error("Incompatible type for *= operation");
      }
    }, val);
  };
}

template<typename T, typename Values>
auto makeDivOp(const T& operand) {
  using value_type = typename Values::map_types;
  return [&operand](value_type& val) {
    std::visit([&](auto& v) {
      using V = std::decay_t<decltype(v)>;
      if constexpr (requires(V a, T b) { a /= b; }) {
        v /= operand;
      } 
      else {
        throw std::runtime_error("Incompatible type for /= operation");
      }
    }, val);
  };
}

template<typename Values>
struct VariantHolder {
  using value_type = typename Values::map_types;
  value_type value;

  VariantHolder() = default;
  VariantHolder(value_type v) : value(std::move(v)) {}

  VariantHolder& operator=(value_type&& v) {
    value = std::move(v);
    return *this;
  }

  VariantHolder& operator=(const value_type& v) {
    value = v;
    return *this;
  }
};

template<typename Values>
struct PolyMap {
  using value_type = typename Values::map_types;
  std::map<std::string, VariantHolder<Values>, std::less<>> polyMap;
  static inline constexpr All_t All{};

  auto& operator[](const std::string& key) { return polyMap[key]; }

  template<typename... Keys>
  auto operator()(Keys&&... keys) {
    return KeySelectionProxy<Values>(this, {std::forward<Keys>(keys)...});
  }

  auto operator()(All_t) {
    std::set<std::string> allKeys;
    for (const auto& [key, _] : polyMap) allKeys.insert(key);
    return KeySelectionProxy<Values>(this, allKeys);
  }
};

template<typename Values>
struct KeySelectionProxy {
  using value_type = typename Values::map_types;
  using Operation = std::function<void(value_type&)>;

  PolyMap<Values>* const source;
  std::vector<std::string> keys;
  std::vector<std::pair<const std::string*, Operation>> operations;

  explicit KeySelectionProxy(PolyMap<Values>* const src, std::vector<std::string> keys_, std::vector<std::pair<const std::string*, Operation>> ops = {})
    : source{src}, 
      keys{std::move(keys_)}, 
      operations{std::move(ops)} 
      {}

  // basic operators
  KeySelectionProxy<Values>& operator=(const KeySelectionProxy<Values>&& operand) {
    auto other = operand;
    if (this->keys.size() != other.keys.size()) {
        throw std::runtime_error("KeySelectionProxy assignment requires matching key counts");
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& targetKey = this->keys[i];
        const std::string& sourceKey = other.keys[i];

        auto itDst = source->polyMap.find(targetKey);
        auto itSrc = other.source->polyMap.find(sourceKey);
        if (itDst == source->polyMap.end() || itSrc == other.source->polyMap.end()) continue;

        value_type evaluated = itSrc->second.value;

        // Apply any deferred operations from 'other'
        for (const auto& [opKey, opFunc] : other.operations) {
            if (*opKey == sourceKey) {
                opFunc(evaluated);  // Apply to copy
            }
        }

        // Safe assignment using std::visit
        std::visit([&](auto&& srcVal) {
            using T = std::decay_t<decltype(srcVal)>;
            std::visit([&](auto& dstVal) {
                using U = std::decay_t<decltype(dstVal)>;
                if constexpr (std::is_same_v<T, U>) {
                    dstVal = srcVal;
                } else {
                    throw std::runtime_error("Mismatched variant types during assignment.");
                }
            }, itDst->second.value);
        }, evaluated);
      }

      return *this;
  }

  template<typename T>
  KeySelectionProxy operator+(const T& increment) {
    operations.emplace_back(nullptr, makeSumOp<T, Values>(increment));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator+(const KeySelectionProxy<Values>& other) {
    assert(other.source != nullptr);
    if (this->keys.size() == other.keys.size()) {
      for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const std::string& otherKey = other.keys[i];

        auto it = source->polyMap.find(key);
        auto itOther = other.source->polyMap.find(otherKey);
        if (it == source->polyMap.end() || itOther == other.source->polyMap.end()) continue;

        const std::string* keyPtr = &it->first;
        value_type& otherVal = itOther->second.value;

        std::visit(
          [&](const auto& inner_val) {
            using T = std::decay_t<decltype(inner_val)>;
            operations.emplace_back(keyPtr, makeSumOp<T, Values>(inner_val));
          },
          otherVal
        );
      }
      return KeySelectionProxy<Values>(source, keys, std::move(operations));
    } 
    else {
      throw std::runtime_error("binary proxy operator + requires same number of keys");
    }
  }

  template<typename T> 
  KeySelectionProxy& operator+=(const T& increment) {
    operations.emplace_back(nullptr, makeSumOp<T, Values>(increment));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values>& operator+=(const KeySelectionProxy<Values>&& other) {
    auto proxy = *this + other;
    proxy.evaluate();
    return *this;
  }

  template<typename T> 
  KeySelectionProxy operator-(const T& decrement) {
    operations.emplace_back(nullptr, makeSubOp<T, Values>(decrement));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator-(const KeySelectionProxy<Values>& other) {
    assert(other.source != nullptr);
    if (this->keys.size() == other.keys.size()) {
      for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const std::string& otherKey = other.keys[i];

        auto it = source->polyMap.find(key);
        auto itOther = other.source->polyMap.find(otherKey);
        if (it == source->polyMap.end() || itOther == other.source->polyMap.end()) continue;

        const std::string* keyPtr = &it->first;
        value_type& otherVal = itOther->second.value;

        std::visit(
          [&](const auto& inner_val) {
            using T = std::decay_t<decltype(inner_val)>;
            operations.emplace_back(keyPtr, makeSubOp<T, Values>(inner_val));
          },
          otherVal
        );
      }
      return KeySelectionProxy<Values>(source, keys, std::move(operations));
    } 
    else {
      throw std::runtime_error("binary proxy operator - requires same number of keys");
    }
  }
  
  template<typename T> 
  KeySelectionProxy& operator-=(const T& decrement) {
    operations.emplace_back(nullptr, makeSubOp<T, Values>(decrement));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values>& operator-=(const KeySelectionProxy<Values>&& other) {
    auto proxy = *this - other;
    proxy.evaluate();
    return *this;
  }

  template<typename T> 
  KeySelectionProxy operator*(const T& factor) {
    operations.emplace_back(nullptr, makeMultOp<T, Values>(factor));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator*(const KeySelectionProxy<Values>&& other) {
    assert(other.source != nullptr);
    if (this->keys.size() == other.keys.size()) {
      for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const std::string& otherKey = other.keys[i];

        auto it = source->polyMap.find(key);
        auto itOther = other.source->polyMap.find(otherKey);
        if (it == source->polyMap.end() || itOther == other.source->polyMap.end()) continue;

        const std::string* keyPtr = &it->first;
        value_type& otherVal = itOther->second.value;

        std::visit(
          [&](const auto& inner_val) {
            using T = std::decay_t<decltype(inner_val)>;
            operations.emplace_back(keyPtr, makeMultOp<T, Values>(inner_val));
          },
          otherVal
        );
      }
      return KeySelectionProxy<Values>(source, keys, std::move(operations));
    } 
    else {
      throw std::runtime_error("binary proxy operator * requires same number of keys");
    }
  }
  

  template<typename T> 
  KeySelectionProxy& operator*=(const T& factor) {

    operations.emplace_back(nullptr, makeMultOp<T, Values>(factor));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values>& operator*=(const KeySelectionProxy<Values>& other) {
    auto proxy = *this * other;
    proxy.evaluate();
    return *this;
  }

  template<typename T> 
  KeySelectionProxy operator/(const T& denominator) {
    operations.emplace_back(nullptr, makeDivOp<T, Values>(denominator));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator/(const KeySelectionProxy<Values>&& other) {
    assert(other.source != nullptr);
    if (this->keys.size() == other.keys.size()) {
      for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const std::string& otherKey = other.keys[i];

        auto it = source->polyMap.find(key);
        auto itOther = other.source->polyMap.find(otherKey);
        if (it == source->polyMap.end() || itOther == other.source->polyMap.end()) continue;

        const std::string* keyPtr = &it->first;
        value_type& otherVal = itOther->second.value;

        std::visit(
          [&](const auto& inner_val) {
            using T = std::decay_t<decltype(inner_val)>;
            operations.emplace_back(keyPtr, makeDivOp<T, Values>(inner_val));
          },
          otherVal
        );
      }
      return KeySelectionProxy<Values>(source, keys, std::move(operations));
    } 
    else {
      throw std::runtime_error("binary proxy operator / requires same number of keys");
    }
  }
  

  template<typename T> 
  KeySelectionProxy& operator/=(const T& denominator) {

    operations.emplace_back({nullptr, makeDivOp<T, Values>(denominator)});
    evaluate();
    operations.clear();
    return *this;

  }

  KeySelectionProxy<Values>& operator/=(const KeySelectionProxy<Values>& other) {
    auto proxy = *this / other;
    proxy.evaluate();
    return *this;
  }


  operator PolyMap<Values>&() { return evaluate(); }

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
    operations.emplace_back(nullptr, zeroOp);
    evaluate();
    operations.clear();
    return *this;
  }

  private:

    PolyMap<Values>& evaluate() {
      for (const auto& [opKey, op] : operations) {
        if (opKey == nullptr) {
          for (const auto& key : keys) {
            auto it = source->polyMap.find(key);
            if (it == source->polyMap.end()) continue;

            try {
              op(it->second.value);
            } 
            catch (const std::exception& e) {
              std::cerr << "Operation failed on key [" << key << "]: " << e.what() << '\n';
            }
          }
        } 
        else {
          try {
            auto it = source->polyMap.find(*opKey);
            if (it == source->polyMap.end())
              throw std::runtime_error("Key not found in map");
            op(it->second.value);
          } 
          catch (const std::exception& e) {
            std::cerr << "Operation failed on key [" << *opKey << "]: " << e.what() << '\n';
          }
        }
      }
      return *source;
    }
};

// out of class operator definitions to allow for lhs operations
template<typename T, typename Values> 
auto operator+(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs + lhs; // Uses the existing member operator
}

template<typename T, typename Values>
auto operator-(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs - lhs;
}

template<typename T, typename Values>
auto operator*(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs * lhs;
}

template<typename T, typename Values>
auto operator/(const T& lhs, const KeySelectionProxy<Values>& rhs)
{
  return rhs / lhs;
}