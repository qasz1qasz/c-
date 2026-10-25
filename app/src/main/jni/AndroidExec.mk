LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := gamemodifier
LOCAL_SRC_FILES := modifier_main.c
LOCAL_CFLAGS    += -Wall -Wextra -O2
include $(BUILD_EXECUTABLE)
