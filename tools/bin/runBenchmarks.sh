# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash

EXEC_DIR=$(dirname $0)

echo "EXEC_DIR = ${EXEC_DIR}"

CONFIG_DIR="${1:-${EXEC_DIR}/../../config}"

echo "CONFIG_DIR = ${CONFIG_DIR}"

PYTHON3=$(which python3)

eval "PYTHONPATH=${PYTHONPATH}:${CONFIG_DIR} ${PYTHON3} ${EXEC_DIR}/runBenchmarkConfig.py"



