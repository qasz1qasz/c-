#!/usr/bin/env python3
# Android 16 适配补丁（构建时由 build.sh 调用）：
# AndroidImgui v2.1 的 ANativeWindowCreator 用一张按安卓版本查的符号补丁表，
# 表里只到 Android 15。Android 16 没条目时，它会去解析 Android 13 年代的旧符号
# （LayerMetadata 还在 android:: 命名空间、createSurface 的 flags 是 uint32_t），
# 这些符号在 Android 16 的 libgui 里不存在，解析结果为空指针，
# 一调用就 Segmentation fault。
#
# 已按 AOSP android-16.0.0_r1 的 libs/gui/include/gui/SurfaceComposerClient.h 核对：
# Android 16 的 createSurface 签名、gui::LayerMetadata、getPhysicalDisplayIds、
# getDisplayState、setTrustedOverlay 都与 Android 15 一致，
# 所以给补丁表补一条 16、内容与 15 相同即可。
#
# 用法：python3 patch_android16.py <ANativeWindowCreator.h 路径>
# 幂等：已经有 16 的条目就跳过；找不到 15 的条目（上游改版）就报错停构建。
import sys

def main() -> int:
    path = sys.argv[1]
    text = open(path, encoding="utf-8").read()

    if "\n                        16,\n" in text:
        print("patch_android16: 已有 Android 16 条目，跳过")
        return 0

    anchor = "\n                        15,\n"
    idx = text.find(anchor)
    if idx < 0:
        print("patch_android16: 找不到 Android 15 的补丁条目，上游可能改版，已停构建", file=sys.stderr)
        return 1

    # 条目从 anchor 前面那一行的 "{" 开始，按大括号配平找到条目结尾
    start = text.rfind("{", 0, idx)
    depth = 0
    end = -1
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end < 0:
        print("patch_android16: 条目大括号不配平，已停构建", file=sys.stderr)
        return 1

    block15 = text[start:end]
    block16 = block15.replace("\n                        15,\n", "\n                        16,\n", 1)
    if block16 == block15:
        print("patch_android16: 复制 16 条目失败，已停构建", file=sys.stderr)
        return 1

    text = text[:start] + block16 + ",\n                    " + text[start:]
    open(path, "w", encoding="utf-8").write(text)
    print("patch_android16: 已给 ANativeWindowCreator 补上 Android 16 条目（与 15 相同）")
    return 0

if __name__ == "__main__":
    sys.exit(main())
