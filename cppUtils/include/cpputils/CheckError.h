// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_CHECK_ERROR_H_
#define INCLUDE_CPPUTILS_CHECK_ERROR_H_

#include "cpputils/Print.h"

#include <cmath>

#include <cassert>
#include <iostream>

namespace cpputils {

template <typename... Args>
void failIf(const bool cond, const Args&... args) {
  if (cond) {
    cpputils::printErr(args...);
    std::abort();
  }
}

template <typename T>
struct AbsoluteError {
  const T threshold;

  bool operator()(const T& refVal, const T& measured) const {
    if (std::fabs(measured - refVal) > std::fabs(threshold)) {
      return true;
    }
    return false;
  }
};

template <typename T>
struct RelativeError {
  const T threshold;

  bool operator()(const T& refVal, const T& measured) const {
    T relErr = (measured - refVal) / refVal;
    if (std::fabs(relErr) > std::fabs(threshold)) {
      return true;
    }
    return false;
  }
};

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_CHECK_ERROR_H_
