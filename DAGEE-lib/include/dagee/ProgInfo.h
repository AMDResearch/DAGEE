// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_PROG_INFO_H
#define DAGEE_INCLUDE_DAGEE_PROG_INFO_H

#include "dagee/AllocFactory.h"
#include "dagee/coreDef.h"

#include "elfio/elfio.hpp"

#include <cstdint>
#include <cstdio>
#include <link.h>

#include <algorithm>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace dagee {

namespace impl {
static constexpr const char PROC_SELF[] = "/proc/self/exe";
// static constexpr const char KERNEL_SECTION[] = ".kernel";
static constexpr const char KERNEL_SECTION[] = ".hip_fatbin";
} // namespace impl

template <typename P>
static inline ELFIO::section* find_section_if(ELFIO::elfio& reader, P p) {
  const auto it = find_if(reader.sections.begin(), reader.sections.end(), std::move(p));

  return it != reader.sections.end() ? *it : nullptr;
}

// Adopted from HIP/src/rocclr/hip_code_object.cpp
static constexpr char MAGIC_STR[] = "__CLANG_OFFLOAD_BUNDLE__";
static constexpr auto MAGIC_STR_SZ = sizeof(MAGIC_STR) - 1;
static constexpr uint64_t MAGIC_STR_ALIGNMENT = 8ul;

template <typename AllocF = dagee::StdAllocatorFactory<> >
struct KernelSectionParser {
  // Adopted from HIP/src/rocclr/hip_code_object.cpp
  // Clang Offload bundler description & Header
  struct ClangOffloadBundleDesc {
    uint64_t mOffset;
    uint64_t mSize;
    uint64_t mTripleSize;
    const char mTriple[1];
  };

  struct ClangOffloadBundleHeader {
    const char mMagicStr[MAGIC_STR_SZ];
    uint64_t mNumBundles;
    ClangOffloadBundleDesc mDesc[1];
  };

  /**
   * The bytes for each ISA have a header followed by a ISA triple string and then
   * code bytes
   */
  struct OneISAblob {
    using Str = typename AllocF::Str;

    constexpr static const char* GPU_ISA_PAT = "gfx";

    Str mTriple;
    Str mBytes;

    bool isGPU(void) const { return mTriple.find(GPU_ISA_PAT) != std::string::npos; }

    bool isEmpty() const noexcept { return mBytes.empty(); }

    const char* data(void) const { return mBytes.data(); }

    const Str& bytes() const { return mBytes; }

    const Str& triple(void) const { return mTriple; }

    size_t size(void) const { return mBytes.size(); }
  };

  using VecISAblobs = typename AllocF::template Vec<OneISAblob>;

  VecISAblobs mCodeBlobs;
  std::once_flag mOnceFlag;

  bool isValid(const ClangOffloadBundleHeader& secHead) const {
    return std::equal(MAGIC_STR, MAGIC_STR + MAGIC_STR_SZ, secHead.mMagicStr);
  }

  template <typename T>
  T* alignUp(T* ptr, std::size_t alignment) const {
    return reinterpret_cast<T*>(((reinterpret_cast<uint64_t>(ptr) + alignment - 1) / alignment) *
                                alignment);
  }

  const VecISAblobs& codeBlobs(void) const { return mCodeBlobs; }

