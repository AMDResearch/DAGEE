# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash

function execCmd {
  local CMD="$1"
  local LOC_DRY_RUN=${DRY_RUN:=false}

  echo "Running: $CMD"

  if ! $LOC_DRY_RUN ; then 
    if ! eval "$CMD" ; then 
      echo "ERROR: failed to run: $CMD"
    fi
  fi
}

