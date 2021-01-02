#!/bin/bash 

SCRIPT_DIR=$(dirname $0)
echo "SCRIPT_DIR=$SCRIPT_DIR"

REPO_ROOT="$(pwd)"
echo "REPO_ROOT=$REPO_ROOT"

if ! [ -d .git -a -f .gitmodules ]; then
  echo "Must run this script from the root of the repository"
  exit 1
fi;

function exec_cmd {
  local CMD="$1"
  local DRY_RUN=false

  echo "Running: $CMD"

  if ! $DRY_RUN ; then 
    eval "$CMD"
  fi
}

function check_tool_availability {
  local TOOL_NAME="$1"
  local INSTALLER_SUGGESTION="$2"

  if ! command -v  $TOOL_NAME > /dev/null 2>&1 ; then 
    echo "$TOOL_NAME not found on path. Please add to path or install. E.g., $INSTALLER_SUGGESTION install $TOOL_NAME"
  fi;
}

check_tool_availability "clang-format" "apt"
check_tool_availability "cpplint" "pip3"

function backup_and_link {
  local SOURCE="$1" # give path to SOURCE relative to TARGET
  local TARGET="$2"
  local TARGET_DIR="$(dirname $TARGET)"
  local TARGET_FILE="$(basename $TARGET)"
  
  if [ -f $TARGET ] ; then
    echo "Backing up existing $TARGET to $TARGET.old"
    BACKUP_CMD="mv $TARGET $TARGET.old"
    exec_cmd "$BACKUP_CMD"
  elif [ -L $TARGET ] ; then # is a symbolic link
    exec_cmd "rm $TARGET"
  fi

  pushd $TARGET_DIR
  if ! [ -f $SOURCE ]; then
    echo "$SOURCE not found"
    exit 1
  fi

  local LINK_CMD="ln -s $SOURCE $TARGET_FILE"
  exec_cmd "$LINK_CMD"
  popd 
}

PRE_COMMIT_PATH="$REPO_ROOT/.git/hooks/pre-commit"
backup_and_link "../../$SCRIPT_DIR/pre-commit" "$PRE_COMMIT_PATH"

DOT_CLANG_FORMAT_PATH="$REPO_ROOT/.clang-format"
backup_and_link "$SCRIPT_DIR/dot_clang_format" "$DOT_CLANG_FORMAT_PATH"

CPPLINT_CFG_PATH="$REPO_ROOT/CPPLINT.cfg"
backup_and_link "$SCRIPT_DIR/CPPLINT.cfg" "$CPPLINT_CFG_PATH"
