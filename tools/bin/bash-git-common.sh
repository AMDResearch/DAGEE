# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash

SCRIPT_DIR=$(dirname $0)

source $SCRIPT_DIR/bash-common.sh

function assertGitRepo {
  local REPO_DIR="$1"

  cd $REPO_DIR && git status > /dev/null 2>&1

  if ! [ $? ] ; then 
    echo "$REPO_DIR not a git repository"
    exit 1
  fi
}

function assertHasSubmodule {
  local REPO_DIR="$1"
  local MOD_FILE=$REPO_DIR/.gitmodules

  if ! [ -f $MOD_FILE ] ; then
    echo "$REPO_DIR does not have submodule(s)"
    exit 1
  fi
}


