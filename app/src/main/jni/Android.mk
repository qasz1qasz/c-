LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := gamemodifier
LOCAL_SRC_FILES := gamemodifier.c
LOCAL_LDLIBS    += -llog
LOCAL_CFLAGS    += -Wall -Wextra
include $(BUILD_SHARED_LIBRARY)
