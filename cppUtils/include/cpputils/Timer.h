// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_TIMER_H_
#define INCLUDE_CPPUTILS_TIMER_H_

#include "cpputils/Print.h"

#include <chrono>
#include <iostream>

#include <cassert>
#include <cstdint>

#include <time.h>

namespace cpputils {

template <typename __UNUSED=void>
class CppTimer {

  using Clock = std::chrono::high_resolution_clock;
  using TimeTy = decltype(Clock::now());
  
  bool mOn;
  TimeTy mStart;
  TimeTy mStop;
  const char* const mTopic;
  const char* const mRegion;

  template <typename T>
  typename T::rep elapsed(void) const {
    return std::chrono::duration_cast<T>(mStop - mStart).count();
  }

public:

  explicit CppTimer(const char* const topicName, const char* const regionName, bool startNow=false) 
    :
      mOn(false),
      mStart(),
      mStop(),
      mTopic(topicName),
      mRegion(regionName)
  {
    assert(mTopic && "Subject name cannot be nullptr");
    assert(mRegion && "Region name cannot be nullptr");
    if (startNow) {
      start();
    }
  }

  ~CppTimer(void) {
    if (mOn) { 
      stop();
    }
    print();
  }

  void start(void) {
    assert(!mOn && "Timer already running");
    mOn = true;
    mStart = Clock::now();
  }

  void stop(void) {
    assert(mOn && "Timer hasn't started");
    mOn = false;
    mStop = Clock::now();
  }

  std::chrono::milliseconds::rep elapsed_ms(void) const {
    return elapsed<std::chrono::milliseconds>();
  }

  std::chrono::microseconds::rep elapsed_us(void) const {
    return elapsed<std::chrono::microseconds>();
  }

  std::chrono::nanoseconds::rep elapsed_ns(void) const {
    return elapsed<std::chrono::nanoseconds>();
  }

  std::chrono::seconds::rep elapsed_s(void) const {
    return elapsed<std::chrono::seconds>();
  }

  void print(void) const {
    cpputils::printStat(mTopic, std::string(mRegion) + " Time (ms)", elapsed_ms());
  }
};

template <typename __UNUSED=void>
class CTimer {

  constexpr static const clockid_t CLOCKTYPE = CLOCK_MONOTONIC_RAW;
  constexpr static const uint64_t BILL = 1000000000;
  constexpr static const uint64_t MILL = 1000000;
  constexpr static const uint64_t KILO = 1000;


  struct timespec mStart; 
  struct timespec mStop;
  bool mOn;
  const char* const mTopic;
  const char* const mRegion;

 public:
  explicit CTimer(const char* const topicName, const char* const regionName, bool startNow=false) 
    :
      mOn(false),
      mStart(),
      mStop(),
      mTopic(topicName),
      mRegion(regionName)
  {
    assert(mTopic && "Subject name cannot be nullptr");
    assert(mRegion && "Region name cannot be nullptr");
    if (startNow) {
      start();
    }
  }

  ~CTimer(void) {
    if (mOn) { 
      stop();
    }
    print();
  }

  void print(void) const {
    cpputils::printStat(mTopic, std::string(mRegion) + " Time (ms)", elapsed_ms());
  }

  void start(void) {
    assert(!mOn && "Timer already running");
    mOn = true;
    clock_gettime(CLOCKTYPE, &mStart);
  }

  void stop(void) {
    assert(mOn && "Timer hasn't started");
    mOn = false;
    clock_gettime(CLOCKTYPE, &mStop);
  }

  std::uint64_t elapsed_ns() const {
    std::int64_t sec = mStop.tv_sec - mStart.tv_sec;
    assert(sec >= 0 && "seconds went backwards");

    std::int64_t nsec = mStop.tv_nsec - mStart.tv_nsec;

    nsec = sec * BILL + nsec;
    assert (nsec >= 0 && "time went backwards");

    return nsec;
  }

  std::uint64_t elapsed_us() const {
    return elapsed_ns() / KILO;
  }

  std::uint64_t elapsed_ms() const {
    return elapsed_ns() / MILL;
  }

  std::uint64_t duration_s() const {
    return elapsed_ns() / BILL;
  }
};


using Timer = CppTimer<>;

template <typename O>
O& operator << (O& out, const Timer& t) {
  t.print(out);
  return out;
}

template <typename F>
void timeThis(const F& func, const char* const topicName, const char* const regionName) {

  Timer t(topicName, regionName, true);
  func();
}

} // end namespace cpputils

#endif// INCLUDE_CPPUTILS_TIMER_H_
