LOCAL_PATH := $(call my-dir)
ROOT := $(LOCAL_PATH)/..

include $(CLEAR_VARS)
LOCAL_MODULE := menu

# FILL: layout assumed -> menu2/{*.cpp, imgui/, platform/, AvatarData.h}
LOCAL_SRC_FILES := \
    $(filter-out $(ROOT)/chams.cpp, $(wildcard $(ROOT)/*.cpp)) \
    $(wildcard $(ROOT)/imgui/*.cpp) \
    $(wildcard $(ROOT)/platform/*.cpp)

LOCAL_C_INCLUDES := $(ROOT) $(ROOT)/imgui $(ROOT)/platform
LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog

include $(BUILD_EXECUTABLE)