  template <typename I>
  bool parseOneSection(const I& beg, const I& end) {
    static_assert(std::is_same<I, const char*>::value, "param type must be char*");

    // The section (.hip_fatbin) may have multiple code blobs demarcated by the
    // MAGIC_STR symbol, so we collect all code blobs between begin and end.
    // The code to loop over all code blobs and track last code blob bundle offsets
    // and sizes is inspired by rocm/bin/extractkernel
    const auto* secHead = reinterpret_cast<const ClangOffloadBundleHeader*>(beg);
    const auto* secTail = reinterpret_cast<const ClangOffloadBundleHeader*>(end);
    if (secHead == secTail) {
      return false;
    }

    bool foundValidCodeBlob = false;

    while (secHead < secTail) {
      // adjust the code blob offset in the section to the code blob alignment
      secHead = alignUp(secHead, MAGIC_STR_ALIGNMENT);
      assert(isValid(*secHead) && "invalid code blob. MAGIC_STR mismatch");

      // track the last code blob bundle entry within the code blob
      uint64_t lastBundleOffset = 0ul;
      uint64_t lastBundleSize = 0ul;

      if (isValid(*secHead)) {
        auto* desc = &secHead->mDesc[0];
        for (uint64_t i = 0; i < secHead->mNumBundles; ++i) {
          const char* blobPtr =
              reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(secHead) + desc->mOffset);

          lastBundleOffset = std::max(desc->mOffset, lastBundleOffset);
          if (lastBundleOffset == desc->mOffset) lastBundleSize = desc->mSize;
          if (desc->mSize > 0) {
            mCodeBlobs.emplace_back(OneISAblob());
            auto& blob = mCodeBlobs.back();
            blob.mBytes.assign(blobPtr, blobPtr + desc->mSize);
            blob.mTriple.assign(&desc->mTriple[0], &desc->mTriple[0] + desc->mTripleSize);

            // std::printf("triple=%s\n", blob.mTriple.c_str());
          }

          // go to the next code blob bundle entry
          desc = reinterpret_cast<const ClangOffloadBundleDesc*>(
              reinterpret_cast<uintptr_t>(&desc->mTriple[0]) + desc->mTripleSize);
        }

        // we need to find at least one valid code blob to return true
        foundValidCodeBlob = true;
      }

      // go to the next code blob
      secHead = reinterpret_cast<const ClangOffloadBundleHeader*>(
          reinterpret_cast<uintptr_t>(secHead) + lastBundleOffset + lastBundleSize);
    }

    if (foundValidCodeBlob) return true;

    assert(false && "Failed to read the kernel section in ELF");

    return false;
  }

  void parseKernelSections() {
    std::call_once(mOnceFlag, [this]() {
      dl_iterate_phdr(
          [](dl_phdr_info* info, std::size_t, void* p) {
            ELFIO::elfio reader;

            const auto elf = (info->dlpi_addr && std::strlen(info->dlpi_name) != 0)
                                 ? info->dlpi_name
                                 : impl::PROC_SELF;

            // std::cout << "Opening File: " << elf << std::endl;

            if (!reader.load(elf)) return 0;

            const auto it = find_section_if(reader, [](const ELFIO::section* x) {
              return x->get_name() == impl::KERNEL_SECTION;
            });

            if (!it) return 0;

            auto& self = *static_cast<KernelSectionParser*>(p);

            self.parseOneSection(it->get_data(), it->get_data() + it->get_size());
            return 0;
          },
          this);
    });
  }
};

