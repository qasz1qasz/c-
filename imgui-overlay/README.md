# ImGui 悬浮窗（终端可执行版）

一个能在安卓终端里以 root 直接运行的 ImGui 悬浮窗，产物名 `gamemodifier_imgui`（arm64-v8a）。
窗口分两个区域：动态修改（指针链，可开关冻结）和静态修改（静态广角，基址 + 固定偏移）；
每个区域各有一个输入框和一个「确定修改」按钮，填完数值点确定才写入。

## 文件分工

| 文件 | 语言 | 作用 |
|---|---|---|
| `imgui_menu.c/.h` | C（cimgui） | 界面：样式（深色、圆角 8、主色紫 #6C4DFF）+ 动态/静态两个区域 + 各一个输入框和确定按钮 + 点输入框弹出的窗内数字键盘 + 动态滑动开关（自绘，带动画）+ 状态行 + 退出按钮 |
| `feature_write.c/.h` | C | 功能本体：两个用户提供的 GameGuardian 写法翻成的 C——动态：找 `com.rekoo.pubgm` 进程、取 `libUE4.so` 的 bss 段、按指针链（64 位）解析、把输入值按 float 写入，开关期间每秒写一次（freeze=true）；静态：地址 = `libUE4.so` 第一段映射基址（`getRangesList[1].start`）+ `0x37C03B8`，点按钮写一次（文件头有完整对照表） |
| `main.cpp` | C++ | 平台层：建悬浮层、OpenGL ES 渲染、主循环、后台写入线程 |
| `touch_input.cpp/.h` | C++ | 触摸：自己读 `/dev/input`，坐标按屏幕方向换算，经 ImGui 事件队列注入；窗体底部有触摸调试行 |
| `volume_keys.cpp/.h` | C++ | 音量键监听：只读旁听 `/dev/input` 的音量键事件，音量+ 显示窗体、音量- 隐藏窗体 |
| `patch_android16.py` | Python | 构建时给 AndroidImgui 的符号补丁表补 Android 16 条目（build.sh 调用） |

ImGui 本体是 C++ 库，所以界面代码用官方 C 绑定 cimgui 写成纯 C；
只有平台层（系统窗口/触摸接口本身就是 C++）用 C++。

## 依赖（build.sh 自动拉齐，版本钉死）

- Dear ImGui v1.92.7
- cimgui 1.92.7（与 ImGui 同版本，避免结构体对不齐）
- AndroidImgui v2.1（enenH/AndroidImgui）：通过系统 `libgui` 的 SurfaceComposerClient
  创建悬浮层；其 ImGui 子目录改为指向同一份 v1.92.7 源码。
  注：触摸不用它自带的 TouchHelperA（在另一台手机实测点不动、拖不动），改自己实现

## 编译

GitHub Actions 会在原有 workflow 里多编这一个产物；本地编：

```bash
cd imgui-overlay
bash build.sh   # 需要 Android NDK（27.x）和 cmake
```

产物在 `build/gamemodifier_imgui`，也会发布到仓库 Releases 的 `latest` 里。

## 运行

```bash
chmod 755 gamemodifier_imgui
su
./gamemodifier_imgui
```

- 先开游戏再用：动态区默认值 110、静态区默认值 2，在输入框里改成想要的数值，
  点该区的「确定修改」就立即写一次，窗口里显示结果（成功/失败原因）。
- 动态区的开关打开后，每秒按动态输入框的数值持续重写（冻结）；关掉即停止。
  静态区没有冻结，点一次按钮写一次。
- 输入靠点按：点输入框会弹出窗内自带数字键盘（数字、小数点、正负号、删除、
  清空、完成）。本程序是终端可执行文件、悬浮层没有安卓窗口的输入通道，
  系统输入法唤不起来，所以用自带键盘代替，效果一样是填数值。
- 退出：点窗体里的「退出程序」按钮（或标题栏的 X），悬浮层和程序一起关掉；也可以回终端按 Ctrl+C。
- 触摸是只读旁路，不拦截游戏操作；拖动窗口按住标题栏即可。
- 音量+ 显示窗体、音量- 隐藏窗体（隐藏只是不画窗，功能开关状态不变）；
  音量键同样是只读旁听、不拦截，所以按的时候系统音量也会跟着变。

## 已知限制

- 平台层依赖系统私有接口（libgui 符号），AndroidImgui 的符号表只到 Android 15；
  构建时用 `patch_android16.py` 给它补上了 Android 16 条目（与 15 同一套符号，
  已按 AOSP android-16.0.0_r1 源码核对）。程序启动时还会先自检一遍要用的系统接口，
  哪个缺失会直接打印名字并退出，不再像旧版那样空指针段错误、什么都看不到；
  个别厂商 ROM 若仍有符号差异，把终端里「缺失系统接口」那几行发回来即可继续适配。
- 指针链偏移与之前一样，游戏更新后可能失效，窗口状态行会提示「指针为空」。
- 真机显示效果以实跑为准（云端只能验证编译通过）。
