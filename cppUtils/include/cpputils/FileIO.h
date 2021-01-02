// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_FILE_IO_H_
#define INCLUDE_CPPUTILS_FILE_IO_H_

#include <fstream>
#include <iostream>
#include <vector>

namespace cpputils {

template <typename I>
void writeToFile(const std::string& filename, const I beg, const I end,
                 const char* const sep = " ") {
  std::ofstream fs(filename);

  if (fs.good()) {
    for (I i = beg; i != end; ++i) {
      fs << *i << sep;
    }
  } else {
    std::abort();
  }
  fs << std::endl;
}

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_FILE_IO_H_
