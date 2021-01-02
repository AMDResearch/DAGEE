// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_COMPILER_TRICKS_H_
#define INCLUDE_CPPUTILS_COMPILER_TRICKS_H_

namespace cpputils {

template <typename T>
void unusedVar(const T&) {}

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_COMPILER_TRICKS_H_
