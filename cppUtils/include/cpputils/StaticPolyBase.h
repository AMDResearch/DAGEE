// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_STATIC_POLY_BASE_H_
#define INCLUDE_CPPUTILS_STATIC_POLY_BASE_H_

namespace cpputils {

template <typename Derived>
struct StaticPolyBase {
  Derived* drvPtr(void) { return this; }

  const Derived* drvPtr(void) const { return this; }

  Derived& drvRef(void) { return *this; }

  const Derived& drvRef(void) const { return *this; }
};

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_STATIC_POLY_BASE_H_
