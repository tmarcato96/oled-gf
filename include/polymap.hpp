#pragma once

#include <complex>
#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

// Helpers
namespace pm {
  template<class T> using decay_t = std::remove_cv_t<std::remove_reference_t<T>>;

  template<class T> struct is_std_complex : std::false_type
  {
  };
  template<class U> struct is_std_complex<std::complex<U>> : std::true_type
  {
  };
} // namespace pm

// Helper traits for variant
template<class T, class Variant> struct is_alternative_of_variant : std::false_type
{
};

template<class T, class... Us>
struct is_alternative_of_variant<T, std::variant<Us...>> : std::disjunction<std::is_same<std::remove_cvref_t<T>, Us>...>
{
};

template<class T, class Variant>
inline constexpr bool is_alternative_of_variant_v = is_alternative_of_variant<T, Variant>::value;

// Operation default policy
struct DefaultOpPolicy
{
  template<class V, class OpT> static void add(V& a, const OpT& b)
  {
    if constexpr (std::is_same_v<pm::decay_t<V>, pm::decay_t<OpT>>) { a += b; }
    else if constexpr (std::is_arithmetic_v<pm::decay_t<V>> && std::is_arithmetic_v<pm::decay_t<OpT>> &&
                       std::is_convertible_v<OpT, pm::decay_t<V>>) {
      a += b;
    }
    else if constexpr (pm::is_std_complex<pm::decay_t<V>>::value &&
                       (std::is_arithmetic_v<pm::decay_t<OpT>> || pm::is_std_complex<pm::decay_t<OpT>>::value) &&
                       std::is_convertible_v<OpT, pm::decay_t<V>>) {
      a += static_cast<pm::decay_t<V>>(b); // e.g., complex += double
    }
    else {
      throw std::runtime_error("Incompatible types for += operation");
    }
  }

  template<class V, class OpT> static void sub(V& a, const OpT& b)
  {
    if constexpr (std::is_same_v<pm::decay_t<V>, pm::decay_t<OpT>>) { a -= b; }
    else if constexpr (std::is_arithmetic_v<pm::decay_t<V>> && std::is_arithmetic_v<pm::decay_t<OpT>> &&
                       std::is_convertible_v<OpT, pm::decay_t<V>>) {
      a -= b;
    }
    else if constexpr (pm::is_std_complex<pm::decay_t<V>>::value &&
                       (std::is_arithmetic_v<pm::decay_t<OpT>> || pm::is_std_complex<pm::decay_t<OpT>>::value) &&
                       std::is_convertible_v<OpT, pm::decay_t<V>>) {
      a -= static_cast<pm::decay_t<V>>(b);
    }
    else {
      throw std::runtime_error("Incompatible types for -= operation");
    }
  }

  template<class V, class OpT> static void mul(V& a, const OpT& b)
  {
    if constexpr (std::is_same_v<pm::decay_t<V>, pm::decay_t<OpT>>) { a *= b; }
    else if constexpr ((std::is_arithmetic_v<pm::decay_t<V>> || pm::is_std_complex<pm::decay_t<V>>::value) &&
                       (std::is_arithmetic_v<pm::decay_t<OpT>> || pm::is_std_complex<pm::decay_t<OpT>>::value) &&
                       std::is_convertible_v<OpT, pm::decay_t<V>>) {
      a *= static_cast<pm::decay_t<V>>(b);
    }
    else {
      throw std::runtime_error("Incompatible types for *= operation");
    }
  }

  template<class V, class OpT> static void div(V& a, const OpT& b)
  {
    if constexpr (std::is_same_v<pm::decay_t<V>, pm::decay_t<OpT>>) { a /= b; }
    else if constexpr ((std::is_arithmetic_v<pm::decay_t<V>> || pm::is_std_complex<pm::decay_t<V>>::value) &&
                       (std::is_arithmetic_v<pm::decay_t<OpT>> || pm::is_std_complex<pm::decay_t<OpT>>::value) &&
                       std::is_convertible_v<OpT, pm::decay_t<V>>) {
      a /= static_cast<pm::decay_t<V>>(b);
    }
    else {
      throw std::runtime_error("Incompatible types for /= operation");
    }
  }
};

