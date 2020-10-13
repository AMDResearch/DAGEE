# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash

SUBMOD_NAME=${submod?"ERROR: Usage: <submod=value> <new_path=value> [repo_dir=pwd] $0"}
SUBMOD_PATH=${new_path?"ERROR: Usage <submod=value> <new_path=value> [repo_dir=pwd] $0"}
REPO_DIR=${repo_dir:=`pwd`}


SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/bash-git-common.sh

assertGitRepo $REPO_DIR
assertHasSubmodule $REPO_DIR

MOD_FILE=$REPO_DIR/.gitmodules

OLD_SUBMOD_PATH=$(git config --file=$MOD_FILE submodule.$SUBMOD_NAME.path)

DRY_RUN=true
execCmd "git mv $OLD_SUBMOD_PATH $SUBMOD_PATH"
execCmd "git config --file=$MOD_FILE submodule.$SUBMOD_NAME.path $SUBMOD_PATH"
execCmd "cd $REPO_DIR && git submodule sync"
execCmd "git add $MOD_FILE"
execCmd "git submodule update --init"
