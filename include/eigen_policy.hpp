#pragma once
#include <Eigen/Core>
#include <complex>
#include <type_traits>

namespace pm_eigen {

  template<class T> using decay_t = std::remove_cv_t<std::remove_reference_t<T>>;

  template<class T>
  struct is_eigen_dense
    : std::bool_constant<std::is_base_of_v<Eigen::ArrayBase<decay_t<T>>, decay_t<T>> ||
                         std::is_base_of_v<Eigen::MatrixBase<decay_t<T>>, decay_t<T>>>
  {
  };

  template<class T> struct is_std_complex : std::false_type
  {
  };
  template<class U> struct is_std_complex<std::complex<U>> : std::true_type
  {
  };

  struct EigenArrayPolicy
  {
    template<class V, class OpT> static void add(V& a, const OpT& b)
    {
      if constexpr (is_eigen_dense<V>::value) {
        // Safe to refer to Scalar now
        using VS = typename decay_t<V>::Scalar;

        if constexpr (is_eigen_dense<OpT>::value && std::is_same_v<decay_t<V>, decay_t<OpT>>) {
          a += b; // same Eigen type
        }
        else if constexpr ((std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                           std::is_convertible_v<OpT, VS>) {
          a += static_cast<VS>(b); // Eigen += scalar/complex
        }
        else {
          throw std::runtime_error("Incompatible types for += (Eigen)");
        }
      }
      else if constexpr (std::is_arithmetic_v<decay_t<V>> && std::is_arithmetic_v<decay_t<OpT>> &&
                         std::is_convertible_v<OpT, decay_t<V>>) {
        a += b; // scalar += scalar
      }
      else if constexpr (is_std_complex<decay_t<V>>::value &&
                         (std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                         std::is_convertible_v<OpT, decay_t<V>>) {
        a += static_cast<decay_t<V>>(b); // complex += scalar/complex
      }
      else {
        throw std::runtime_error("Incompatible types for +=");
      }
    }

    template<class V, class OpT> static void sub(V& a, const OpT& b)
    {
      if constexpr (is_eigen_dense<V>::value) {
        using VS = typename decay_t<V>::Scalar;
        if constexpr (is_eigen_dense<OpT>::value && std::is_same_v<decay_t<V>, decay_t<OpT>>) a -= b;
        else if constexpr ((std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                           std::is_convertible_v<OpT, VS>)
          a -= static_cast<VS>(b);
        else throw std::runtime_error("Incompatible types for -=");
      }
      else if constexpr (std::is_arithmetic_v<decay_t<V>> && std::is_arithmetic_v<decay_t<OpT>> &&
                         std::is_convertible_v<OpT, decay_t<V>>)
        a -= b;
      else if constexpr (is_std_complex<decay_t<V>>::value &&
                         (std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                         std::is_convertible_v<OpT, decay_t<V>>)
        a -= static_cast<decay_t<V>>(b);
      else throw std::runtime_error("Incompatible types for -=");
    }

    template<class V, class OpT> static void mul(V& a, const OpT& b)
    {
      if constexpr (is_eigen_dense<V>::value) {
        using VS = typename decay_t<V>::Scalar;
        if constexpr (is_eigen_dense<OpT>::value && std::is_same_v<decay_t<V>, decay_t<OpT>>) a *= b;
        else if constexpr ((std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                           std::is_convertible_v<OpT, VS>)
          a *= static_cast<VS>(b);
        else throw std::runtime_error("Incompatible types for *=");
      }
      else if constexpr (std::is_arithmetic_v<decay_t<V>> && std::is_arithmetic_v<decay_t<OpT>> &&
                         std::is_convertible_v<OpT, decay_t<V>>)
        a *= b;
      else if constexpr (is_std_complex<decay_t<V>>::value &&
                         (std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                         std::is_convertible_v<OpT, decay_t<V>>)
        a *= static_cast<decay_t<V>>(b);
      else throw std::runtime_error("Incompatible types for *=");
    }

    template<class V, class OpT> static void div(V& a, const OpT& b)
    {
      if constexpr (is_eigen_dense<V>::value) {
        using VS = typename decay_t<V>::Scalar;
        if constexpr (is_eigen_dense<OpT>::value && std::is_same_v<decay_t<V>, decay_t<OpT>>) a /= b;
        else if constexpr ((std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                           std::is_convertible_v<OpT, VS>)
          a /= static_cast<VS>(b);
        else throw std::runtime_error("Incompatible types for /=");
      }
      else if constexpr (std::is_arithmetic_v<decay_t<V>> && std::is_arithmetic_v<decay_t<OpT>> &&
                         std::is_convertible_v<OpT, decay_t<V>>)
        a /= b;
      else if constexpr (is_std_complex<decay_t<V>>::value &&
                         (std::is_arithmetic_v<decay_t<OpT>> || is_std_complex<decay_t<OpT>>::value) &&
                         std::is_convertible_v<OpT, decay_t<V>>)
        a /= static_cast<decay_t<V>>(b);
      else throw std::runtime_error("Incompatible types for /=");
    }
  };

} // namespace pm_eigen
