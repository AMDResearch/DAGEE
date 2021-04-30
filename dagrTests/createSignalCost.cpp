#include "dagr/hsaCore.h"
#include "dagr/executor.h"

#include "cpputils/Timer.h"

#include <vector>

template <typename SP, typename StrT>
void allocateFromPool(const StrT& name) {

  constexpr static const size_t TRIALS = 1000;

  using VecPool = std::vector<SP>;
  VecPool pools;

  cpputils::Timer t0("Signal Pool Creation", name, true);
  for (size_t i = 0; i < TRIALS; ++i) {
    pools.emplace_back();
  }
  t0.stop();

  cpputils::Timer t1("Signal Pool Usage", name, true);
  for (auto& pool: pools) {
    for (size_t i = 0; i < SP::BATCH_SIZE; ++i) {
      auto sig = pool.allocate();
    }
  }
  t1.stop();
}

int main() {

  dagr::RuntimeState S;
  dagr::GpuExecutionResource er(S.gpuAgent(0));

  allocateFromPool<dagr::InterruptSignalPool>("Interruptible");
  // allocateFromPool<dagr::SignalPoolIpc>("Ipc");
  allocateFromPool<dagr::UserSignalPool>("User/GPU only");

  return 0;
}
