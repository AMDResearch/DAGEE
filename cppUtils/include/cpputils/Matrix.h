// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_MATRIX_H_
#define INCLUDE_CPPUTILS_MATRIX_H_

#include <cassert>
#include <cstddef>

#include <vector>
#include <type_traits>

namespace cpputils {

template <typename T, bool ROW_MAJOR=true>
class Matrix {

public:

  using size_type = std::size_t;
  using value_type = T;

  constexpr static const bool IS_ROW_MAJOR = ROW_MAJOR;
  constexpr static const bool IS_COL_MAJOR = !ROW_MAJOR;

  using rowMajor = Matrix<T, true>;
  using colMajor = Matrix<T, false>;

protected:

  T* mPtr;
  size_t mLdDimSz;
  size_t mCols;
  size_t mRows;

  template <typename V, bool RM = ROW_MAJOR>
  typename std::enable_if<RM, V&>::type get(size_t i , size_t j) const {
    return mPtr[i*mLdDimSz + j];
  }

  template <typename V, bool RM = ROW_MAJOR>
  typename std::enable_if<!RM, V&>::type get(size_t i , size_t j) const {
    return mPtr[j*mLdDimSz + i];
  }
public:

  Matrix(T* ptr, size_t rows, size_t cols, size_t lDimSz=0) 
    : 
      mPtr(ptr),
      mLdDimSz(lDimSz),
      mCols(cols),
      mRows(rows)
  { 
    assert(ptr && "Matrix initialized with nullptr");
    assert(rows > 0 && "invalid row size");
    assert(cols > 0 && "invalid col size");

    if (mLdDimSz == 0) {
      mLdDimSz = IS_ROW_MAJOR ? mCols: mRows;
    }
  }

  const value_type& operator()(size_t i , size_t j) const {
    return get<const value_type>(i, j);
  }

  value_type& operator()(size_t i , size_t j) {
    return get<value_type>(i, j);
  }

  T* ptr(void) { return mPtr; }

  const T* ptr(void) const { return mPtr; }

  size_t ldDimSz(void) const { return mLdDimSz; }

  size_t rows(void) const { return mRows; }

  size_t cols(void) const { return mCols; }

  size_t numElements(void) const { return mRows * mCols; }

  //  <------------------ mCols ------------>
  // ^<--- mColOffset---><---mSubCols--->
  // .     ^
  // .     .
  // .     .
  // .     mRowOffset
  // .     .
  // .     v
  // .     ^
  // mRows .
  // .     .
  // .     mSubRows
  // .     .
  // .     v
  // v

  // TODO: define copyability of matrix. Is it cheap to copy or expensive?
  Matrix getSubMat(size_t rowOffset, size_t colOffset, size_t subRows, size_t subCols) {

    assert(rowOffset < rows() && "invalid row offset");
    assert(colOffset < cols() && "invalid col offset");

    if (subRows > (rows() - rowOffset)) {
      subRows = rows() - rowOffset;
    }
    if (subCols > (cols() - subCols)) {
      subCols = cols() - colOffset;
    }

    T* ptr = &((*this)(rowOffset, colOffset));

    return Matrix(ptr, subRows, subCols, ldDimSz());
  }

  Matrix getSubMat(size_t rowOff, size_t colOff) {
    return getSubMat(rowOff, colOff, (rows() - rowOff), (cols() - colOff));
  }

};

template <typename Mat, size_t B=64>
class TiledMatrix {
  // Each contiguous B*B chunk in the array pointed to by mPtr is a tile
  // We treat it as a Matrix of mColTiles x mColTiles tiles, where each tile is B*B
  // in size 
  //
public:
  using TileMat = Mat;
  using value_type = typename TileMat::value_type;

  constexpr static const bool IS_ROW_MAJOR = TileMat::IS_ROW_MAJOR;
  constexpr static const bool IS_COL_MAJOR = TileMat::IS_COL_MAJOR;
  constexpr static const size_t TILE_SIZE = B;
  constexpr static const size_t TILE_AREA = B * B;


protected:
  using T = value_type;

  static size_t calcTiles(size_t dimSize) {
    return (dimSize + TILE_SIZE - 1) / TILE_SIZE;
  }

  T* mPtr;
  size_t mRowTiles;
  size_t mColTiles;
  size_t mRows;
  size_t mCols;


  template <typename M=TileMat, bool RM=IS_ROW_MAJOR>
  typename std::enable_if<RM, M>::type getTileImpl(size_t trow, size_t tcol) const {

    assert(trow < mRowTiles && tcol < mColTiles);
    T* tileBeg = &(mPtr[mColTiles * trow * TILE_AREA + tcol * TILE_AREA]);
    return M(tileBeg, TILE_SIZE, TILE_SIZE, TILE_SIZE);
  }

  template <typename M=TileMat, bool RM=IS_ROW_MAJOR>
  typename std::enable_if<!RM, M>::type getTileImpl(size_t trow, size_t tcol) const {

    assert(trow < mRowTiles && tcol < mColTiles);
    T* tileBeg = &(mPtr[mRowTiles * tcol * TILE_AREA + trow * TILE_AREA]);
    return M(tileBeg, TILE_SIZE, TILE_SIZE, TILE_SIZE);
  }

  template <typename V>
  V& index(size_t i, size_t j) const {
    size_t trow = i / TILE_SIZE;
    size_t tcol = j / TILE_SIZE;

    TileMat tileMat = getTileImpl(trow, tcol);

    size_t rowOff = i % TILE_SIZE;
    size_t colOff = j % TILE_SIZE;

    return tileMat(rowOff, colOff);
  }

public:
  static size_t storageSize(size_t rows, size_t cols) {

    size_t rowTiles = calcTiles(rows);
    size_t colTiles = calcTiles(cols);
    return rowTiles * colTiles * TILE_AREA;
  }


  
  TiledMatrix(T* ptr, size_t rows, size_t cols): 
    mPtr(ptr),
    mRowTiles(calcTiles(rows)),
    mColTiles(calcTiles(cols)),
    mRows(rows),
    mCols(cols)
  {
  }

  size_t rowTiles(void) const { return mRowTiles; }

  size_t colTiles(void) const { return mColTiles; }

  TileMat getTile(size_t i, size_t j) const {
    return getTileImpl(i, j);
  }

  T& operator() (size_t i, size_t j) {
    return index<T>(i, j);
  }

  const T& operator() (size_t i, size_t j) const {
    return index<const T>(i, j);
  }

  size_t rows(void) const { return mRows; }

  size_t cols(void) const { return mCols; }

  size_t numElements(void) const { return mRows * mCols; }
};

}// end namespace cpputils

#endif// INCLUDE_CPPUTILS_MATRIX_H_
