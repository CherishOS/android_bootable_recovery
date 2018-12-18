/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include "roots.h"

#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <cryptfs.h>
#include <ext4_utils/wipe.h>
#include <fs_mgr.h>
#include <fs_mgr/roots.h>
#include <fs_mgr_dm_linear.h>

#include "otautil/mounts.h"

static struct fstab* fstab = nullptr;

extern struct selabel_handle* sehandle;

void load_volume_table() {
  fstab = fs_mgr_read_fstab_default();
  if (!fstab) {
    LOG(ERROR) << "Failed to read default fstab";
    return;
  }

  int ret = fs_mgr_add_entry(fstab, "/tmp", "ramdisk", "ramdisk");
  if (ret == -1) {
    LOG(ERROR) << "Failed to add /tmp entry to fstab";
    fs_mgr_free_fstab(fstab);
    fstab = nullptr;
    return;
  }

  printf("recovery filesystem table\n");
  printf("=========================\n");
  for (int i = 0; i < fstab->num_entries; ++i) {
    const Volume* v = &fstab->recs[i];
    printf("  %d %s %s %s %" PRId64 "\n", i, v->mount_point, v->fs_type, v->blk_device, v->length);
  }
  printf("\n");
}

Volume* volume_for_mount_point(const std::string& mount_point) {
  return fs_mgr_get_entry_for_mount_point(fstab, mount_point);
}

// Mount the volume specified by path at the given mount_point.
int ensure_path_mounted_at(const char* path, const char* mount_point) {
  return android::fs_mgr::EnsurePathMounted(fstab, path, mount_point) ? 0 : -1;
}

int ensure_path_mounted(const char* path) {
  // Mount at the default mount point.
  return android::fs_mgr::EnsurePathMounted(fstab, path) ? 0 : -1;
}

int ensure_path_unmounted(const char* path) {
  return android::fs_mgr::EnsurePathUnmounted(fstab, path) ? 0 : -1;
}

static int exec_cmd(const std::vector<std::string>& args) {
  CHECK_NE(static_cast<size_t>(0), args.size());

  std::vector<char*> argv(args.size());
  std::transform(args.cbegin(), args.cend(), argv.begin(),
                 [](const std::string& arg) { return const_cast<char*>(arg.c_str()); });
  argv.push_back(nullptr);

  pid_t child;
  if ((child = fork()) == 0) {
    execv(argv[0], argv.data());
    _exit(EXIT_FAILURE);
  }

  int status;
  waitpid(child, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    LOG(ERROR) << args[0] << " failed with status " << WEXITSTATUS(status);
  }
  return WEXITSTATUS(status);
}

static int64_t get_file_size(int fd, uint64_t reserve_len) {
  struct stat buf;
  int ret = fstat(fd, &buf);
  if (ret) return 0;

  int64_t computed_size;
  if (S_ISREG(buf.st_mode)) {
    computed_size = buf.st_size - reserve_len;
  } else if (S_ISBLK(buf.st_mode)) {
    uint64_t block_device_size = get_block_device_size(fd);
    if (block_device_size < reserve_len ||
        block_device_size > std::numeric_limits<int64_t>::max()) {
      computed_size = 0;
    } else {
      computed_size = block_device_size - reserve_len;
    }
  } else {
    computed_size = 0;
  }

  return computed_size;
}

