cmake_minimum_required(VERSION 3.22)
project(gamemodifier_imgui C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# AndroidImgui 的 CMake 里有一段兜底逻辑会改工具链，这里先把 NDK 路径定死避免它误判
if (DEFINED ANDROID_NDK AND NOT DEFINED CMAKE_ANDROID_NDK)
    set(CMAKE_ANDROID_NDK ${ANDROID_NDK})
endif ()

set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# 平台层：窗口创建 / 渲染后端（依赖由 build.sh 拉齐，imgui 子目录已指向 v1.92.7）
add_subdirectory(${DEPS_DIR}/AndroidImgui ${CMAKE_BINARY_DIR}/AndroidImgui)

# ImGui 1.92.7 的字体加载器声明有个坑：定义了 IMGUI_ENABLE_FREETYPE 后，
# stb_truetype 加载器默认不再启用，而 cimgui 的生成代码会无条件引用它，
# 所以必须两个宏一起开（官方注释也说明可以同时启用两个加载器）
target_compile_definitions(AndroidImgui PUBLIC IMGUI_ENABLE_STB_TRUETYPE)

# 界面 C 绑定：cimgui 1.92.7（与 Dear ImGui v1.92.7 同版本，避免结构体对不齐）
add_library(cimgui STATIC ${DEPS_DIR}/cimgui/cimgui.cpp)
target_include_directories(cimgui PUBLIC ${DEPS_DIR}/cimgui ${DEPS_DIR}/imgui)
target_compile_definitions(cimgui PUBLIC IMGUI_ENABLE_FREETYPE IMGUI_ENABLE_STB_TRUETYPE)
target_link_libraries(cimgui PUBLIC AndroidImgui)

add_executable(gamemodifier_imgui
        main.cpp
        touch_input.cpp
        imgui_menu.c
        feature_write.c)

target_include_directories(gamemodifier_imgui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DEPS_DIR}/cimgui)

target_link_libraries(gamemodifier_imgui PRIVATE
        AndroidImgui
        cimgui
        log
        android
        EGL
        GLESv3
        dl)
