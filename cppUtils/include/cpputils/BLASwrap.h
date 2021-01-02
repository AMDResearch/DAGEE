// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_BLASWRAP_H_
#define INCLUDE_CPPUTILS_BLASWRAP_H_

#include "cpputils/Matrix.h"

#include <cstdlib>

#include <type_traits>
#include <vector>

extern "C" {
// google LAPACK doxygen & BLAS doxygen
extern void spotrf_(char* uplo, int* n, float* A, int* lda, int* info);
extern void dpotrf_(char* uplo, int* n, double* A, int* lda, int* info);

extern void sgemm_(char* transA, char* transB, int* m, int* n, int* k, float* alpha, float* A,
                   int* lda, float* B, int* ldb, float* beta, float* C, int* ldc);
extern void dgemm_(char* transA, char* transB, int* m, int* n, int* k, double* alpha, double* A,
                   int* lda, double* B, int* ldb, double* beta, double* C, int* ldc);

extern void ssyrk_(char* uplo, char* trans, int* n, int* k, float* alpha, float* A, int* lda,
                   float* beta, float* C, int* ldc);
extern void dsyrk_(char* uplo, char* trans, int* n, int* k, double* alpha, double* A, int* lda,
                   double* beta, double* C, int* ldc);

extern void strsm_(char* side, char* uplo, char* transA, char* diag, int* m, int* n, float* alpha,
                   float* A, int* lda, float* B, int* ldb);
extern void dtrsm_(char* side, char* uplo, char* transA, char* diag, int* m, int* n, double* alpha,
                   double* A, int* lda, double* B, int* ldb);
} // end extern "C"

namespace cpputils {

template <bool LOWER = true, bool TRANS_A = false, bool TRANS_B = false, bool UNIT_DIAG = false,
          bool LEFT_SIDE = true>
struct BLAS {
  using lower = BLAS<true, TRANS_A, TRANS_B, UNIT_DIAG, LEFT_SIDE>;
  using upper = BLAS<false, TRANS_A, TRANS_B, UNIT_DIAG, LEFT_SIDE>;
  using transA = BLAS<LOWER, true, TRANS_B, UNIT_DIAG, LEFT_SIDE>;
  using transB = BLAS<LOWER, TRANS_A, true, UNIT_DIAG, LEFT_SIDE>;
  using unitDiag = BLAS<LOWER, TRANS_A, TRANS_B, true, LEFT_SIDE>;
  using leftSide = BLAS<LOWER, TRANS_A, TRANS_B, UNIT_DIAG, true>;
  using rightSide = BLAS<LOWER, TRANS_A, TRANS_B, UNIT_DIAG, false>;

  // constexpr static const char f.uplo = LOWER ? 'L' : 'U';
  // constexpr static const char f.transA = TRANS_A ? 'T' : 'N';
  // constexpr static const char f.transB = TRANS_B ? 'T' : 'N';
  // constexpr static const char f.diag = UNIT_DIAG ? 'U' : 'N';
  // constexpr static const char f.side = LEFT_SIDE ? 'L' : 'R';

  struct Flags {
    char uplo = LOWER ? 'L' : 'U';
    char transA = TRANS_A ? 'T' : 'N';
    char transB = TRANS_B ? 'T' : 'N';
    char diag = UNIT_DIAG ? 'U' : 'N';
    char side = LEFT_SIDE ? 'L' : 'R';
  };

  template <typename T>
  static void checkIsFP(void) {
    static_assert(std::is_floating_point<T>::value, "T must be a floating point type");
  }

  template <typename T, typename F>
  static void gemm(int rowsC, int colsC, int k, T alpha, T beta, const T* A, int lda, const T* B,
                   int ldb, T* C, int ldc, const F& blasFunc) {
    T* _A = const_cast<T*>(A);
    T* _B = const_cast<T*>(B);

    Flags f;
    blasFunc(&f.transA, &f.transB, &rowsC, &colsC, &k, &alpha, _A, &lda, _B, &ldb, &beta, C, &ldc);
  }

