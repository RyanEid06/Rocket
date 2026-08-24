#!/usr/bin/env sh
# Source this file from a POSIX shell: . ./dependencies/activate.sh
rocket_dependency_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
rocket_llvm_root="$rocket_dependency_root/installed/llvm-22.1.6"
rocket_ninja_root="$rocket_dependency_root/installed/ninja-1.13.1"
PATH="$rocket_llvm_root/bin:$rocket_ninja_root:$PATH"
LLVM_DIR="$rocket_llvm_root/lib/cmake/llvm"
ROCKET_DEPS="$rocket_dependency_root"
export PATH LLVM_DIR ROCKET_DEPS
unset rocket_dependency_root rocket_llvm_root rocket_ninja_root
