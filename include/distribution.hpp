#pragma once

#include <type_traits>
#include <utility>
#include <variant>

#include <Eigen/Core>

#include <fileutils.hpp>
#include <forwardDecl.hpp>
#include <utils.hpp>

#define MAX_SPECTRUM_SIZE 100
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

template<typename T = Matrix> struct Distribution
{ // Linear distribution

  T values;

  Distribution(double xLeft, double xRight, size_t numPoints = MAX_SPECTRUM_SIZE)
  {
    if constexpr (std::is_same_v<T, Matrix>) {
      if (xLeft > xRight) { std::swap(xLeft, xRight); }
      Eigen::Index N = toIndexChecked(numPoints);
      values.resize(N, 1);
      values.col(0) = Vector::LinSpaced(N, xLeft, xRight);
    }
    else {
      static_assert(sizeof(T) == 0, "This constructor only supports Eigen::Matrix.");
    }
  }

  Distribution(T value) :
    values{std::move(value)}
  {}

  Distribution() = default;
};

struct NormalDistribution : public Distribution<>
{

  NormalDistribution(double xmin, double xmax, double x0, double sigma, size_t NumPoints = 20) :
    Distribution(xmin, xmax, NumPoints)
  {
    values.conservativeResize(values.rows(), values.cols() + 1);
    values.col(1) =
      (1.0 / std::sqrt(2 * M_PI * (sigma * sigma))) * (-0.5 * ((values.col(0) - x0) / sigma).square()).exp();
  }
};

struct FileDistribution : public Distribution<>
{

  FileDistribution(const std::string& filepath)
  {
    constexpr size_t ncols = 2;
    values = Data::loadFromFile(filepath, ncols);
    if (values.rows() > MAX_SPECTRUM_SIZE) downsample();
    Data::sortRowsByFirstColumn(values);
    normalize();
  }

private:
  void normalize()
  {
    auto x = values.col(0);
    auto y = values.col(1);
    const Eigen::Index N = values.rows();

    Vector dX = x.segment(1, N - 1) - x.segment(0, N - 1);
    double integral;
    if ((dX == dX(0)).all()) {
      // Uniform spacing
      integral = dX(0) * (0.5 * (values(0, 1) + values(N - 1, 1)) + y.segment(1, N - 2).sum());
    }
    else {
      integral = (0.5 * (y.segment(0, N - 1) + y.segment(1, N - 1)) * dX).sum();
    }

    if (integral > 0.0) values.col(1) /= integral;
  }

  void downsample()
  {
    const Eigen::Index sampleSize = MAX_SPECTRUM_SIZE;
    Eigen::Index stride = values.rows() / sampleSize;
    Matrix newValues = values(Eigen::seq(0, Eigen::last, stride), Eigen::all);
    values = newValues;
  }
};

namespace dist {
  // Utilities to make sure we can deduce Distribution correctly to intialize SDSweep
  // Distribution is something that has .values.
  template<class T>
  concept HasValuesMember = requires(T t) { t.values; };

  template<class Arg>
  using deduced_value_t = std::conditional_t<HasValuesMember<std::remove_cvref_t<Arg>>,
    std::remove_cvref_t<decltype(std::declval<Arg>().values)>,
    std::remove_cvref_t<Arg>>;

  // Raw value
  template<class V> Distribution<std::remove_cvref_t<V>> as_distribution(V&& v)
  {
    return Distribution<std::remove_cvref_t<V>>(std::forward<V>(v));
  }

  // Distribution object with .values
  template<HasValuesMember D> auto as_distribution(D&& d)
  {
    using V = std::remove_cvref_t<decltype(d.values)>;
    static_assert(std::is_same_v<V, double> || std::is_same_v<V, Matrix>,
      "Distribution Base: only Matrix or double values are supported.");

    using Base = Distribution<V>;

    if constexpr (std::is_base_of_v<Base, std::remove_cvref_t<D>>) {
      // slice
      return Base(std::forward<D>(d));
    }
    else {
      return Base(std::remove_cvref_t<decltype(d.values)>(d.values)); // just use the values
    }
  }

} // namespace dist

struct AnyDist
{
  // Type-erased holder to give to ConfigVisitor
  using Data =
    std::variant<double, Matrix, Distribution<double>, Distribution<Matrix>, NormalDistribution, FileDistribution>;

  Data data;

  AnyDist() = default;

  template<class T>
  AnyDist(T&& x) :
    data(std::forward<T>(x))
  {}
};