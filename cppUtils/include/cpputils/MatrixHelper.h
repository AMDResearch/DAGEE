// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_MATRIX_H_ELPER_H_
#define INCLUDE_CPPUTILS_MATRIX_H_ELPER_H_

#include "cpputils/Matrix.h"

#include <iostream>
#include <random>
#include <vector>

namespace cpputils {

template <typename M>
void printMatrix(const M& mat, const char* const msg) {
  std::printf("%s: \n", msg);
  for (size_t i = 0; i < mat.rows(); ++i) {
    std::printf("[ ");
    for (size_t j = 0; j < mat.cols(); ++j) {
      std::printf("%-10g", mat(i, j));
    }

    std::printf("]\n");
  }
}

template <typename M>
M transpose(M& mat) {
  for (size_t i = 0; i < mat.rows(); ++i) {
    for (size_t j = i + 1; j < mat.cols(); ++j) {
      std::swap(mat(i, j), mat(j, i));
    }
  }

  return M(mat.ptr(), mat.cols(), mat.rows());
}

template <typename M, typename T = typename M::value_type>
M genSymPosDefMat_Naive(size_t rows, std::vector<T>& backingVec) {
  backingVec.clear();
  backingVec.resize(rows * rows, 1.0);

  M mat(backingVec.data(), rows, rows);

  for (size_t i = 0; i < mat.rows(); ++i) {
    mat(i, i) = static_cast<float>(mat.rows());
  }

  return mat;
}

template <typename T>
cpputils::Matrix<T> genSameValMatrix(size_t rows, size_t cols, std::vector<T>& backingVec,
                                     const T& defaultVal = T()) {
  backingVec.clear();
  backingVec.resize(rows * cols, defaultVal);
  return cpputils::Matrix<T>(backingVec.data(), rows, cols);
}

template <typename T>
cpputils::Matrix<T> genRandMatrix(size_t rows, size_t cols, std::vector<T>& backingVec) {
  backingVec.clear();
  backingVec.resize(rows * cols, 1.0);

  cpputils::Matrix<T> M(backingVec.data(), rows, cols);

  static size_t seed = 0;
  constexpr float DIST_MIN = 0.0;
  constexpr float DIST_MAX = 10.0;

  std::mt19937 rng(seed++);
  std::uniform_real_distribution<float> dist(DIST_MIN, DIST_MAX);

  for (size_t i = 0; i < M.rows(); ++i) {
    for (size_t j = 0; j < M.cols(); ++j) {
      M(i, j) = dist(rng);
    }
  }

  return M;
}

template <typename M>
void convToLowerTri(M& Mat) {
  for (size_t i = 0; i < Mat.rows(); ++i) {
    for (auto j = i + 1; j < Mat.cols(); ++j) {
      Mat(i, j) = 0; // relying on automatic upcasting to float/double
    }
  }
}

/**
 * ErrFunc object returns true if values mismatch according to some criterion
 */
template <typename M1, typename M2, typename ErrFunc>
bool compareMatrices(const M1& m1, const M2& m2, const ErrFunc& err) {
  if (m1.rows() != m2.rows() || m1.cols() != m2.cols()) {
    std::cerr << "ERROR: mismatch in size" << std::endl;
  }
  static_assert(std::is_same<typename M1::value_type, typename M2::value_type>::value,
                "M1 and M2 matrices should have the same value_type");

  bool passed = true;

  for (size_t i = 0; i < m1.rows(); ++i) {
    for (size_t j = 0; j < m1.cols(); ++j) {
      if (err(m1(i, j), m2(i, j))) {
        passed = false;
        std::cerr << "ERROR: error at index (" << i << "," << j << "), m1's value = " << m1(i, j)
                  << ", m2's value = " << m2(i, j) << std::endl;
      }
    }
  }

  if (passed) {
    std::cout << "OK: Matrix comparison passed" << std::endl;
  } else {
    std::cerr << "ERROR: Matrix comparison failed" << std::endl;
  }

  return passed;
}

/**
 * A basic routine
 * for copying matrices where source and destination have different
 * representations
 */
template <typename M1, typename M2>
void copyMatrix(M2& dst, const M1& src) {
  assert(src.rows() == dst.rows() && src.cols() == dst.cols());

  for (size_t i = 0; i < src.rows(); ++i) {
    for (size_t j = 0; j < src.cols(); ++j) {
      dst(i, j) = src(i, j);
    }
  }
}

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_MATRIX_H_ELPER_H_
