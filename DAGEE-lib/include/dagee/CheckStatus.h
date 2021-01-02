// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_CHECK_STATUS_H
#define DAGEE_INCLUDE_DAGEE_CHECK_STATUS_H

#include "atmi.h"
#include "atmi_runtime.h"

#include "hip/hip_common.h"
#include "hip/hip_runtime.h"

#include "hsa/hsa.h"

#define CHECK_ATMI(cmd)                                                         \
  do {                                                                          \
    auto status = cmd;                                                          \
    if (status != ATMI_STATUS_SUCCESS) {                                        \
      fprintf(stderr, "error: %s failed at %s:%d\n", #cmd, __FILE__, __LINE__); \
    }                                                                           \
  } while (false);

#define CHECK_HIP(cmd)                                                                         \
  do {                                                                                         \
    hipError_t error = cmd;                                                                    \
    if (error != hipSuccess) {                                                                 \
      fprintf(stderr, "error: '%s'(%d) at %s:%d\n", hipGetErrorString(error), error, __FILE__, \
              __LINE__);                                                                       \
      exit(EXIT_FAILURE);                                                                      \
    }                                                                                          \
  } while (false);

#define CHECK_HSA(cmd)                                                                   \
  do {                                                                                   \
    hsa_status_t status = cmd;                                                           \
    if (status != HSA_STATUS_SUCCESS) {                                                  \
      char errStr[64];                                                                   \
      hsa_status_string(status, (const char**)&errStr);                                  \
      fprintf(stderr, "error: '%s'(%d) at %s:%d\n", errStr, status, __FILE__, __LINE__); \
    }                                                                                    \
  } while (false);

#endif // DAGEE_INCLUDE_DAGEE_CHECK_STATUS_H
