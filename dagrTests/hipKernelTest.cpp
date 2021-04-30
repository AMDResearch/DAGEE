#include "kernels.h"

#include "dagr/executor.h"

#include "cpputils/Timer.h"
#include "cpputils/CmdLine.h"

#include <iostream>
#include <string>


int main(int argc, char** argv) {
  dagr::RuntimeState S;
  dagr::GpuExecutionResource er(S.gpuAgent(0));

  auto* kinfoEmpty = er.kernInfoState().registerKernel<>(&emptyKern);

  dagr::SerialOrderedExecutor ordExec(&er);
  auto th = ordExec.launchTask(ordExec.makeTask(dim3(1), dim3(1), kinfoEmpty));

  ordExec.waitOnTask(th);

  return 0;
}
