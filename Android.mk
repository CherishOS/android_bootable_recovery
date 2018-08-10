# Copyright (C) 2007 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

# Needed by build/make/core/Makefile.
RECOVERY_API_VERSION := 3
RECOVERY_FSTAB_VERSION := 2

# TARGET_RECOVERY_UI_LIB should be one of librecovery_ui_{default,wear,vr} or a device-specific
# module that defines make_device() and the exact RecoveryUI class for the target. It defaults to
# librecovery_ui_default, which uses ScreenRecoveryUI.
TARGET_RECOVERY_UI_LIB ?= librecovery_ui_default

recovery_common_cflags := \
    -Wall \
    -Werror \
    -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

# librecovery_ui_ext (shared library)
# ===================================
include $(CLEAR_VARS)

LOCAL_MODULE := librecovery_ui_ext

# LOCAL_MODULE_PATH for shared libraries is unsupported in multiarch builds.
LOCAL_MULTILIB := first

ifeq ($(TARGET_IS_64_BIT),true)
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/lib64
else
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/lib
endif

LOCAL_WHOLE_STATIC_LIBRARIES := \
    $(TARGET_RECOVERY_UI_LIB)

LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblog \
    librecovery_ui

include $(BUILD_SHARED_LIBRARY)

# librecovery_ui (shared library)
# ===============================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    device.cpp \
    screen_ui.cpp \
    ui.cpp \
    vr_ui.cpp \
    wear_ui.cpp

LOCAL_MODULE := librecovery_ui

LOCAL_CFLAGS := $(recovery_common_cflags)

LOCAL_MULTILIB := first

ifeq ($(TARGET_IS_64_BIT),true)
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/lib64
else
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/lib
endif

LOCAL_STATIC_LIBRARIES := \
    libminui \
    libotautil \

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libpng \
    libz \

include $(BUILD_SHARED_LIBRARY)

# librecovery_ui (static library)
# ===============================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    device.cpp \
    screen_ui.cpp \
    ui.cpp \
    vr_ui.cpp \
    wear_ui.cpp

LOCAL_MODULE := librecovery_ui

LOCAL_CFLAGS := $(recovery_common_cflags)

LOCAL_STATIC_LIBRARIES := \
    libminui \
    libotautil \

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libpng \
    libz \

include $(BUILD_STATIC_LIBRARY)

# Health HAL dependency
health_hal_static_libraries := \
    android.hardware.health@2.0-impl \
    android.hardware.health@2.0 \
    android.hardware.health@1.0 \
    android.hardware.health@1.0-convert \
    libhealthstoragedefault \
    libhidltransport \
    libhidlbase \
    libhwbinder_noltopgo \
    libvndksupport \
    libbatterymonitor

librecovery_static_libraries := \
    libfusesideload \
    libminadbd \
    libminui \
    libverifier \
    libotautil \
    $(health_hal_static_libraries) \
    libvintf_recovery \
    libvintf \

librecovery_shared_libraries := \
    libasyncio \
    libbase \
    libbootloader_message \
    libcrypto \
    libcrypto_utils \
    libcutils \
    libext4_utils \
    libfs_mgr \
    libhidl-gen-utils \
    liblog \
    libpng \
    libselinux \
    libtinyxml2 \
    libutils \
    libz \
    libziparchive \

# librecovery (static library)
# ===============================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    adb_install.cpp \
    fsck_unshare_blocks.cpp \
    fuse_sdcard_provider.cpp \
    install.cpp \
    recovery.cpp \
    roots.cpp \

LOCAL_C_INCLUDES := \
    system/vold \

LOCAL_CFLAGS := $(recovery_common_cflags)

LOCAL_MODULE := librecovery

LOCAL_STATIC_LIBRARIES := \
    $(librecovery_static_libraries)

LOCAL_SHARED_LIBRARIES := \
    $(librecovery_shared_libraries)

include $(BUILD_STATIC_LIBRARY)

# recovery (static executable)
# ===============================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    logging.cpp \
    recovery_main.cpp \

LOCAL_MODULE := recovery

LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/bin

# Cannot link with LLD: undefined symbol: UsbNoPermissionsLongHelpText
# http://b/77543887, lld does not handle -Wl,--gc-sections as well as ld.
LOCAL_USE_CLANG_LLD := false

LOCAL_CFLAGS := $(recovery_common_cflags)

LOCAL_STATIC_LIBRARIES := \
    librecovery \
    librecovery_ui_default \
    $(librecovery_static_libraries)

LOCAL_SHARED_LIBRARIES := \
    librecovery_ui \
    $(librecovery_shared_libraries)

LOCAL_HAL_STATIC_LIBRARIES := libhealthd

LOCAL_REQUIRED_MODULES := \
    e2fsdroid.recovery \
    mke2fs.recovery \
    mke2fs.conf

ifeq ($(TARGET_USERIMAGES_USE_F2FS),true)
ifeq ($(HOST_OS),linux)
LOCAL_REQUIRED_MODULES += \
    sload.f2fs \
    mkfs.f2fs
endif
endif

# e2fsck is needed for adb remount -R.
ifeq ($(BOARD_EXT4_SHARE_DUP_BLOCKS),true)
ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_REQUIRED_MODULES += \
    e2fsck_static
endif
endif

ifeq ($(BOARD_CACHEIMAGE_PARTITION_SIZE),)
LOCAL_REQUIRED_MODULES += \
    recovery-persist \
    recovery-refresh
endif

LOCAL_REQUIRED_MODULES += \
    librecovery_ui_ext

# TODO(b/110380063): Explicitly install the following shared libraries to recovery, until `recovery`
# module is built with Soong (with `recovery: true` flag).
LOCAL_REQUIRED_MODULES += \
    libasyncio.recovery \
    libbase.recovery \
    libbootloader_message.recovery \
    libcrypto.recovery \
    libcrypto_utils.recovery \
    libcutils.recovery \
    libext4_utils.recovery \
    libfs_mgr.recovery \
    libhidl-gen-utils.recovery \
    liblog.recovery \
    libpng.recovery \
    libselinux.recovery \
    libsparse.recovery \
    libtinyxml2.recovery \
    libutils.recovery \
    libz.recovery \
    libziparchive.recovery \

include $(BUILD_EXECUTABLE)

include \
    $(LOCAL_PATH)/boot_control/Android.mk \
    $(LOCAL_PATH)/tests/Android.mk \
    $(LOCAL_PATH)/updater/Android.mk \
    $(LOCAL_PATH)/updater_sample/Android.mk \
