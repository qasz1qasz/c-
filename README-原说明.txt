GameModifier - AIDE 项目源码使用说明
========================================

项目结构：
  GameModifier/
  ├── jni/
  │   ├── gamemodifier.c      # C原生代码
  │   ├── Android.mk           # NDK构建配置
  │   └── Application.mk       # NDK应用配置
  ├── app/
  │   └── src/
  │       └── com/
  │           └── gamemod/
  │               ├── MainActivity.java   # 主活动+悬浮窗UI
  │               └── NativeLib.java      # JNI原生方法声明
  ├── res/
  │   ├── layout/
  │   │   └── floating_window.xml   # 悬浮窗布局
  │   └── values/
  │       └── strings.xml           # 字符串资源
  ├── AndroidManifest.xml    # 权限配置
  └── project.properties     # 项目属性

功能说明：
  1. 获取游戏进程：点击"获取游戏进程"按钮，通过包名 com.rekoo.pubgm 查找进程PID
  2. 修改功能：点击"点击进行修改"按钮，自动解析指针链 0x56CFF0->0x30->0x4B8->0x360->0x170->0x39C，写入固定值110
  3. 防封功能：开关开启后，修改前会调用 AntiBanProtect 函数（函数体内留空，由你自己实现逻辑）

AIDE 打包步骤：
  1. 将项目文件夹复制到手机，例如 /storage/emulated/0/GameModifier/
  2. 打开AIDE -> 点击"打开项目" -> 选择 GameModifier 文件夹
  3. AIDE会自动识别项目结构
  4. 确保AIDE已安装NDK支持包（设置->NDK配置）
  5. 点击运行/构建，AIDE会编译Java和C代码生成APK
  6. 安装后首次打开会跳转权限设置页面，授予"悬浮窗权限"即可使用

注意事项：
  - 修改其他游戏进程内存需要 Root 权限，非Root设备只能查找进程无法读写内存
  - 0x56CFF0 当前按绝对地址处理，如果实际是模块偏移，需在 gamemodifier.c 中将
    uintptr_t current = offsets[0]; 改为 uintptr_t current = moduleBase + offsets[0];
  - 防封函数 AntiBanProtect 在 gamemodifier.c 中有明确标注，你在里面写自己的逻辑即可
  - 如果AIDE构建时报R文件找不到错误，检查 res/layout/floating_window.xml 文件名是否完全一致
