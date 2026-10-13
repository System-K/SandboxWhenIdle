#!/bin/sh
# SKSE-Hotfixes Linux cross build (clang-cl + xwin). Upstream CMakePresets.json contains comments,
# which CMake 4 rejects, so this script passes the preset values directly.
set -e
T=/mnt/h/dev/modding/toolchain
export PATH="$T/bin:$PATH" VCPKG_ROOT="$T/vcpkg" CommonLibSSEPath="$PWD/extern/CommonLibSSE"
cmake -S . -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE="$T/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$T/clang-cl-xwin.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-xwin -DVCPKG_OVERLAY_TRIPLETS="$T/triplets" \
  -DVCPKG_OVERLAY_PORTS="$T/ports" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
  -DXWIN_EXTRA_CXX_FLAGS="/permissive- /Zc:preprocessor -DNOMINMAX -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00" \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF -DBUILD_SKYRIMAE=ON -DBUILD_TESTS=OFF -DCOPY_BUILD=OFF -DNAME=Actions
cmake --build build/linux -j16
