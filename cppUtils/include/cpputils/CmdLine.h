// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_CMD_LINE_H_
#define INCLUDE_CPPUTILS_CMD_LINE_H_

#include <vector>
#include <unordered_map>
#include <string>
#include <initializer_list>
#include <iostream>
#include <algorithm>
#include <sstream>

#include <cassert>

#define CMD_LINE_ENUM_VAL(X) { #X, X }

namespace cpputils {

// TODO: Handle Enum options
// - User provides a char string to enum map (may be through a macro)
//

namespace cmdline {


class OptionBase {
protected:
  char mName;
  std::string mHelpStr;

  OptionBase(char name, const std::string& help): mName(name), mHelpStr (help) 
  {}

  virtual ~OptionBase() {}

public:

  const std::string& helpStr(void) const { return mHelpStr; }

  virtual bool needsVal(void) const = 0;

  virtual void initVal(const std::string& val) = 0;

  char name(void) const { return mName; }
};

template <typename T>
class Option: public OptionBase {
  T mVal;

public:

  Option (char name, const std::string& help, const T& defval=T()): OptionBase(name, help), mVal(defval) {}

  operator const T& (void) const {
    return mVal;
  }

  const T& val(void) const {
    return mVal;
  }


  virtual bool needsVal(void) const { return true; }

  virtual void initVal(const std::string& val) {

    std::istringstream ss(val);
    char dummy;

    ss >> mVal;

    if (ss.fail() || ss.get(dummy)) { // "123abc" is parsed into int as "123", so we check for remaining chars
      std::cerr << "Bad value: " << val << " for option: " << mName << std::endl;
      std::abort();
    }
  }
};

template<> class Option<bool>: public OptionBase {
  bool mVal;

public:
  Option(char name, const std::string& help, const bool defval=false): OptionBase(name, help), mVal(defval) {}

  operator bool (void) const { return mVal; }
  
  virtual bool needsVal(void) const { return false; }

  // only called when option found
  virtual void initVal(const std::string&) {
    mVal = true;
  }
};

template <typename E>
class EnumOption: public OptionBase {

  using StrEnumMap = std::unordered_map<std::string, E>;
  using StrEnumPair = typename StrEnumMap::value_type;

  E mVal;
  StrEnumMap mStrEnumMap;

public:

  EnumOption(char name, const std::string& help, const E& defval, std::initializer_list<StrEnumPair> strEnumPairs)
    :
      OptionBase(name, help),
      mVal(defval),
      mStrEnumMap(strEnumPairs)
  {}

  operator E (void) const { return mVal; }

  const std::string& strVal(void) const {

    for (const auto& p: mStrEnumMap) {
      if (p.second == mVal) {
        return p.first;
      }
    }

    return helpStr(); // default fall back

  }
  
  virtual bool needsVal(void) const { return true; }

  // only called when option found
  virtual void initVal(const std::string& valStr) {
    const auto i = mStrEnumMap.find(valStr);

    if (i == mStrEnumMap.cend()) {
      std::cerr << "Bad value: " << valStr << " for option: " << mName << std::endl;
      std::abort();

    } else {
      mVal = i->second;
    }
  }
};

class Parser {

  std::vector<std::string> mNonOpts;

  std::vector<OptionBase*> mOpts;

  std::vector<char> mOptKeys;

  void printHelp() {
    std::cerr << "Usage: progname [options]" << std::endl;
    std::cerr << "Options: " << std::endl;

    for (OptionBase* o: mOpts) {
      std::cerr << "-" << o->name();
      if (o->needsVal()) {
        std::cerr << " <val>"; 
      }
      std::cerr << "   :    " << o->helpStr() << std::endl;
    }
  }

  void helpExit(const std::string& msg) {
    std::cerr << msg << std::endl;
    printHelp();
    std::abort();
  }

  OptionBase* findOption(char argName) {
    auto i = std::find(mOptKeys.begin(), mOptKeys.end(), argName);

    if (i == mOptKeys.end()) {
      helpExit(std::string("Unrecognized option: ") + argName);

      return nullptr;

    } else {
      std::ptrdiff_t index = i - mOptKeys.begin();

      assert(index >= 0);
      assert(size_t(index) < mOpts.size());
      return mOpts[size_t(index)];
    }
  }

public:

  Parser(std::initializer_list<OptionBase*> opts): mOpts(opts) {
    for (const OptionBase* o: mOpts) {
      mOptKeys.push_back(o->name());
    }
  }

  const std::vector<std::string>& nonOptVec(void) const {
    return mNonOpts;
  }

  void addOption(OptionBase* opt) noexcept {
    mOpts.emplace_back(opt);
    mOptKeys.emplace_back(opt->name());
  }

  void parse(unsigned numArgs, char* argsArr[], bool ignoreFirst=true) {

    size_t i = 0; 
    if (ignoreFirst) {
      i = 1;
    }

    for (; i < numArgs; ++i) {

      const std::string arg(argsArr[i]);

      if (arg[0] == '-') {

        if (arg.size() != 2) {
          helpExit("Unrecognized Option: " + arg);
        }

        char argName = arg[1];

        OptionBase* opt = findOption(argName);
        assert(opt);

        if (opt->needsVal()) {
          ++i;
          assert(i < numArgs);
          const std::string argVal(argsArr[i]);

          opt->initVal(argVal);

        } else {
          opt->initVal("true");
        }

      } else {
        mNonOpts.emplace_back(arg);
      }

    } // end for

  }
};

} // end namespace cmdline

} // end namespace cpputils

#endif// INCLUDE_CPPUTILS_CMD_LINE_H_