int format_volume(const char* volume, const char* directory) {
  const Volume* v = android::fs_mgr::GetEntryForPath(fstab, volume);
  if (v == nullptr) {
    LOG(ERROR) << "unknown volume \"" << volume << "\"";
    return -1;
  }
  if (strcmp(v->fs_type, "ramdisk") == 0) {
    LOG(ERROR) << "can't format_volume \"" << volume << "\"";
    return -1;
  }
  if (strcmp(v->mount_point, volume) != 0) {
    LOG(ERROR) << "can't give path \"" << volume << "\" to format_volume";
    return -1;
  }
  if (ensure_path_unmounted(volume) != 0) {
    LOG(ERROR) << "format_volume: Failed to unmount \"" << v->mount_point << "\"";
    return -1;
  }
  if (strcmp(v->fs_type, "ext4") != 0 && strcmp(v->fs_type, "f2fs") != 0) {
    LOG(ERROR) << "format_volume: fs_type \"" << v->fs_type << "\" unsupported";
    return -1;
  }

  // If there's a key_loc that looks like a path, it should be a block device for storing encryption
  // metadata. Wipe it too.
  if (v->key_loc != nullptr && v->key_loc[0] == '/') {
    LOG(INFO) << "Wiping " << v->key_loc;
    int fd = open(v->key_loc, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
      PLOG(ERROR) << "format_volume: Failed to open " << v->key_loc;
      return -1;
    }
    wipe_block_device(fd, get_file_size(fd));
    close(fd);
  }

  int64_t length = 0;
  if (v->length > 0) {
    length = v->length;
  } else if (v->length < 0 ||
             (v->key_loc != nullptr && strcmp(v->key_loc, "footer") == 0)) {
    android::base::unique_fd fd(open(v->blk_device, O_RDONLY));
    if (fd == -1) {
      PLOG(ERROR) << "format_volume: failed to open " << v->blk_device;
      return -1;
    }
    length =
        get_file_size(fd.get(), v->length ? -v->length : CRYPT_FOOTER_OFFSET);
    if (length <= 0) {
      LOG(ERROR) << "get_file_size: invalid size " << length << " for "
                 << v->blk_device;
      return -1;
    }
  }

  if (strcmp(v->fs_type, "ext4") == 0) {
    static constexpr int kBlockSize = 4096;
    std::vector<std::string> mke2fs_args = {
      "/system/bin/mke2fs", "-F", "-t", "ext4", "-b", std::to_string(kBlockSize),
    };

    int raid_stride = v->logical_blk_size / kBlockSize;
    int raid_stripe_width = v->erase_blk_size / kBlockSize;
    // stride should be the max of 8KB and logical block size
    if (v->logical_blk_size != 0 && v->logical_blk_size < 8192) {
      raid_stride = 8192 / kBlockSize;
    }
    if (v->erase_blk_size != 0 && v->logical_blk_size != 0) {
      mke2fs_args.push_back("-E");
      mke2fs_args.push_back(
          android::base::StringPrintf("stride=%d,stripe-width=%d", raid_stride, raid_stripe_width));
    }
    mke2fs_args.push_back(v->blk_device);
    if (length != 0) {
      mke2fs_args.push_back(std::to_string(length / kBlockSize));
    }

    int result = exec_cmd(mke2fs_args);
    if (result == 0 && directory != nullptr) {
      std::vector<std::string> e2fsdroid_args = {
        "/system/bin/e2fsdroid", "-e", "-f", directory, "-a", volume, v->blk_device,
      };
      result = exec_cmd(e2fsdroid_args);
    }

    if (result != 0) {
      PLOG(ERROR) << "format_volume: Failed to make ext4 on " << v->blk_device;
      return -1;
    }
    return 0;
  }

  // Has to be f2fs because we checked earlier.
  static constexpr int kSectorSize = 4096;
  std::string cmd("/sbin/mkfs.f2fs");
  // clang-format off
  std::vector<std::string> make_f2fs_cmd = {
    cmd,
    "-g", "android",
    v->blk_device,
  };
  // clang-format on
  if (length >= kSectorSize) {
    make_f2fs_cmd.push_back(std::to_string(length / kSectorSize));
  }

  int result = exec_cmd(make_f2fs_cmd);
  if (result == 0 && directory != nullptr) {
    cmd = "/sbin/sload.f2fs";
    // clang-format off
    std::vector<std::string> sload_f2fs_cmd = {
      cmd,
      "-f", directory,
      "-t", volume,
      v->blk_device,
    };
    // clang-format on
    result = exec_cmd(sload_f2fs_cmd);
  }
  if (result != 0) {
    PLOG(ERROR) << "format_volume: Failed " << cmd << " on " << v->blk_device;
    return -1;
  }
  return 0;
}

int format_volume(const char* volume) {
  return format_volume(volume, nullptr);
}

int setup_install_mounts() {
  if (fstab == nullptr) {
    LOG(ERROR) << "can't set up install mounts: no fstab loaded";
    return -1;
  }
  for (int i = 0; i < fstab->num_entries; ++i) {
    const Volume* v = fstab->recs + i;

    // We don't want to do anything with "/".
    if (strcmp(v->mount_point, "/") == 0) {
      continue;
    }

    if (strcmp(v->mount_point, "/tmp") == 0 || strcmp(v->mount_point, "/cache") == 0) {
      if (ensure_path_mounted(v->mount_point) != 0) {
        LOG(ERROR) << "Failed to mount " << v->mount_point;
        return -1;
      }
    } else {
      if (ensure_path_unmounted(v->mount_point) != 0) {
        LOG(ERROR) << "Failed to unmount " << v->mount_point;
        return -1;
      }
    }
  }
  return 0;
}

bool logical_partitions_mapped() {
  return android::fs_mgr::LogicalPartitionsMapped();
}

std::string get_system_root() {
  return android::fs_mgr::GetSystemRoot();
}