  static void gemm(int rowsC, int colsC, int k, float alpha, float beta, const float* A, int lda,
                   const float* B, int ldb, float* C, int ldc) {
    gemm(rowsC, colsC, k, alpha, beta, A, lda, B, ldb, C, ldc, &sgemm_);
  }

  static void gemm(int rowsC, int colsC, int k, double alpha, double beta, const double* A, int lda,
                   const double* B, int ldb, double* C, int ldc) {
    gemm(rowsC, colsC, k, alpha, beta, A, lda, B, ldb, C, ldc, &dgemm_);
  }

  template <typename M, typename T = typename M::value_type>
  static void gemm(const M& A, const M& B, M& C, T alpha, T beta) {
    // TODO: put checks on dim
    int k = TRANS_A ? A.rows() : A.cols(); // TODO: verify
    gemm(C.rows(), C.cols(), k, alpha, beta, A.ptr(), A.ldDimSz(), B.ptr(), B.ldDimSz(), C.ptr(),
         C.ldDimSz());
  }

  template <typename T, typename F>
  static void trsm(int rowsB, int colsB, T alpha, const T* A, int lda, T* B, int ldb,
                   const F& blasFunc) {
    T* _A = const_cast<T*>(A);

    Flags f;
    blasFunc(&f.side, &f.uplo, &f.transA, &f.diag, &rowsB, &colsB, &alpha, _A, &lda, B, &ldb);
  }

  static void trsm(int rowsB, int colsB, float alpha, const float* A, int lda, float* B, int ldb) {
    trsm(rowsB, colsB, alpha, A, lda, B, ldb, &strsm_);
  }

  static void trsm(int rowsB, int colsB, double alpha, const double* A, int lda, double* B,
                   int ldb) {
    trsm(rowsB, colsB, alpha, A, lda, B, ldb, &dtrsm_);
  }

  template <typename M, typename T = typename M::value_type>
  static void trsm(const M& A, M& B, T alpha) {
    // TODO dim checks e.g. A.rows() == B.rows()

    trsm(B.rows(), B.cols(), alpha, A.ptr(), A.ldDimSz(), B.ptr(), B.ldDimSz());
  }

  template <typename T, typename F>
  static void syrk(int rowsC, int colsA, T alpha, T beta, const T* A, int lda, T* C, int ldc,
                   const F& blasFunc) {
    T* _A = const_cast<T*>(A);
    Flags f;

    blasFunc(&f.uplo, &f.transA, &rowsC, &colsA, &alpha, _A, &lda, &beta, C, &ldc);
  }

  static void syrk(int rowsC, int colsA, float alpha, float beta, const float* A, int lda, float* C,
                   int ldc) {
    syrk(rowsC, colsA, alpha, beta, A, lda, C, ldc, &ssyrk_);
  }

  static void syrk(int rowsC, int colsA, double alpha, double beta, const double* A, int lda,
                   double* C, int ldc) {
    syrk(rowsC, colsA, alpha, beta, A, lda, C, ldc, &dsyrk_);
  }

  template <typename M, typename T = typename M::value_type>
  static void syrk(const M& A, M& C, T alpha, T beta) {
    const int k = TRANS_A ? A.rows() : A.cols();

    if (TRANS_A) {
      assert(A.cols() == C.rows() && "row/col mismatch in matrix multiply");
    } else {
      assert(A.rows() == C.rows() && "row/col mismatch in matrix multiply");
    }

    syrk(C.rows(), k, alpha, beta, A.ptr(), A.ldDimSz(), C.ptr(), C.ldDimSz());
  }

  template <typename T, typename F>
  static int potrf(int n, T* A, int lda, const F& blasFunc) {
    Flags f;
    int info = -1;
    blasFunc(&f.uplo, &n, A, &lda, &info);
    return info;
  }

  static int potrf(int n, float* A, int lda) { return potrf(n, A, lda, &spotrf_); }

  static int potrf(int n, double* A, int lda) { return potrf(n, A, lda, &dpotrf_); }

  template <typename M>
  static int potrf(M& mat) {
    return potrf(mat.rows(), mat.ptr(), mat.ldDimSz());
  }
};

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_BLASWRAP_H_
