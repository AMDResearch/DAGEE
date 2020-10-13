// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_DEVICE_DAG_H
#define DAGEE_INCLUDE_DAGEE_DEVICE_DAG_H

#include "dagee/TaskDAG.h"
#include "dagee/HIPinternal.h"

namespace dagee {

enum class DAGexecBackEnd: int {
  HSA=0, ATMI
};

enum class TaskLaunchMethod: int {
  HOST_CALLBACK=0, DEVICE_ENQ, BARRIER_PKT, CONT_FUNC
};

// class DeviceDAGpush: public StaticDAGpush<KernelInstance> {
// };
// 
// class DeviceDAGpull: public StaticDAGPull<KernelInstance> {
// };


}// end namespace dagee


#endif// DAGEE_INCLUDE_DAGEE_DEVICE_DAG_H
