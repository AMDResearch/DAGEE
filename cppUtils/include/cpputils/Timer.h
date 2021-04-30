// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_TIMER_H_
#define INCLUDE_CPPUTILS_TIMER_H_

#include "cpputils/Print.h"

#include <time.h>

#include <cassert>
#include <cstdint>

#include <chrono>
#include <iostream>
#include <string>

namespace cpputils {

enum class TimeUnit {
  S=0,
  MS,
  US,
  NS
};

template <TimeUnit TIME_UNIT=TimeUnit::US>
class CppTimer {

protected:
  using ClockTy = std::chrono::high_resolution_clock;
  using TimeTy = decltype(ClockTy::now());
  using DurationTy = ClockTy::duration;

  struct DurationWrap {
    DurationTy mDuration;

    DurationWrap(const DurationTy& d) noexcept:
      mDuration {d}
    {}

    template <typename T>
    typename T::rep elapsed(void) const noexcept{
      return std::chrono::duration_cast<T>(mDuration).count();
    }

    std::chrono::milliseconds::rep as_ms(void) const noexcept {
      return elapsed<std::chrono::milliseconds>();
    }

    std::chrono::microseconds::rep as_us(void) const noexcept {
      return elapsed<std::chrono::microseconds>();
    }

    std::chrono::nanoseconds::rep as_ns(void) const noexcept {
      return elapsed<std::chrono::nanoseconds>();
    }

    std::chrono::seconds::rep as_s(void) const noexcept { 
      return elapsed<std::chrono::seconds>(); 
    }
  };

  bool mOn;
  TimeTy mStart;
  DurationTy mDuration;
  const std::string mTopic;
  const std::string mRegion;


  template <typename T>
  void printImpl(const char* const unitStr, const T& val) const noexcept {
    cpputils::printStat(mTopic, mRegion + unitStr, val);
  }

 public:
  using Print_s = CppTimer<TimeUnit::S>;
  using Print_ms = CppTimer<TimeUnit::MS>;
  using Print_us = CppTimer<TimeUnit::US>;
  using Print_ns = CppTimer<TimeUnit::NS>;

  using Interval = DurationWrap;

  template <typename S1, typename S2>
  CppTimer(const S1& topicName, const S2& regionName,
                    bool startNow = false) noexcept: 
      mOn(false), 
      mStart(), 
      mDuration(), 
      mTopic(topicName), 
      mRegion(regionName) 
  {
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
    mStart = ClockTy::now();
  }

  Interval stop(void) {
    assert(mOn && "Timer hasn't started");
    mOn = false;
    auto t = mStart;
    mStart = ClockTy::now();
    DurationTy currInterval = mStart - t;
    mDuration += currInterval;
    return currInterval;
  }

  std::chrono::seconds::rep elapsed_s(void) const { 
    return Interval(mDuration).as_s();
  }

  std::chrono::milliseconds::rep elapsed_ms(void) const {
    return Interval(mDuration).as_ms();
  }

  std::chrono::microseconds::rep elapsed_us(void) const {
    return Interval(mDuration).as_us();
  }

  std::chrono::nanoseconds::rep elapsed_ns(void) const {
    return Interval(mDuration).as_ns();
  }

  void print(void) const {
    switch (TIME_UNIT) {
      case TimeUnit::S:
        printImpl(" Time (sec)", elapsed_s());
        break;
      case TimeUnit::MS:
        printImpl(" Time (ms)", elapsed_ms());
        break;
      case TimeUnit::US:
        printImpl(" Time (us)", elapsed_us());
        break;
      case TimeUnit::NS:
        printImpl(" Time (ns)", elapsed_ns());
        break;
      default:
        std::abort();
    }
  }
};

#if 0
template <typename __UNUSED = void>
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
  explicit CTimer(const char* const topicName, const char* const regionName, bool startNow = false)
      : mOn(false), mStart(), mStop(), mTopic(topicName), mRegion(regionName) {
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
    assert(nsec >= 0 && "time went backwards");

    return nsec;
  }

  std::uint64_t elapsed_us() const { return elapsed_ns() / KILO; }

  std::uint64_t elapsed_ms() const { return elapsed_ns() / MILL; }

  std::uint64_t duration_s() const { return elapsed_ns() / BILL; }
};
#endif//#if 0

using Timer = CppTimer<>;

template <typename O>
O& operator<<(O& out, const Timer& t) {
  t.print(out);
  return out;
}

template <typename F>
void timeThis(const F& func, const char* const topicName, const char* const regionName) {
  Timer t(topicName, regionName, true);
  func();
}

} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_TIMER_H_
