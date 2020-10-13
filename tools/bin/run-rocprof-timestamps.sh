# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash 

NUM_SIGNALS=1024
NUM_QUEUES=16

PROG_ARGS="$*"
PREFIX=$(basename $1)
TIMESTAMP=$(date '+%Y-%m-%d--%H.%M.%S')

LOGFILE="${PREFIX}-${TIMESTAMP}.log"
if echo ${PREFIX} | grep -i bsp ; then
  OUTFILE="${PREFIX}-${TIMESTAMP}.csv"
  /opt/rocm/bin/rocprof --basenames on --timestamp on --stats -t .  -o ${OUTFILE} ${PROG_ARGS} 2>&1 | tee ${LOGFILE}
else
  for d in ATMI_SYNC_CALLBACK ATMI_SYNC_BARRIER_PKT; do
  # for d in ATMI_SYNC_BARRIER_PKT ; do
    OUTFILE="${PREFIX}-$d-${TIMESTAMP}.csv"
    ATMI_MAX_HSA_SIGNALS=${NUM_SIGNALS} ATMI_DEVICE_GPU_WORKERS=${NUM_QUEUES} \
    ATMI_DEPENDENCY_SYNC_TYPE=$d /opt/rocm/bin/rocprof --basenames on --timestamp on --stats -t .  -o ${OUTFILE} ${PROG_ARGS}
  done 2>&1 | tee ${LOGFILE}
fi
