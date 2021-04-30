// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__BINARY_H_
#define DAGEE_INCLUDE_DAGR__BINARY_H_

#include "dagr/hsaCore.h"

#include "dagee/ProgInfo.h"

#include <fstream>
#include <vector>

namespace dagr {

struct BinarFileReader {

  std::string mBytes;

  template <typename Str>
  void init(const Str& fileName) noexcept {
    std::ifstream fh(fileName, std::ios::in | std::ios::binary);
    assert(fh.is_open() && fh.good());

    fh.seekg(0, fh.end);
    auto fsize = fh.tellg();
    fh.seekg(0, fh.beg);

    mBytes.resize(fsize, 0);
    fh.read(&mBytes[0], fsize);
    fh.close();
  }

  template <typename Str>
  BinarFileReader(const Str& fileName) noexcept {
    init(fileName);
  }

  const std::string& bytes() const noexcept {
    return mBytes;
  }

  size_t size() const noexcept {
    return mBytes.size();
  }
};

struct ExecutableState {

  using VecExecutable = std::vector<Executable>;

  Agent mAgent;
  VecExecutable mExecutables;

  template <typename Str>
  void loadMemory(const Str& bytes) noexcept {

    Executable exe;
    ASSERT_HSA(hsa_executable_create_alt(HSA_PROFILE_FULL, HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR, nullptr, &exe));

    CodeObjectReader codeObjReader;
    ASSERT_HSA(hsa_code_object_reader_create_from_memory(bytes.data(), bytes.size(), &codeObjReader));


    // TODO (amber): FIXME: check that ISA matches the agent before proceeding.
    // Needed for a Multi GPU system

    ASSERT_HSA(hsa_executable_load_agent_code_object(exe, mAgent, codeObjReader, nullptr, nullptr));

    ASSERT_HSA(hsa_code_object_reader_destroy(codeObjReader));

    ASSERT_HSA(hsa_executable_freeze(exe, nullptr));

    mExecutables.emplace_back(exe);
  }

  template <typename Str>
  void loadBinaryFile(const Str& fileName) noexcept {

    BinarFileReader fh(fileName);
    loadMemory(fh.bytes());
    
  }

  void init() noexcept {
    dagee::KernelSectionParser<> kparser;
    kparser.parseKernelSections();

    for (const auto& blob: kparser.codeBlobs()) {
      if (!blob.isEmpty() && blob.isGPU()) {
        loadMemory(blob.bytes());
      }
    }
  }

  explicit ExecutableState(const Agent& agent) noexcept :
    mAgent(agent),
    mExecutables() 
  {
    init();
  }

  ~ExecutableState() noexcept {
    for (const auto& exe: mExecutables) {
      hsa_executable_destroy(exe);
    }
    mExecutables.clear();
  }

  KernelObject kernelObjtectByName(const char* const name) const noexcept {

    KernelObject ret;
    assert(ret.mVal == 0);

    hsa_executable_symbol_t kernSym;
    bool found = false;

    for (const auto& exe: mExecutables) {

      if (hsa_executable_get_symbol_by_name(exe, name, &mAgent, &kernSym) == HSA_STATUS_SUCCESS) {
        found = true;
        break;
      }
    }

    if (!found) {
      std::abort();
      return ret;
    }

    ASSERT_HSA(hsa_executable_symbol_get_info(kernSym, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &ret.mVal));
    return ret;
  }
  
};



}// end namespace dagr


#endif// DAGEE_INCLUDE_DAGR__BINARY_H_
