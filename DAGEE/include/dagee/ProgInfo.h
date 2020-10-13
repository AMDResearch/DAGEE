// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_PROG_INFO_H
#define DAGEE_INCLUDE_DAGEE_PROG_INFO_H

#include "dagee/coreDef.h"
#include "dagee/AllocFactory.h"

#include "elfio/elfio.hpp"


#include <cstdint>
#include <cstdio>
#include <link.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace dagee {

static constexpr const char PROC_SELF[] = "/proc/self/exe";
static constexpr const char KERNEL_SECTION[] = ".kernel";

template <typename P>
static inline ELFIO::section* find_section_if(ELFIO::elfio& reader, P p) {
  const auto it =
    find_if(reader.sections.begin(), reader.sections.end(), std::move(p));

  return it != reader.sections.end() ? *it : nullptr;
}


// Adopted from HIP/include/hip/hcc_detail/code_object_bundle.hpp
static constexpr char MAGIC_STR[] = "__CLANG_OFFLOAD_BUNDLE__";
static constexpr auto MAGIC_STR_SZ = sizeof(MAGIC_STR) - 1;

template <typename AllocF = dagee::StdAllocatorFactory<> >
struct KernelSectionParser {

  /**
   * Each kernel section has a section header that contains the MAGIC_STR and number
   * of code blobs per each ISA
   */
  union SectionHeader {
    struct {
      char mMagicStr[MAGIC_STR_SZ];
      std::uint64_t mNumKernels;
    };
    char mCharBuf[sizeof(mMagicStr) + sizeof(mNumKernels)];
  };

  /**
   * The bytes for each ISA have a header followed by a ISA triple string and then
   * code bytes
   */
  struct OneISAblob {

    using Str = typename AllocF::Str;

    constexpr static const char* GPU_ISA_PAT = "gfx";

    union Header {
      struct {
        std::uint64_t mOffset; // the starting point wrt beginning of the kernel section
        std::uint64_t mBlobSz; // size of the bytes
        std::uint64_t mTripleSz; // size of the triple ISA string
      };
      char mCharBuf[sizeof(mOffset) + sizeof(mBlobSz) + sizeof(mTripleSz)];
    } mHeader;

    Str mTriple;
    Str mBytes;

    bool mValid = false;

    bool isValid(void) const { return mValid; }

    bool isEmpty(void) const { return mHeader.mBlobSz == 0; }

    bool isGPU(void) const { 
      assert(isValid());
      return mTriple.find(GPU_ISA_PAT) != std::string::npos;
    }

    const char* data(void) const { return mBytes.data(); }

    const Str& triple(void) const { return mTriple; }

    size_t size(void) const { 
      assert(mBytes.size() == mHeader.mBlobSz);
      return mBytes.size();
    }

  };


  using VecISAblobs = typename AllocF::template Vec<OneISAblob>;

  VecISAblobs mCodeBlobs;
  std::once_flag mOnceFlag;

  bool isValid(const SectionHeader& secHead) const {
    return std::equal(MAGIC_STR, MAGIC_STR + MAGIC_STR_SZ, secHead.mMagicStr);
  }

  const VecISAblobs& codeBlobs(void) const { return mCodeBlobs; }

  template <typename I>
  bool parseOneSection(const I& beg, const I& end) {

    if (beg == end) { 
      return false;
    }

    SectionHeader secHead;

    std::copy_n(beg, sizeof(secHead.mCharBuf), secHead.mCharBuf);

    if(isValid(secHead)) {

      // std::cout << "Found mNumKernels = " << secHead.mNumKernels << std::endl;

      auto cur = beg + sizeof(secHead.mCharBuf);

      for (size_t i = 0; i < secHead.mNumKernels; ++i) {

        mCodeBlobs.emplace_back(OneISAblob());
        auto& blob = mCodeBlobs.back();
        auto& h = blob.mHeader;

        std::copy_n(cur, sizeof(h.mCharBuf), h.mCharBuf);
        std::advance(cur, sizeof(h.mCharBuf));

        blob.mTriple.assign(cur, cur + h.mTripleSz);
        std::advance(cur, h.mTripleSz);

        auto b = beg + h.mOffset;
        auto e = b + h.mBlobSz;
        blob.mBytes.assign(b, e);
        blob.mValid = true;

        // std::cout << "mOffset: " << h.mOffset << ", mBlobSz: " << h.mBlobSz 
          // << ", mTripleSz: " << h.mTripleSz 
          // << ", mTriple: " << blob.mTriple << std::endl; 


      }

      return true;
    }

    assert(false && "Failed to read the kernel section in ELF");

    return false;
  }

