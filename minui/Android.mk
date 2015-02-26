LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := graphics.c graphics_adf.c graphics_fbdev.c events.c \
	resources.c

LOCAL_WHOLE_STATIC_LIBRARIES += libadf
LOCAL_STATIC_LIBRARIES += libpng

LOCAL_MODULE := libminui

# This used to compare against values in double-quotes (which are just
# ordinary characters in this context).  Strip double-quotes from the
# value so that either will work.

ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),ABGR_8888)
  LOCAL_CFLAGS += -DRECOVERY_ABGR
endif
ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),RGBX_8888)
  LOCAL_CFLAGS += -DRECOVERY_RGBX
endif
ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),BGRA_8888)
  LOCAL_CFLAGS += -DRECOVERY_BGRA
endif

ifneq ($(TARGET_RECOVERY_OVERSCAN_PERCENT),)
  LOCAL_CFLAGS += -DOVERSCAN_PERCENT=$(TARGET_RECOVERY_OVERSCAN_PERCENT)
else
  LOCAL_CFLAGS += -DOVERSCAN_PERCENT=0
endif

include $(BUILD_STATIC_LIBRARY)

# Used by OEMs for factory test images.
include $(CLEAR_VARS)
LOCAL_MODULE := libminui
LOCAL_WHOLE_STATIC_LIBRARIES += libminui
LOCAL_SHARED_LIBRARIES := libpng
include $(BUILD_SHARED_LIBRARY)
