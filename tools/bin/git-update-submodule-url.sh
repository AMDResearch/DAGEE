# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash 

SUBMOD_NAME=${submod?"ERROR: Usage: <submod=value> <new_url=value> [repo_dir=pwd] $0"}
SUBMOD_URL=${new_url?"ERROR: Usage <submod=value> <new_url=value> [repo_dir=pwd] $0"}
REPO_DIR=${repo_dir:=`pwd`}


echo "$SUBMOD_NAME"
echo "$SUBMOD_URL"
echo "$REPO_DIR"

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/bash-git-common.sh

assertGitRepo $REPO_DIR
assertHasSubmodule $REPO_DIR

MOD_FILE=$REPO_DIR/.gitmodules

DRY_RUN=true
execCmd "git config --file=$MOD_FILE submodule.$SUBMOD_NAME.url $SUBMOD_URL"
execCmd "cd $REPO_DIR && git submodule sync"
execCmd "git add $MOD_FILE"