  void parseKernelSections() {
    std::call_once(mOnceFlag, [this]() {
      dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* p) {
          ELFIO::elfio reader;

          const auto elf = (info->dlpi_addr && std::strlen(info->dlpi_name) != 0) ?
          info->dlpi_name : PROC_SELF;

          // std::cout << "Opening File: " << elf << std::endl;

          if (!reader.load(elf)) return 0;

          const auto it = find_section_if(reader, [](const ELFIO::section* x) {
              return x->get_name() == KERNEL_SECTION;
              });

          if (!it) return 0;

          auto& self = *static_cast<KernelSectionParser*>(p);

          self.parseOneSection(it->get_data(), it->get_data() + it->get_size());
          return 0;

        }, this);
    });

  }

};

template <typename AllocF = dagee::StdAllocatorFactory<> >
class KernelPtrToNameLookup {

  using Str = typename AllocF::Str;
  using MapPtrStr = typename AllocF::template HashMap<GenericFuncPtr, Str>;
  using VecPairPtrStr = typename AllocF::template Vec<std::pair<GenericFuncPtr, Str> >;

  std::once_flag mOnceFlag;
  MapPtrStr mFuncPtrToName;

  struct Symbol {
      Str name;
      ELFIO::Elf64_Addr value = 0;
      ELFIO::Elf_Xword size = 0;
      ELFIO::Elf_Half sect_idx = 0;
      std::uint8_t bind = 0;
      std::uint8_t type = 0;
      std::uint8_t other = 0;
  };

  static Symbol read_symbol(const ELFIO::symbol_section_accessor& section,
      unsigned int idx) {
    assert(idx < section.get_symbols_num());

    Symbol r;
    section.get_symbol(
        idx, r.name, r.value, r.size, r.bind, r.type, r.sect_idx, r.other);


    return r;
  }
  
  void function_names_for(const ELFIO::elfio& reader, ELFIO::section* symtab, ELFIO::section* textSec, VecPairPtrStr& names) {

    ELFIO::symbol_section_accessor symbols{reader, symtab};

    // std::cout << "index of text section: " << textSec->get_index() << std::endl;

    for (auto i = 0u; i != symbols.get_symbols_num(); ++i) {
      // TODO: this is boyscout code, caching the temporaries
      //       may be of worth.
      auto s = read_symbol(symbols, i);

      if (s.type != STT_FUNC) continue;
      if (s.type == SHN_UNDEF) continue;
      if (s.name.empty()) continue;


      // Added check to filter out non text symbols
      if (s.sect_idx != textSec->get_index()) { continue; }
      // std::cout << "name = " << s.name << ", section_index = " << s.sect_idx << std::endl;


      names.emplace_back(s.value, s.name);
    }
  }

  void init(void) {

    std::call_once(mOnceFlag, [this]() {

        dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* p) {

            ELFIO::elfio reader;
            const auto elf = (info->dlpi_addr && std::strlen(info->dlpi_name) != 0) ?
                info->dlpi_name : PROC_SELF;

            if (!reader.load(elf)) { return 0; }

            const auto it = find_section_if(reader, [](const ELFIO::section* x) {
                return x->get_type() == SHT_SYMTAB;
            });

            if (!it) { return 0; }

            const auto textSec = find_section_if(reader, [] (const ELFIO::section* x) {
                  return x->get_name() == ".text";
                });

            if (!textSec) { return 0; }

            auto& self = *static_cast<KernelPtrToNameLookup*>(p);

            VecPairPtrStr names;
            self.function_names_for(reader, it, textSec, names);

            for (auto&& x : names) { 
              x.first += info->dlpi_addr;
            }

            self.mFuncPtrToName.insert(
                std::make_move_iterator(names.begin()),
                std::make_move_iterator(names.end()));

            return 0;
        }, this);
    });
  }

public:

  KernelPtrToNameLookup(void) {
    init();
  }

  bool hasEntry(GenericFuncPtr funcPtr) const {
    return mFuncPtrToName.find(funcPtr) != mFuncPtrToName.cend();
  }

  const Str& name(GenericFuncPtr funcPtr) const {
    assert(hasEntry(funcPtr));

    return mFuncPtrToName.find(funcPtr)->second;
  }

  size_t size(void) const {
    return mFuncPtrToName.size();
  }

};

}// end namespace dagee

#endif// DAGEE_INCLUDE_DAGEE_PROG_INFO_H
