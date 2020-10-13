// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef CPPUTILS_INCLUDE_INCLUDE_CPPUTILS_BIT_INCLUDE_CPPUTILS_H_
#define CPPUTILS_INCLUDE_INCLUDE_CPPUTILS_BIT_INCLUDE_CPPUTILS_H_

#include <cstddef>
#include <cassert>

namespace cpputils {

template <typename W>
struct BitUtil {

  constexpr static const size_t NUM_BYTES = sizeof(W);
  constexpr static const size_t NUM_BITS = 8 * NUM_BYTES;

private:
  static void checkBitIndex(size_t idx) {
    assert(idx < NUM_BITS && "invalid bit idx");
  }

  static void checkBitRange(size_t lo, size_t hi) {
    checkBitIndex(lo);
    assert(hi > 0 && hi <= NUM_BITS && "invalid hi idx for range");
  }

public:
  /**
   * masks off the upper bits [idx+1:NUM_BITS) and returns the result
   */
  static W maskUpper(const W& word, size_t idx) {
    return word & ~(~W(0) << idx);
  }

  /**
   * masks off the lower bits [0..idx+1) and returns the result
   */
  static W maskLower(const W& word, size_t idx) {
    return word & (~W(0) << idx);
  }

  static W getLSB(const W& word) {
    return word & W(1);
  }

  static W genMask(size_t lo, size_t hi) {
    W mask = ~(~W(0) << (hi - lo));
    mask <<= lo;
    return mask;
  }

  static W getBit(const W& word, size_t idx) {
    checkBitIndex(idx);
    return (word >> idx) & W(1);
  }

  // hi is non inclusive, just like begin and end iterators in V++
  static W getBitRange(const W& word, size_t lo, size_t hi) {
    checkBitRange(lo, hi);
    assert(hi > lo && "invalid bit range");
    auto x = (word >> lo); // shift the range to LSB
    return maskUpper(x, (hi - lo));
  }

  // TODO: create a template + tuple based version that does not require a runtime loop
  // bitIndices has bits ordered from lsb to msb
  template <typename V>
  static W getBits(const W& word, const V& bitIndices) {
    W res = 0;
    unsigned idx = 0;
    for (auto i: bitIndices) {
      W temp = getBit(word, i);
      res = res | (temp << idx);
      ++idx;
    }
    return res;
  }

  static W setBitRange(const W& word, size_t lo, size_t hi) {
    checkBitRange(lo, hi);
    return (word | genMask(lo, hi) );
  }

  static W setBit(const W& word, size_t idx) {
    checkBitIndex(idx);
    return word | (W(1) << idx);
  }

  template <typename V>
  static W setBits(const W& word, const V& bitIndices) {
    W ret = word;

    for (const auto& i: bitIndices) {
      ret = setBit(ret, i);
    }
    return ret;
  }

  static W clearBitRange(const W& word, size_t lo, size_t hi) {
    checkBitRange(lo, hi);
    return word & ~genMask(lo, hi);
  }

  static W clearBit(const W& word, size_t idx) {
    checkBitIndex(idx);
    W mask = ~(W(1) << idx);
    return word & mask;
  }

  template <typename V>
  static W clearBits(const W& word, const V& bitIndices) {
    W ret = word;

    for (auto i: bitIndices) {
      ret = clearBit(ret, i);
    }
    return ret;
  }

  static W removeBitRange(const W& word, size_t lo, size_t hi) {
    checkBitRange(lo, hi);
    if (lo == 0) {
      return word >> hi;
    } 

    W lower = getBitRange(word, 0u, lo);
    W upper = word >> (hi - lo); // shift right to move upper part to its new position
    upper = maskLower(upper, lo);
    return upper | lower;
  }

  static W removeBit(const W& word, size_t idx) {
    checkBitIndex(idx);
    return removeBitRange(word, idx, idx + 1);
    
  }

  /**
   * assumes that bitIndices are sorted low to high idx
   */
  template <typename V>
  static W removeBits(const W& word, const V& bitIndices) {
    W res = word;
    size_t shiftAmt = 0;
    for (auto i: bitIndices) {
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
    res >>= shiftAmt; //remove the zeroes we added to adjust for higher bitIndices 
    return res;
  }



  static W insertBitRange(const W& word, size_t lo, size_t hi) {
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

  static W insertBit(const W& word, size_t idx) {
    checkBitIndex(idx);
    return insertBitRange(word, idx, idx+1);
  }

  
  /**
   * Assumes that bitIndices are sorted. Otherwise, insertion will be wrong because
   * we shift left to make space for the new bits
   */
  template <typename V>
  static W insertBits(const W& word, const V& bitIndices) {
    W ret = word;
    for (auto i: bitIndices) {
      ret = insertBit(ret, i);
    }
    return ret;
  }


  static W replaceBitRange(const W& word, size_t lo, size_t hi, const W& newVal) {
    checkBitRange(lo, hi);
    W ret = clearBitRange(word, lo, hi);
    W nv = maskUpper(newVal, (hi - lo));
    nv <<= lo;
    return (ret | nv);
  }

  static W replaceBit(const W& word, size_t idx, const W& newVal) {
    checkBitIndex(idx);
    W ret = clearBit(word, idx);
    W nv = getLSB(newVal);
    nv <<= idx;
    return (ret | nv);
  }

  template <typename V>
  static W replaceBits(const W& word, const V& bitIndices, const W& newVal) {
    W ret = word;
    W nval = newVal;

    for (auto i: bitIndices) {
      ret = replaceBit(ret, i, getLSB(nval));
      nval >>= 1;
    }
    return ret;
  }
};

} // end namespace cpputils

#endif// CPPUTILS_INCLUDE_INCLUDE_CPPUTILS_BIT_INCLUDE_CPPUTILS_H_