// Placeholders
struct All_t
{
};

// fwd declarations and aliases
template<typename Values, typename Policy> struct KeySelectionProxy;

template<typename... Ts> struct Types
{
  using map_types = std::variant<Ts...>;
};

// function generators
template<typename Values, typename Policy, typename T> auto makeSumOp(T&& operand)
{
  using value_type = typename Values::map_types;
  using OpT = std::decay_t<T>;
  auto ptr = std::make_shared<OpT>(std::forward<T>(operand));
  return [ptr](value_type& val) {
    std::visit(
      [&](auto& v) {
        using V = std::decay_t<decltype(v)>;
        Policy::template add<V, OpT>(v, *ptr);
      },
      val);
  };
}

template<typename Values, typename Policy, typename T> auto makeSubOp(T&& operand)
{
  using value_type = typename Values::map_types;
  using OpT = std::decay_t<T>;
  auto ptr = std::make_shared<OpT>(std::forward<T>(operand));
  return [ptr](value_type& val) {
    std::visit(
      [&](auto& v) {
        using V = std::decay_t<decltype(v)>;
        Policy::template sub<V, OpT>(v, *ptr);
      },
      val);
  };
}

template<typename Values, typename Policy, typename T> auto makeMultOp(T&& operand)
{
  using value_type = typename Values::map_types;
  using OpT = std::decay_t<T>;
  auto ptr = std::make_shared<OpT>(std::forward<T>(operand));
  return [ptr](value_type& val) {
    std::visit(
      [&](auto& v) {
        using V = std::decay_t<decltype(v)>;
        Policy::template mul<V, OpT>(v, *ptr);
      },
      val);
  };
}

template<typename Values, typename Policy, typename T> auto makeDivOp(T&& operand)
{
  using value_type = typename Values::map_types;
  using OpT = std::decay_t<T>;
  auto ptr = std::make_shared<OpT>(std::forward<T>(operand));
  return [ptr](value_type& val) {
    std::visit(
      [&](auto& v) {
        using V = std::decay_t<decltype(v)>;
        Policy::template div<V, OpT>(v, *ptr);
      },
      val);
  };
}

template<typename Values, typename Policy = DefaultOpPolicy> struct PolyMap
{
  using value_type = typename Values::map_types;
  std::map<std::string, value_type, std::less<>> polyMap;
  static inline constexpr All_t All{};

  value_type& operator[](const std::string& key) { return polyMap[key]; }

  const value_type& operator[](const std::string& key) const { return polyMap.at(key); }

  template<typename T> const T& get(const std::string& key) const { return std::get<T>(polyMap.at(key)); }

  template<typename T> T& get(const std::string& key) { return std::get<T>(polyMap[key]); }

  template<typename... Keys> auto operator()(Keys&&... keys)
  {
    static_assert((std::is_convertible_v<Keys, std::string> && ...), "All keys must be convertible to std::string");

    std::vector<std::string> keyVec = {std::forward<Keys>(keys)...};

    for (const auto& key : keyVec) {
      if (polyMap.find(key) == polyMap.end()) { throw std::runtime_error("Key not found in polyMap: " + key); }
    }

    return KeySelectionProxy<Values, Policy>(this, std::move(keyVec));
  }

  auto operator()(All_t)
  {
    std::vector<std::string> allKeys;
    for (const auto& [key, _] : polyMap) allKeys.push_back(key);
    return KeySelectionProxy<Values, Policy>(this, allKeys);
  }

  auto operator()() { return (*this)(All); }

  template<typename... Keys> void initializeFromWithZeros(const PolyMap& ref, Keys&&... keys)
  {
    static_assert((std::is_convertible_v<Keys, std::string> && ...), "All keys must be convertible to std::string");

    std::vector<std::string> keyVec{std::forward<Keys>(keys)...};

    for (const auto& k : keyVec) {
      auto rit = ref.polyMap.find(k);
      if (rit == ref.polyMap.end()) throw std::runtime_error("Key not found in reference map: " + k);
      polyMap[k] = rit->second; // creates or overwrites
    }

    (*this)(keys...).setZero();
  }

  template<typename... Keys> void insert(const value_type& val, Keys&&... keys)
  {
    static_assert((std::is_convertible_v<Keys, std::string> && ...), "All keys must be convertible to std::string");

    for (const auto k : {keys...}) { polyMap[k] = val; }
  }

  template<typename Target, typename... Keys> void insertAs(Target&& expr, Keys&&... keys)
  {
    using altT = std::remove_cvref_t<Target>;
    static_assert(
      is_alternative_of_variant_v<altT, value_type>, "insert_as<T>: T must be one of the variant alternatives");

    value_type v{std::in_place_type<altT>, altT(std::forward<Target>(expr))};
    ((polyMap[std::string(std::forward<Keys>(keys))] = v), ...);
  }
};

