// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef CPPUTILS_INCLUDE_INCLUDE_CPPUTILS_BIT_INCLUDE_CPPUTILS_H_
#define CPPUTILS_INCLUDE_INCLUDE_CPPUTILS_BIT_INCLUDE_CPPUTILS_H_

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace cpputils {

template <typename W>
struct BitUtil {
  constexpr static const size_t NUM_BYTES = sizeof(W);
  constexpr static const size_t NUM_BITS = 8 * NUM_BYTES;

 private:
  static void checkBitIndex(size_t idx) noexcept { assert(idx < NUM_BITS && "invalid bit idx"); }

  static void checkBitRange(size_t lo, size_t hi) noexcept {
    checkBitIndex(lo);
    assert(hi > 0 && hi <= NUM_BITS && "invalid hi idx for range");
  }

 public:
  /**
   * masks off the upper bits [idx+1:NUM_BITS) and returns the result
   */
  static W maskUpper(const W& word, size_t idx) noexcept { return word & ~(~W(0) << idx); }

  /**
   * masks off the lower bits [0..idx+1) and returns the result
   */
  static W maskLower(const W& word, size_t idx) noexcept { return word & (~W(0) << idx); }

  static W getLSB(const W& word) noexcept { return word & W(1); }

  static W genMask(size_t lo, size_t hi) noexcept {
    W mask = ~(~W(0) << (hi - lo));
    mask <<= lo;
    return mask;
  }

  static W getBit(const W& word, size_t idx) noexcept {
    checkBitIndex(idx);
    return (word >> idx) & W(1);
  }

  // hi is non inclusive, just like begin and end iterators in V++
  static W getBitRange(const W& word, size_t lo, size_t hi) noexcept {
    checkBitRange(lo, hi);
    assert(hi > lo && "invalid bit range");
    auto x = (word >> lo); // shift the range to LSB
    return maskUpper(x, (hi - lo));
  }

  // TODO: create a template + tuple based version that does not require a runtime loop
  // bitIndices has bits ordered from lsb to msb
  template <typename V>
  static W getBits(const W& word, const V& bitIndices) noexcept {
    W res = 0;
    unsigned idx = 0;
    for (auto i : bitIndices) {
      W temp = getBit(word, i);
      res = res | (temp << idx);
      ++idx;
    }
    return res;
  }

  static W getBitsByMask(const W& word, const W& mask) noexcept {
    W res = 0;
    unsigned idx = 0;
    for (auto i = 0; i < NUM_BITS; ++i) {
      if (getBit(mask, i)) {
        W temp = getBit(word, i);
        res = res | (temp << idx);
        ++idx;
      }
    }
    return res;
  }

  static W setBitRange(const W& word, size_t lo, size_t hi) noexcept {
    checkBitRange(lo, hi);
    return (word | genMask(lo, hi));
  }

  static W setBit(const W& word, size_t idx) noexcept {
    checkBitIndex(idx);
    return word | (W(1) << idx);
  }

  template <typename V>
  static W setBits(const W& word, const V& bitIndices) noexcept {
    W ret = word;

    for (const auto& i : bitIndices) {
      ret = setBit(ret, i);
    }
    return ret;
  }

  static W setBitsByMask(const W& word, const W& mask) noexcept {
    W ret = word;

    for (auto i = 0; i < NUM_BITS; ++i) {
      if (getBit(mask, i)) {
        ret = setBit(ret, i);
      }
    }
    return ret;
  }

  static W clearBitRange(const W& word, size_t lo, size_t hi) noexcept {
    checkBitRange(lo, hi);
    return word & ~genMask(lo, hi);
  }

  static W clearBit(const W& word, size_t idx) noexcept {
    checkBitIndex(idx);
    W mask = ~(W(1) << idx);
    return word & mask;
  }

  template <typename V>
  static W clearBits(const W& word, const V& bitIndices) noexcept {
    W ret = word;

    for (auto i : bitIndices) {
      ret = clearBit(ret, i);
    }
    return ret;
  }

  static W removeBitRange(const W& word, size_t lo, size_t hi) noexcept {
    checkBitRange(lo, hi);
    if (lo == 0) {
      return word >> hi;
    }

    W lower = getBitRange(word, 0u, lo);
    W upper = word >> (hi - lo); // shift right to move upper part to its new position
    upper = maskLower(upper, lo);
    return upper | lower;
  }

  static W removeBit(const W& word, size_t idx) noexcept {
    checkBitIndex(idx);
    return removeBitRange(word, idx, idx + 1);
  }

  // Instead of consuming a set of bits that should be removed, this function consumes a
  // set of bits that should be saved. Each element of innerBitIndices represents a bitmask/range of
  // bits that needs to be saved (and not removed) in word. Also each element of innerBitIndices
  // represents an inner range of bits between two consecutive bits that should be removed.
  // If we OR all elements of innerBitIndices, we get a single bitmask where 1 means save the bit
  // and 0 means remove the bit at that location. Size of innerBitIndices is number of bits to
  // be removed + 1.
  // For example, if bits 3 and 5 have to be removed from an 8-bit word, a regular bitmask to
  // represent it would look like 00101000. However, elements of innerBitIndices would be
  // 00000111, 00010000, 11000000. If we OR these elements we would get 11010111 = ~00101000.
  template <typename V>
  static W removeBitsByInnerBits(const W& word, const V& innerBitIndices) noexcept {
    W ret = W(0);
    size_t idx = 0;
    for (auto i : innerBitIndices) {
      // Captures removal of bit ranges. `i` will be non-zero if bits to be removed are not
      // adjacent. If `i` is 0, that means this bit is adjacent to another bit that was removed
      // in the prior iteration, so there are no bits to be saved between this bit and
      // the previous one. so do nothing but increment the shift counter.
      if (i) {
        // shift right depending on index of bit to be removed (idx).
        ret |= ((word & i) >> idx);
      }
      idx++;
    }
    return ret;
  }

  static W removeBitsByMask(const W& word, const W& mask) noexcept {
    W res = word;
    size_t shiftAmt = 0;
    for (auto i = 0; i < NUM_BITS; ++i) {
      if (getBit(mask, i)) {
        res = removeBit(res, i);
        /*
         * since bitIndices are sorted, removing a lower order bit changes the
         * positions of higher order bits, so higher bitIndices are off by 1
         * So, we left shift by 1 to make higher bitIndices valid, but ++shiftAmt to
         * remember how many zeros we added on the right and remove them
         */
        res <<= 1;
        ++shiftAmt;
      }
    }
    res >>= shiftAmt; // remove the zeroes we added to adjust for higher bitIndices
    return res;
  }

  /**
   * assumes that bitIndices are sorted low to high idx
   */
  template <typename V>
  static W removeBits(const W& word, const V& bitIndices) noexcept {
    W res = word;
    size_t shiftAmt = 0;
    for (auto i : bitIndices) {
      res = removeBit(res, i);
      /*
       * since bitIndices are sorted, removing a lower order bit changes the
       * positions of higher order bits, so higher bitIndices are off by 1
       * So, we left shift by 1 to make higher bitIndices valid, but ++shiftAmt to
       * remember how many zeros we added on the right and remove them
       */
      res <<= 1;
      ++shiftAmt;
    }
    res >>= shiftAmt; // remove the zeroes we added to adjust for higher bitIndices
    return res;
  }

  static W insertBitRange(const W& word, size_t lo, size_t hi) noexcept {
    checkBitRange(lo, hi);
    /*
     * algorithm: get the subsets [0:lo+1) and [lo:MSB)
     * shift upper by (hi-lo) i.e. the bit width of the range, to make space for bit range
     * shift left insertVal by lo to bring it to align with the insert space
     * for  safety maskOff [MSB, hi)
     * do upper | insertVal | lower
     */
    W lower = maskUpper(word, lo);
    W upper = maskLower(word, lo);
    upper <<= (hi - lo); // make space for new bits

    return (upper | lower);
  }

  static W insertBit(const W& word, size_t idx) noexcept {
    checkBitIndex(idx);
    return insertBitRange(word, idx, idx + 1);
  }

  static W insertBitsByMask(const W& word, const W& mask) noexcept {
    W ret = word;
    for (auto i = 0; i < NUM_BITS; ++i) {
      if (getBit(mask, i)) {
        ret = insertBit(ret, i);
      }
    }
    return ret;
  }

  // This does the opposite of removeBitsByInnerBits. It uses each element of innerBitIndices to
  // capture the right set of bits from word and shift them appropriately to insert bits at
  // the right locations.
  template <typename V>
  static W insertBitsByInnerBits(const W& word, const V& innerBitIndices) noexcept {
    W ret = W(0);
    size_t idx = 0;
    for (auto i : innerBitIndices) {
      if (i) {
        ret |= ((word & (i >> idx)) << idx);
      }
      idx++;
    }
    return ret;
  }

  /**
   * Assumes that bitIndices are sorted. Otherwise, insertion will be wrong
   * because we shift left to make space for the new bits
   */
  template <typename V>
  static W insertBits(const W& word, const V& bitIndices) noexcept {
    W ret = word;
    for (auto i : bitIndices) {
      ret = insertBit(ret, i);
    }
    return ret;
  }

  static W replaceBitRange(const W& word, size_t lo, size_t hi, const W& newVal) noexcept {
    checkBitRange(lo, hi);
    W ret = clearBitRange(word, lo, hi);
    W nv = maskUpper(newVal, (hi - lo));
    nv <<= lo;
    return (ret | nv);
  }

  static W replaceBit(const W& word, size_t idx, const W& newVal) noexcept {
    checkBitIndex(idx);
    W ret = clearBit(word, idx);
    W nv = getLSB(newVal);
    nv <<= idx;
    return (ret | nv);
  }

  template <typename V>
  static W replaceBits(const W& word, const V& bitIndices, const W& newVal) noexcept {
    W ret = word;
    W nval = newVal;

    for (auto i : bitIndices) {
      ret = replaceBit(ret, i, getLSB(nval));
      nval >>= 1;
    }
    return ret;
  }

  static W replaceBitsByMask(const W& word, const W& mask, const W& newVal) noexcept {
    W ret = word;
    W nval = newVal;

    for (auto i = 0; i < NUM_BITS; ++i) {
      if (getBit(mask, i)) {
        ret = replaceBit(ret, i, getLSB(nval));
        nval >>= 1;
      }
    }
    return ret;
  }
};

template <typename T>
class BitUtil<T*> {

  using Int = uint64_t;
  using IntBitUtil = BitUtil<Int>;
  using P = T*;

  static Int toInt(P ptr) noexcept {
    return reinterpret_cast<Int>(ptr);
  }
  
  static P toPtr(Int i) noexcept {
    return reinterpret_cast<P>(i);
  }

public:

  using pointer = P;

  constexpr static const auto NUM_BYTES = IntBitUtil::NUM_BYTES;
  constexpr static const auto NUM_BITS = IntBitUtil::NUM_BITS;

  static P maskUpper(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::maskUpper(toInt(ptr), idx));
  }

  static P maskLower(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::maskLower(toInt(ptr), idx));
  }

  static P getLSB(const P& ptr) noexcept {
    return toPtr(IntBitUtil::getLSB(toInt(ptr)));
  }

  static P genMask(size_t lo, size_t hi) noexcept {
    return toPtr(IntBitUtil::genMask(lo, hi));
  }

  static P getBit(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::getBit(toInt(ptr), idx));
  }

  static P getBitRange(const P& ptr, size_t lo, size_t hi) noexcept {
    return toPtr(IntBitUtil::getBitRange(toInt(ptr), lo, hi));
  }

  template <typename V>
  static P getBits(const P& ptr, const V& bitIndices) noexcept {
    return toPtr(IntBitUtil::getBits(toInt(ptr), bitIndices));
  }

  static P getBitsByMask(const P& ptr, Int mask) noexcept {
    return toPtr(IntBitUtil::getBitsByMask(toInt(ptr), mask));
  }

  static P setBit(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::setBit(toInt(ptr), idx));
  }

  static P setBitRange(const P& ptr, size_t lo, size_t hi) noexcept {
    return toPtr(IntBitUtil::setBitRange(toInt(ptr), lo, hi));
  }

  template <typename V>
  static P setBits(const P& ptr, const V& bitIndices) noexcept {
    return toPtr(IntBitUtil::setBits(toInt(ptr), bitIndices));
  }

  static P setBitsByMask(const P& ptr, Int mask) noexcept {
    return toPtr(IntBitUtil::setBits(toInt(ptr), mask));
  }

  static P clearBit(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::clearBit(toInt(ptr), idx));
  }

  static P clearBitRange(const P& ptr, size_t lo, size_t hi) noexcept {
    return toPtr(IntBitUtil::clearBitRange(toInt(ptr), lo, hi));
  }

  template <typename V>
  static P clearBits(const P& ptr, const V& bitIndices) noexcept {
    return toPtr(IntBitUtil::clearBits(toInt(ptr), bitIndices));
  }
  
  static P replaceBit(const P& ptr, size_t idx, const P& newVal) noexcept {
    return toPtr(IntBitUtil::replaceBit(toInt(ptr), idx, toInt(newVal)));
  }

  static P replaceBitRange(const P& ptr, size_t lo, size_t hi, const P& newVal) noexcept {
    return toPtr(IntBitUtil::replaceBitRange(toInt(ptr), lo, hi, toInt(newVal)));
  }

  template <typename V>
  static P replaceBits(const P& ptr, const V& bitIndices, const P& newVal) noexcept {
    return toPtr(IntBitUtil::replaceBits(toInt(ptr), bitIndices, toInt(newVal)));
  }

  static P replaceBitsByMask(const P& ptr, Int mask, const P& newVal) noexcept {
    return toPtr(IntBitUtil::replaceBitsByMask(toInt(ptr), mask, toInt(newVal)));
  }

  static P removeBit(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::removeBit(toInt(ptr), idx));
  }

  static P removeBitRange(const P& ptr, size_t lo, size_t hi) noexcept {
    return toPtr(IntBitUtil::removeBitRange(toInt(ptr), lo, hi));
  }

  template <typename V>
  static P removeBits(const P& ptr, const V& bitIndices) noexcept {
    return toPtr(IntBitUtil::removeBits(toInt(ptr), bitIndices));
  }
  
  static P removeBitsByMask(const P& ptr, Int mask) noexcept {
    return toPtr(IntBitUtil::removeBitsByMask(toInt(ptr), mask));
  }

  template <typename V>
  static P removeBitsByInnerBits(const P& ptr, const V& innerBitIndices) noexcept {
    return toPtr(IntBitUtil::removeBitsByInnerBits(toInt(ptr), innerBitIndices));
  }

  static P insertBit(const P& ptr, size_t idx) noexcept {
    return toPtr(IntBitUtil::insertBit(toInt(ptr), idx));
  }

  static P insertBitRange(const P& ptr, size_t lo, size_t hi) noexcept {
    return toPtr(IntBitUtil::insertBitRange(toInt(ptr), lo, hi));
  }

  template <typename V>
  static P insertBits(const P& ptr, const V& bitIndices) noexcept {
    return toPtr(IntBitUtil::insertBits(toInt(ptr), bitIndices));
  }

  static P insertBitsByMask(const P& ptr, Int mask) noexcept {
    return toPtr(IntBitUtil::insertBitsByMask(toInt(ptr), mask));
  }

  template <typename V>
  static P insertBitsByInnerBits(const P& ptr, const V& innerBitIndices) noexcept {
    return toPtr(IntBitUtil::insertBitsByInnerBits(toInt(ptr), innerBitIndices));
  }
};

} // end namespace cpputils

#endif // CPPUTILS_INCLUDE_INCLUDE_CPPUTILS_BIT_INCLUDE_CPPUTILS_H_
