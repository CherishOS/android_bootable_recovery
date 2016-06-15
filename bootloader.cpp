/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fs_mgr.h>

#include "bootloader.h"
#include "common.h"
#include "roots.h"
#include <android-base/unique_fd.h>

static int get_bootloader_message_block(bootloader_message* out, const Volume* v);
static int set_bootloader_message_block(const bootloader_message* in, const Volume* v);

int get_bootloader_message(bootloader_message* out) {
    Volume* v = volume_for_path("/misc");
    if (v == nullptr) {
        LOGE("Cannot load volume /misc!\n");
        return -1;
    }
    if (strcmp(v->fs_type, "emmc") == 0) {
        return get_bootloader_message_block(out, v);
    }
    LOGE("unknown misc partition fs_type \"%s\"\n", v->fs_type);
    return -1;
}

int set_bootloader_message(const bootloader_message* in) {
    Volume* v = volume_for_path("/misc");
    if (v == nullptr) {
        LOGE("Cannot load volume /misc!\n");
        return -1;
    }
    if (strcmp(v->fs_type, "emmc") == 0) {
        return set_bootloader_message_block(in, v);
    }
    LOGE("unknown misc partition fs_type \"%s\"\n", v->fs_type);
    return -1;
}

// ------------------------------------
// for misc partitions on block devices
// ------------------------------------

static void wait_for_device(const char* fn) {
    int tries = 0;
    int ret;
    do {
        ++tries;
        struct stat buf;
        ret = stat(fn, &buf);
        if (ret == -1) {
            printf("failed to stat \"%s\" try %d: %s\n", fn, tries, strerror(errno));
            sleep(1);
        }
    } while (ret && tries < 10);

    if (ret) {
        printf("failed to stat \"%s\"\n", fn);
    }
}

static int get_bootloader_message_block(bootloader_message* out,
                                        const Volume* v) {
    wait_for_device(v->blk_device);
    FILE* f = fopen(v->blk_device, "rb");
    if (f == nullptr) {
        LOGE("failed to open \"%s\": %s\n", v->blk_device, strerror(errno));
        return -1;
    }
    bootloader_message temp;
    int count = fread(&temp, sizeof(temp), 1, f);
    if (count != 1) {
        LOGE("failed to read \"%s\": %s\n", v->blk_device, strerror(errno));
        return -1;
    }
    if (fclose(f) != 0) {
        LOGE("failed to close \"%s\": %s\n", v->blk_device, strerror(errno));
        return -1;
    }
    memcpy(out, &temp, sizeof(temp));
    return 0;
}

static int set_bootloader_message_block(const bootloader_message* in,
                                        const Volume* v) {
    wait_for_device(v->blk_device);
    android::base::unique_fd fd(open(v->blk_device, O_WRONLY | O_SYNC));
    if (fd == -1) {
        LOGE("failed to open \"%s\": %s\n", v->blk_device, strerror(errno));
        return -1;
    }

    size_t written = 0;
    const uint8_t* start = reinterpret_cast<const uint8_t*>(in);
    size_t total = sizeof(*in);
    while (written < total) {
        ssize_t wrote = TEMP_FAILURE_RETRY(write(fd, start + written, total - written));
        if (wrote == -1) {
            LOGE("failed to write %" PRId64 " bytes: %s\n",
                 static_cast<off64_t>(written), strerror(errno));
            return -1;
        }
        written += wrote;
    }

    if (fsync(fd) == -1) {
        LOGE("failed to fsync \"%s\": %s\n", v->blk_device, strerror(errno));
        return -1;
    }
    return 0;
}
