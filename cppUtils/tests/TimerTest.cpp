// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.
#include <cstdlib>
#include <cstdio>

#include <unistd.h>

#include "util/Timer.h"

void doSleep(int sec) {
  sleep(sec);
}

void doUsleep(int microSec) {
  usleep(microSec);
}


int main(int argc, char* argv[]) {

  if (argc != 2) { 
    std::abort();
  }

  int inputDuration = std::atoi(argv[1]);

  constexpr const bool USE_MICRO_SECONDS = true;

  if (USE_MICRO_SECONDS) { 

    util::CppTimer<> cppTimer("CPP", "usleep", true);
    doUsleep(inputDuration);
    cppTimer.stop();

    util::CTimer<> cTimer("C", "usleep", true);
    doUsleep(inputDuration);
    cTimer.stop();

  } else {

    util::CppTimer<> cppTimer("CPP", "sleep", true);
    doSleep(inputDuration);
    cppTimer.stop();

    util::CTimer<> cTimer("C", "sleep", true);
    doSleep(inputDuration);
    cTimer.stop();
  }

  return 0;
}
