#!/usr/bin/env bash
# 拉齐依赖并用 Android NDK + CMake 编译 gamemodifier_imgui（arm64-v8a）
# 依赖版本全部钉死：
#   Dear ImGui   v1.92.7 (dac07199cfd761113d966eb8ad739254e10df2fe)
#   cimgui       1.92.7  (d3f0c2f4a7d4d116ef908295b971a36bdfdafe27)
#   AndroidImgui v2.1    (63c9d83620665f87dce26e0a6de9f209b6d69593)，其 freetype 用它自带的子模块锁定版本
set -euo pipefail
cd "$(dirname "$0")"

NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}"
if [ -z "$NDK_HOME" ] && [ -n "${ANDROID_HOME:-}" ]; then
    # 取已安装的最高版本 NDK
    NDK_HOME="$(ls -d "$ANDROID_HOME"/ndk/* 2>/dev/null | sort -V | tail -1 || true)"
fi
if [ -z "$NDK_HOME" ] || [ ! -f "$NDK_HOME/build/cmake/android.toolchain.cmake" ]; then
    echo "找不到 Android NDK，请先安装（sdkmanager \"ndk;27.0.12077973\"）" >&2
    exit 1
fi
echo "使用 NDK: $NDK_HOME"

mkdir -p deps

if [ ! -d deps/imgui/.git ]; then
    git clone --depth 1 https://github.com/ocornut/imgui.git deps/imgui
    git -C deps/imgui fetch --depth 1 origin dac07199cfd761113d966eb8ad739254e10df2fe
    git -C deps/imgui checkout -q FETCH_HEAD
fi

if [ ! -d deps/cimgui/.git ]; then
    git clone --depth 1 --branch 1.92.7 https://github.com/cimgui/cimgui.git deps/cimgui
fi
# cimgui.cpp 按相对路径 ./imgui/imgui.h 找头文件，把它指到同一份 ImGui 源码
rm -rf deps/cimgui/imgui
ln -sfn "$PWD/deps/imgui" deps/cimgui/imgui

if [ ! -d deps/AndroidImgui/.git ]; then
    git clone --depth 1 --branch v2.1 https://github.com/enenH/AndroidImgui.git deps/AndroidImgui
fi
git -C deps/AndroidImgui submodule update --init freetype
rm -rf deps/AndroidImgui/imgui
ln -sfn "$PWD/deps/imgui" deps/AndroidImgui/imgui

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE="$NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build -j"$(nproc)"
ls -lh build/gamemodifier_imgui
file build/gamemodifier_imgui || true
