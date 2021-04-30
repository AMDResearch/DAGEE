#include "kernels.h"

#include "dagr/executor.h"

#include "cpputils/CmdLine.h"
#include "cpputils/Timer.h"

#include <iostream>
#include <string>

template <typename Exec>
void batchLaunch(Exec& exec, const dagr::GpuKernInfo* kinfoEmpty, size_t batchSz) {
  auto batchState = exec.startBatch();

  for (size_t i = 0; i < batchSz; ++i) {
    exec.addToBatch(batchState, exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
  }

  auto th = exec.finishBatch(batchState, exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
  exec.waitOnTask(th);
}

template <typename Exec>
void indivLaunch(Exec& exec, const dagr::GpuKernInfo* kinfoEmpty, size_t batchSz) {
  for (size_t i = 0; i < batchSz; ++i) {
    // exec.launchAndForget(exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
    exec.launchTask(exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
  }
  auto th = exec.launchTask(exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
  exec.waitOnTask(th);
}

template <typename Exec>
void launchAndForget(Exec& exec, const dagr::GpuKernInfo* kinfoEmpty, size_t batchSz) {
  for (size_t i = 0; i < batchSz; ++i) {
    exec.launchAndForget(exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
  }
  auto th = exec.launchTask(exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
  exec.waitOnTask(th);
}

template <typename Exec>
void syncLaunch(Exec& exec, const dagr::GpuKernInfo* kinfoEmpty, size_t batchSz) {
  for (size_t i = 0; i < batchSz; ++i) {
    auto th = exec.launchTask(exec.makeTask(dim3(1), dim3(1), kinfoEmpty));
    // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    exec.waitOnTask(th);
  }
}

template <typename Exec, typename LaunchFunc>
void launchExperiment(Exec& exec, const LaunchFunc& launchFunc, const char* expName,
                      const dagr::GpuKernInfo* kinfoEmpty, size_t batchSz, size_t warmUp = 100ul) {
  launchFunc(exec, kinfoEmpty, warmUp);

  cpputils::Timer t0(expName, "Total", true);
  launchFunc(exec, kinfoEmpty, batchSz);
  t0.stop();
}

template <typename Exec>
void launchVariants(Exec& exec, const dagr::GpuKernInfo* kinfoEmpty, size_t batchSz) {
  constexpr static const size_t NUM_REP = 10;

  for (size_t i = 0; i < NUM_REP; ++i) {
    launchExperiment(exec, &indivLaunch<decltype(exec)>, "Individual Launch", kinfoEmpty, batchSz);
  }

  for (size_t i = 0; i < NUM_REP; ++i) {
    launchExperiment(exec, &batchLaunch<decltype(exec)>, "Batch Launch", kinfoEmpty, batchSz);
  }

  for (size_t i = 0; i < NUM_REP; ++i) {
    launchExperiment(exec, &launchAndForget<decltype(exec)>, "Forgetful Launch", kinfoEmpty,
                     batchSz);
  }

  for (size_t i = 0; i < NUM_REP; ++i) {
    launchExperiment(exec, &syncLaunch<decltype(exec)>, "Synchronous Launch", kinfoEmpty, batchSz);
  }
}

int main(int argc, char** argv) {
  namespace cl = cpputils::cmdline;

  cl::Option<size_t> numTasksOpt('n', "Number of Tasks to launch", 1024);
  cl::Option<bool> orderedOpt('o', "Ordered vs Unordered Launch", false);

  cl::Parser parser({&numTasksOpt, &orderedOpt});
  parser.parse(argc, argv);

  dagr::RuntimeState S;
  dagr::GpuExecutionResource er(S.gpuAgent(0));

  auto* kinfoEmpty = er.kernInfoState().registerKernel<>(&emptyKern);

  dagr::DeviceMemManager dMemMgr(er.regionState().coarseRegion(0));

  dagr::SerialOrderedExecutor ordExec(&er);

  dagr::SerialUnorderedExecutor unordExec(&er, 4ul);

  if (orderedOpt) {
    launchVariants(ordExec, kinfoEmpty, size_t(numTasksOpt));
  } else {
    launchVariants(unordExec, kinfoEmpty, size_t(numTasksOpt));
  }

  return 0;
}
