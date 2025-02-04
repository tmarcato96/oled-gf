/*! \file linalg.hpp
    \brief A file that implements some simple and useful arithmetic using derived templating. 
*/
#pragma once

#include <Eigen/Core>
#include <cmath>
#include <functional>
#include <numeric>
#include "forwardDecl.hpp"

template<typename Derived>
typename Eigen::DenseBase<Derived>::RandomAccessLinSpacedReturnType arange(typename Derived::Scalar start,
  typename Derived::Scalar stop,
  typename Derived::Scalar step,
  bool with_last = false)
{
  stop -= (with_last) ? 0 : step;
  int N = static_cast<int>(std::floor((stop - start) / step) + 1);
  return Eigen::DenseBase<Derived>::LinSpaced(N, start, stop);
}