template<typename Values, typename Policy = DefaultOpPolicy> struct KeySelectionProxy
{
  using value_type = typename Values::map_types;
  using Operation = std::function<void(value_type&)>;
  using Ops = std::vector<std::pair<const std::string*, Operation>>;

  PolyMap<Values, Policy>* const source;
  std::vector<std::string> keys;
  Ops operations;

  explicit KeySelectionProxy(PolyMap<Values, Policy>* const src,
    std::vector<std::string> keys_,
    std::vector<std::pair<const std::string*, Operation>> ops = {}) :
    source{src},
    keys{std::move(keys_)},
    operations{std::move(ops)}
  {}

  explicit KeySelectionProxy(const KeySelectionProxy<Values, Policy>& other) :
    source{other.source},
    keys{other.keys},
    operations{other.operations}
  {}

  // Assignment semantics
  // Always copy for assignment
  KeySelectionProxy<Values, Policy>& operator=(const KeySelectionProxy<Values, Policy>& other)
  {
    return assignFrom(other, AssignMode::Copy);
  }

  KeySelectionProxy<Values, Policy>& operator=(KeySelectionProxy<Values, Policy>&& other)
  {
    return assignFrom(other, AssignMode::Copy);
  }

  // Explicit destructive move
  KeySelectionProxy<Values, Policy>& moveFrom(KeySelectionProxy<Values, Policy>&& other)
  {
    return assignFrom(other, AssignMode::Move);
  }

  // basic operators
  template<typename T> KeySelectionProxy operator+(T&& increment)
  {
    operations.emplace_back(nullptr, makeSumOp<Values, Policy>(std::forward<T>(increment)));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values, Policy> operator+(KeySelectionProxy<Values, Policy>&& other)
  {
    return make_operator(ProxyBinOp::sum, other);
  }

  template<typename T> KeySelectionProxy& operator+=(T&& increment)
  {
    operations.emplace_back(nullptr, makeSumOp<Values, Policy>(std::forward<T>(increment)));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values, Policy>& operator+=(KeySelectionProxy<Values, Policy>&& other)
  {
    *this = std::move(*this + std::forward<KeySelectionProxy<Values, Policy>>(other));
    return *this;
  }

  template<typename T> KeySelectionProxy operator-(T&& decrement)
  {
    operations.emplace_back(nullptr, makeSubOp<Values, Policy>(std::forward<T>(decrement)));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values, Policy> operator-(KeySelectionProxy<Values, Policy>&& other)
  {
    return make_operator(ProxyBinOp::sub, other);
  }

  template<typename T> KeySelectionProxy& operator-=(T&& decrement)
  {
    operations.emplace_back(nullptr, makeSubOp<Values, Policy>(std::forward<T>(decrement)));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values, Policy>& operator-=(KeySelectionProxy<Values, Policy>&& other)
  {
    *this = std::move(*this - std::forward<KeySelectionProxy<Values, Policy>>(other));
    return *this;
  }

  template<typename T> KeySelectionProxy operator*(T&& factor)
  {
    operations.emplace_back(nullptr, makeMultOp<Values, Policy>(std::forward<T>(factor)));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values, Policy> operator*(KeySelectionProxy<Values, Policy>&& other)
  {
    return make_operator(ProxyBinOp::mult, other);
  }

  template<typename T> KeySelectionProxy& operator*=(T&& factor)
  {
    operations.emplace_back(nullptr, makeMultOp<Values, Policy>(std::forward<T>(factor)));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values, Policy>& operator*=(KeySelectionProxy<Values, Policy>&& other)
  {
    *this = std::move(*this * std::forward<KeySelectionProxy<Values, Policy>>(other));
    return *this;
  }

  template<typename T> KeySelectionProxy operator/(T&& denominator)
  {
    operations.emplace_back(nullptr, makeDivOp<Values, Policy>(std::forward<T>(denominator)));
    return KeySelectionProxy(source, keys, std::move(operations));
  }

  KeySelectionProxy<Values, Policy> operator/(KeySelectionProxy<Values, Policy>&& other)
  {
    return make_operator(ProxyBinOp::div, other);
  }

  template<typename T> KeySelectionProxy& operator/=(T&& denominator)
  {
    operations.emplace_back(nullptr, makeDivOp<Values, Policy>(std::forward<T>(denominator)));
    evaluate();
    operations.clear();
    return *this;
  }

  KeySelectionProxy<Values, Policy>& operator/=(KeySelectionProxy<Values, Policy>&& other)
  {
    *this = std::move(*this / std::forward<KeySelectionProxy<Values, Policy>>(other));
    return *this;
  }

  operator PolyMap<Values, Policy>&() { return evaluate(); }

  KeySelectionProxy& setZero()
  {

    auto zeroOp = [](value_type& val) {
      std::visit(
        [](auto& v) {
          if constexpr (requires { v.setZero(); }) { v.setZero(); }
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
  PolyMap<Values, Policy>& evaluate()
  {
    for (const auto& [opKey, op] : operations) {
      if (opKey == nullptr) {
        for (const auto& key : keys) {
          auto it = source->polyMap.find(key);
          if (it == source->polyMap.end()) continue;

          try {
            op(it->second);
          } catch (const std::exception& e) {
            std::cerr << "Operation failed on key [" << key << "]: " << e.what() << '\n';
          }
        }
      }
      else {
        try {
          auto it = source->polyMap.find(*opKey);
          if (it == source->polyMap.end()) throw std::runtime_error("Key not found in map");
          op(it->second);
        } catch (const std::exception& e) {
          std::cerr << "Operation failed on key [" << *opKey << "]: " << e.what() << '\n';
        }
      }
    }
    return *source;
  }

  // helper function
  enum class ProxyBinOp { sum, sub, mult, div };

  KeySelectionProxy<Values, Policy> make_operator(ProxyBinOp op, const KeySelectionProxy<Values, Policy>& operand)
  {
    assert(operand.source != nullptr);
    if (this->keys.size() != operand.keys.size()) {
      throw std::runtime_error("Binary proxy operators require same number of keys");
    }

    auto* operand_src = operand.source;
    auto operand_ops_copy = operand.operations;

    for (size_t i = 0; i < keys.size(); ++i) {
      // const std::string &key = keys[i];
      // const std::string &operandKey = operand.keys[i];

      auto it = source->polyMap.find(keys[i]);
      if (it == source->polyMap.end()) continue;

      std::string operandKey = operand.keys[i];
      const std::string* keyPtr = &it->first;

      Operation opFunc = [op, operand_src, operandKey, ops = std::optional<Ops>{std::move(operand_ops_copy)}](
                           value_type& lhsVal) mutable {
        auto itoperand = operand_src->polyMap.find(operandKey);
        if (itoperand == operand_src->polyMap.end()) return;

        value_type operandVal = itoperand->second;

        // Chain all previous operations
        if (ops) {
          for (const auto& [rhsOpKey, rhsOp] : *ops) {
            if (!rhsOpKey || *rhsOpKey == operandKey) { rhsOp(operandVal); }
          }
          ops.reset();
        }

        std::visit(
          [&](const auto& inner_val) {
            switch (op) { // picks the right operation for the lambda
            case ProxyBinOp::sum: makeSumOp<Values, Policy>(inner_val)(lhsVal); break;
            case ProxyBinOp::sub: makeSubOp<Values, Policy>(inner_val)(lhsVal); break;
            case ProxyBinOp::mult: makeMultOp<Values, Policy>(inner_val)(lhsVal); break;
            case ProxyBinOp::div: makeDivOp<Values, Policy>(inner_val)(lhsVal); break;
            }
          },
          operandVal);
      };
      operations.emplace_back(keyPtr, std::move(opFunc));
    }
    return KeySelectionProxy<Values, Policy>(source, keys, std::move(operations));
  }

  bool hasDuplicates(const std::vector<std::string>& ks)
  {
    std::unordered_set<std::string> s;
    s.reserve(ks.size());
    for (const auto& k : ks) {
      if (!s.insert(k).second) return true;
    }
    return false;
  }

  void applyOpsAtKey(const std::string& key, const Ops& ops, value_type& v)
  {
    for (const auto& [kptr, op] : ops) {
      if (!kptr || *kptr == key) op(v);
    }
  }

  enum class AssignMode { Copy, Move };

  KeySelectionProxy<Values, Policy>& assignFrom(const KeySelectionProxy& other, AssignMode mode)
  {
    if (keys.size() != other.keys.size())
      throw std::runtime_error("KeySelectionProxy assignment requires matching key counts");

    // Check for duplication
    if (hasDuplicates(keys) || hasDuplicates(other.keys)) throw std::runtime_error("Duplicate keys not allowed!");

    // Move only supports pure slices
    if (mode == AssignMode::Move) {
      if (!other.operations.empty()) throw std::runtime_error("Move assignment requires a pure slice!");
    }

    // Validate and collect all iterators
    using Map = decltype(source->polyMap);
    using iterator = typename Map::iterator;

    std::vector<iterator> dstIts, srcIts;
    dstIts.reserve(keys.size());
    srcIts.reserve(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
      auto itDst = source->polyMap.find(keys[i]);
      auto itSrc = other.source->polyMap.find(other.keys[i]);
      if (itDst == source->polyMap.end() || itSrc == other.source->polyMap.end())
        throw std::runtime_error("Key not found!");
      dstIts.push_back(itDst);
      srcIts.push_back(itSrc);
    }

    // Stage RHS for exception safety
    std::vector<value_type> staged;
    staged.reserve(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
      value_type v;

      if (mode == AssignMode::Move) {
        // Destructive steals buffer
        v = std::move(srcIts[i]->second);
      }
      else {
        v = srcIts[i]->second;
        applyOpsAtKey(other.keys[i], other.operations, v);
      }

      staged.emplace_back(std::move(v));
    }

    // Commit results to LHS
    for (size_t i = 0; i < keys.size(); ++i) { dstIts[i]->second = std::move(staged[i]); }

    return *this;
  }
};

// out of class operator definitions to allow for lhs operations
template<typename T, typename Values, typename Policy>
  requires(!std::same_as<std::decay_t<T>, KeySelectionProxy<Values, Policy>>)
auto operator+(const T& lhs, KeySelectionProxy<Values, Policy>&& rhs)
{
  return rhs + lhs; // Uses the existing member operator
}

template<typename T, typename Values, typename Policy>
  requires(!std::same_as<std::decay_t<T>, KeySelectionProxy<Values, Policy>>)
auto operator-(const T& lhs, KeySelectionProxy<Values, Policy>&& rhs)
{
  return rhs - lhs;
}

template<typename T, typename Values, typename Policy>
  requires(!std::same_as<std::decay_t<T>, KeySelectionProxy<Values, Policy>>)
auto operator*(const T& lhs, KeySelectionProxy<Values, Policy>&& rhs)
{
  return rhs * lhs;
}

template<typename Types> std::ostream& operator<<(std::ostream& os, const PolyMap<Types>& m)
{
  os << "{\n";
  for (const auto& [k, v] : m.polyMap) {
    os << k << ": { ";
    std::visit([&](auto& val) { os << val; }, v);
    os << "},\n";
  }
  os << "}\n";
  return os;
}
