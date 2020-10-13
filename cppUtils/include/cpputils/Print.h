// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_PRINT_H_
#define INCLUDE_CPPUTILS_PRINT_H_

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace cpputils {

template <typename O, typename... Args>
O& print(O& out, const Args&... args) {
  (void) (int[]) { (out << args, 0)... };
  return out;
}

template <typename O, typename... Args>
O& printLn(O& out, const Args&... args) {
  cpputils::print(out, args...);
  out << std::endl;
  return out;
}

template <typename... Args>
void printOut(const Args&... args) {
  cpputils::printLn(std::cout, args...);
}

template <typename... Args>
void printErr(const Args&... args) {
  cpputils::printLn(std::cerr, args...);
}

template <typename... Args>
std::string printStr(const Args&... args) {
  std::ostringstream ss;
  cpputils::print(ss, args...);
  return ss.str();
}


/**
 * \param region is name of the application or a specific region of the application
 * \param category is the type of stat being printed
 * \param val is the value of the stat
 */
template <typename S1, typename S2, typename V>
void printStat(const S1& region, const S2& category, const V& val) {
  printOut("STAT, ", region, ", ", category, ", ", val);
}

/**
 * \param region is name of the application or a specific region of the application
 * \param category is the type of parameter being printed
 * \param val is the value of the parameter
 */
template <typename S1, typename S2, typename V>
void printParam(const S1& region, const S2& category, const V& val) {
  printOut("PARAM, ", region, ", ", category, ", ", val);
}

}// end namespace cpputils

#endif// INCLUDE_CPPUTILS_PRINT_H_
