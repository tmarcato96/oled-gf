#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <type_traits>
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
auto makeMultOp(const T& operand) {
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

  value_type& operator[](const std::string& key) { return polyMap[key].value; }

  const value_type& operator[](const std::string& key) const { 
    return polyMap.at(key).value; 
  }

  template<typename T>
  const T& get(const std::string& key) const { 
    return std::get<T>(polyMap.at(key).value); 
  }

  template<typename T>
  T& get(const std::string& key) { 
    return std::get<T>(polyMap[key].value); 
  }

  template<typename... Keys>
  auto operator()(Keys&&... keys) {
      static_assert((std::is_convertible_v<Keys, std::string> && ...),
                    "All keys must be convertible to std::string");

      std::vector<std::string> keyVec = {std::forward<Keys>(keys)...};

      for (const auto& key : keyVec) {
          if (polyMap.find(key) == polyMap.end()) {
              throw std::runtime_error("Key not found in polyMap: " + key);
          }
      }

      return KeySelectionProxy<Values>(this, std::move(keyVec));
  }

  auto operator()(All_t) {
    std::vector<std::string> allKeys;
    for (const auto& [key, _] : polyMap) allKeys.insert(key);
    return KeySelectionProxy<Values>(this, allKeys);
  }

  auto operator()() {
    (*this)(All_t{});
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
KeySelectionProxy<Values>& operator=(KeySelectionProxy<Values>&& other) {
  if (this->keys.size() != other.keys.size()) {
      throw std::runtime_error("KeySelectionProxy assignment requires matching key counts");
  }

  for (size_t i = 0; i < keys.size(); ++i) {
      const std::string& targetKey = this->keys[i];
      const std::string& sourceKey = other.keys[i];

      auto itDst = source->polyMap.find(targetKey);
      auto itSrc = other.source->polyMap.find(sourceKey);
      if (itDst == source->polyMap.end() || itSrc == other.source->polyMap.end()) continue;

      value_type& dstVal = itDst->second.value;

      // Make a copy of the source value so we can evaluate into it
      value_type evaluated = itSrc->second.value;

      // Apply deferred operations from 'other' (only those for this sourceKey)
      for (const auto& [opKey, opFunc] : other.operations) {
          if (opKey == nullptr || *opKey == sourceKey) {
              opFunc(evaluated);
          }
      }
      dstVal = std::move(evaluated);
  }
  return *this;
}

  template<typename T>
  KeySelectionProxy operator+(const T& increment) {
    operations.emplace_back(nullptr, makeSumOp<T, Values>(increment));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator+(const KeySelectionProxy<Values>& other) {
    return make_operator(ProxyBinOp::sum, other);
  }

  template<typename T> 
  KeySelectionProxy& operator+=(const T& increment) {
    operations.emplace_back(nullptr, makeSumOp<T, Values>(increment));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values>& operator+=(KeySelectionProxy<Values>&& other) {
    *this = *this + other;
    return *this;
  }

  template<typename T> 
  KeySelectionProxy operator-(const T& decrement) {
    operations.emplace_back(nullptr, makeSubOp<T, Values>(decrement));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator-(const KeySelectionProxy<Values>& other) {
    return make_operator(ProxyBinOp::sub, other);
  }
  
  template<typename T> 
  KeySelectionProxy& operator-=(const T& decrement) {
    operations.emplace_back(nullptr, makeSubOp<T, Values>(decrement));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values>& operator-=(KeySelectionProxy<Values>&& other) {
    *this = *this - other;
    return *this;
  }

  template<typename T> 
  KeySelectionProxy operator*(const T& factor) {
    operations.emplace_back(nullptr, makeMultOp<T, Values>(factor));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator*(const KeySelectionProxy<Values>&& other) {
    return make_operator(ProxyBinOp::mult, other);
  }
  
  template<typename T>
  KeySelectionProxy& operator*=(const T& factor) {
    operations.emplace_back(nullptr, makeMultOp<T, Values>(factor));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values>& operator*=(KeySelectionProxy<Values>&& other) {
    *this = *this * other;
    return *this;
  }

  template<typename T> 
  KeySelectionProxy operator/(const T& denominator) {
    operations.emplace_back(nullptr, makeDivOp<T, Values>(denominator));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values> operator/(KeySelectionProxy<Values>&& other) {
    return make_operator(ProxyBinOp::div, other);
  }
  

  template<typename T>
  KeySelectionProxy& operator/=(const T& denominator) {
    operations.emplace_back(nullptr, makeDivOp<T, Values>(denominator));
    evaluate();
    operations.clear();
    return *this;

  }

  KeySelectionProxy<Values>& operator/=(KeySelectionProxy<Values>&& other) {
    *this = *this / other;
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

    //helper function
    enum class ProxyBinOp{sum, sub, mult, div};

    KeySelectionProxy<Values> make_operator(ProxyBinOp op, const KeySelectionProxy<Values>& operand) {
      assert(operand.source != nullptr);
      if (this->keys.size() == operand.keys.size()) {
        for (size_t i = 0; i < keys.size(); ++i) {
          const std::string& key = keys[i];
          const std::string& operandKey = operand.keys[i];

          auto it = source->polyMap.find(key);
          auto itoperand = operand.source->polyMap.find(operandKey);
          if (it == source->polyMap.end() || itoperand == operand.source->polyMap.end()) continue;

          const std::string* keyPtr = &it->first;
          value_type& operandVal = itoperand->second.value;

          std::visit(
            [&](const auto& inner_val) {
              using T = std::decay_t<decltype(inner_val)>;

              switch(op) { //picks the right operation for the lambda
                case ProxyBinOp::sum:
                  operations.emplace_back(keyPtr, makeSumOp<T, Values>(inner_val));
                  break;
                case ProxyBinOp::sub:
                  operations.emplace_back(keyPtr, makeSubOp<T, Values>(inner_val));
                  break;
                case ProxyBinOp::mult:
                  operations.emplace_back(keyPtr, makeMultOp<T, Values>(inner_val));
                  break;
                case ProxyBinOp::div:
                  operations.emplace_back(keyPtr, makeDivOp<T, Values>(inner_val));
                  break;
              }
            },
            operandVal
          );
        }
        return KeySelectionProxy<Values>(source, keys, std::move(operations));
      } 
      else {
        throw std::runtime_error("binary proxy operators require same number of keys");
      }
    }
};

// out of class operator definitions to allow for lhs operations
template<typename T, typename Values> 
requires (!std::same_as<std::decay_t<T>, KeySelectionProxy<Values>>)
auto operator+(const T& lhs, KeySelectionProxy<Values>&& rhs)
{
  return rhs + lhs; // Uses the existing member operator
}

template<typename T, typename Values>
requires (!std::same_as<std::decay_t<T>, KeySelectionProxy<Values>>)
auto operator-(const T& lhs, KeySelectionProxy<Values>&& rhs)
{
  return rhs - lhs;
}

template<typename T, typename Values>
requires (!std::same_as<std::decay_t<T>, KeySelectionProxy<Values>>)
auto operator*(const T& lhs, KeySelectionProxy<Values>&& rhs)
{
  return rhs * lhs;
}


//type printing
template <typename T>
std::string type_name() {
#if defined(__clang__)
    std::string name = __PRETTY_FUNCTION__;
    return name.substr(name.find('=') + 2, name.rfind(']') - name.find('=') - 2);
#elif defined(__GNUC__)
    std::string name = __PRETTY_FUNCTION__;
    return name.substr(name.find('=') + 2, name.rfind(';') - name.find('=') - 2);
#else
    return typeid(T).name();
#endif
}

/*
cmake_minimum_required(VERSION 3.10)

project(testmap)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_MODULE_PATH}")
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_PREFIX_PATH}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")



add_executable(testmap testmap.cpp)

int main() {
    using MyTypes = Types<int, double, Eigen::ArrayXXd, Eigen::ArrayXd>;
    PolyMap<MyTypes> mapa;
    PolyMap<MyTypes> mapb;

    mapa["a"] = 10;
    mapa["b"] = 20;
    mapa["c"] = 3.5;

    mapa["cheese"] = 5.0;
    //mapa("cheese", "a") += mapa("cheese", "a");

    mapb["a"] = 10;
    mapa.get<int>("a");
    Eigen::ArrayXXd tmp = Eigen::ArrayXXd::Ones(6, 6);
    mapa["cheese"] = tmp;
    mapb["cheese"] = tmp;

    //mapb("cheese", "a") = mapa("cheese", "a") + 2 + 3 + 102;
    //mapa("c", "b") = 5 + mapa("a", "b") / 10;

     mapa("a", "cheese") += mapb("a", "cheese"); //operation is ordered according to the slice!

    Eigen::ArrayXXd tmp1 = (Eigen::ArrayXXd::Ones(6,6) + Eigen::ArrayXXd::Ones(6,6)).eval();
    for (const auto& [key, val] : mapa.polyMap) {
        std::cout << key << " = ";
        std::visit([](auto&& v) {
            std::cout << v << '\n';
        }, val.value);
    }

    for (const auto& [key, val] : mapb.polyMap) {
        std::cout << key << " = ";
        std::visit([](auto&& v) {
            std::cout << v << '\n';
        }, val.value);
    }

    return 0;
}
*/