namespace impl {
constexpr static const char KERNEL_STUB_FUNC_PREFIX[] = "__device_stub__";
constexpr static const size_t KERNEL_STUB_PREFIX_LENGTH = sizeof(KERNEL_STUB_FUNC_PREFIX) - 1;
} // namespace impl

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

  static Symbol read_symbol(const ELFIO::symbol_section_accessor& section, unsigned int idx) {
    assert(idx < section.get_symbols_num());

    Symbol r;
    section.get_symbol(idx, r.name, r.value, r.size, r.bind, r.type, r.sect_idx, r.other);

    return r;
  }

  static bool isStubFunction(const std::string& name) {
    return name.find(impl::KERNEL_STUB_FUNC_PREFIX) != std::string::npos;
  }

  static void printMatches(std::smatch& sm) {
    std::printf("prefix = %s\n", sm.prefix().str().c_str());
    for (size_t i = 0; i < sm.size(); ++i) {
      std::printf("group %zu, value = %s\n", i, sm.str(i).c_str());
    }
    std::printf("suffix = %s\n", sm.suffix().str().c_str());
  }

  static std::string convStubToKernelName(const std::string& name) {
    assert(isStubFunction(name));

    /**
     * HIP-Clang gneerates a stub function for each unique kernel function
     * The stub is named as <name_scope>__device_stub__<kernelName>(kernel param signature)
     * <name_scope> refers to the namespace and class etc. that encloses the kernel function
     * In the binary this stub shows up as a mangled name of the form
     * <mangled-name-scope><func name length>__device_stub__<kernelName><mangled signature>
     * So we remove the prefix __device_stub__ and adjust the function name length to
     * generate mangled name for the original kernel function
     */

    // split into 3 parts. Prefix, KERNEL_STUB_FUNC_PREFIX, suffix
    std::regex stubRegex(impl::KERNEL_STUB_FUNC_PREFIX);
    std::smatch sm;
    if (std::regex_search(name, sm, stubRegex)) {
      auto mangledPrefixWithLength =
          sm.prefix().str(); // this should be everything until KERNEL_STUB_FUNC_PREFIX. First group
                             // in regex, enclosed by ().
      auto kernNameWithoutStub = sm.suffix().str(); // stuff after KERNEL_STUB_FUNC_PREFIX

      std::smatch smNumbers;
      size_t len = 0;
      std::string mangledPrefix;
      if (std::regex_search(mangledPrefixWithLength, smNumbers, std::regex("[0-9]+$"))) {
        // printMatches(smNumbers);

        mangledPrefix = smNumbers.prefix();

        // last group has the length of kernel with stub
        len = std::stoi(smNumbers.str());
        // compute  new length
        assert(len > impl::KERNEL_STUB_PREFIX_LENGTH);
        len -= impl::KERNEL_STUB_PREFIX_LENGTH;

      } else {
        std::abort();
      }

      // std::printf("name = %s, len = %zu\n", name.c_str(), len);

      std::stringstream ss;
      ss << mangledPrefix << len << kernNameWithoutStub;
      return ss.str();
    }

    std::abort(); // couldn't match
    return "";
  }

  void function_names_for(const ELFIO::elfio& reader, ELFIO::section* symtab,
                          ELFIO::section* textSec, VecPairPtrStr& names) {
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
      if (s.sect_idx != textSec->get_index()) {
        continue;
      }
      // std::cout << "name = " << s.name << ", section_index = " << s.sect_idx << std::endl;
      if (isStubFunction(s.name)) {
        auto kernelName = convStubToKernelName(s.name);
        // std::printf("Converted %s to %s\n", s.name.c_str(), kernelName.c_str());
        names.emplace_back(s.value, kernelName);
      }
    }
  }

  void init(void) {
    std::call_once(mOnceFlag, [this]() {
      dl_iterate_phdr(
          [](dl_phdr_info* info, std::size_t, void* p) {
            ELFIO::elfio reader;
            const auto elf = (info->dlpi_addr && std::strlen(info->dlpi_name) != 0)
                                 ? info->dlpi_name
                                 : impl::PROC_SELF;

            if (!reader.load(elf)) {
              return 0;
            }

            const auto it = find_section_if(
                reader, [](const ELFIO::section* x) { return x->get_type() == SHT_SYMTAB; });

            if (!it) {
              return 0;
            }

            const auto textSec = find_section_if(
                reader, [](const ELFIO::section* x) { return x->get_name() == ".text"; });

            if (!textSec) {
              return 0;
            }

            auto& self = *static_cast<KernelPtrToNameLookup*>(p);

            VecPairPtrStr names;
            self.function_names_for(reader, it, textSec, names);

            for (auto&& x : names) {
              x.first += info->dlpi_addr;
            }

            self.mFuncPtrToName.insert(std::make_move_iterator(names.begin()),
                                       std::make_move_iterator(names.end()));

            return 0;
          },
          this);
    });
  }

 public:
  KernelPtrToNameLookup(void) { init(); }

  bool hasEntry(GenericFuncPtr funcPtr) const {
    return mFuncPtrToName.find(funcPtr) != mFuncPtrToName.cend();
  }

  const Str& name(GenericFuncPtr funcPtr) const {
    assert(hasEntry(funcPtr));

    return mFuncPtrToName.find(funcPtr)->second;
  }

  size_t size(void) const { return mFuncPtrToName.size(); }
};

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_PROG_INFO_H
