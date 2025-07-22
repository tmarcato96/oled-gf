/*! \file forwardDecl.hpp
    \brief A simple header used to include forward declarations and typedefs.
*/
#pragma once

#include <Eigen/Core>


// Global typedefs
typedef Eigen::ArrayXXcd CMatrix;
typedef Eigen::ArrayXcd CVector;
typedef Eigen::Array<std::complex<double>, 1, Eigen::Dynamic> CTVector;
typedef Eigen::ArrayXXd Matrix;
typedef Eigen::ArrayXd Vector;
typedef Eigen::Array<double, 1, Eigen::Dynamic> TVector;