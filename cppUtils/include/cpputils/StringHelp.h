// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_STRING_H_ELPER_H_
#define INCLUDE_CPPUTILS_STRING_H_ELPER_H_

#include <sstream>
#include <string>
#include <vector>

namespace cpputils {
template <typename C>
void splitCSVstr(const std::string& inputStr, C& output, const char delim = ',') {
  std::stringstream ss(inputStr);

  for (std::string item; std::getline(ss, item, delim);) {
    output.push_back(item);
  }
}

} // end namespace cpputils
#endif // INCLUDE_CPPUTILS_STRING_H_ELPER_H_
