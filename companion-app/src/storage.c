#include "storage.h"
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>

bool storage_path_exists(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    SceIoStat stat;
    memset(&stat, 0, sizeof(stat));
    int res = sceIoGetstat(path, &stat);
    return (res >= 0);
}

static bool partition_is_mounted(const char *dev_path) {
    SceUID dfd = sceIoDopen(dev_path);
    if (dfd >= 0) {
        sceIoDclose(dfd);
        return true;
    }
    return false;
}

void storage_probe_partitions(StorageAvailability *avail) {
    if (!avail) {
        return;
    }
    avail->uma0_mounted = partition_is_mounted("uma0:");
    avail->ux0_mounted  = partition_is_mounted("ux0:");
}

int storage_make_dir_recursive(const char *dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return -1;
    }

    char temp[PATH_MAX_LEN];
    snprintf(temp, sizeof(temp), "%s", dir_path);

    char *p = temp;
    /* Skip device prefix like "ux0:" or "uma0:" */
    char *colon = strchr(p, ':');
    if (colon) {
        p = colon + 1;
    }
    if (*p == '/') {
        p++;
    }

    while (*p) {
        if (*p == '/') {
            *p = '\0';
            sceIoMkdir(temp, 0777);
            *p = '/';
        }
        p++;
    }
    return sceIoMkdir(temp, 0777);
}

int storage_configure_target(StorageMedium medium, StorageTarget *target) {
    if (!target) {
        return -1;
    }
    memset(target, 0, sizeof(StorageTarget));

    if (medium == STORAGE_UMA0) {
        target->medium = STORAGE_UMA0;
        target->is_sd2vita = true;
        snprintf(target->root_path, sizeof(target->root_path), "uma0:");
        snprintf(target->itls_dir, sizeof(target->itls_dir), UMA0_ITLS_DIR);
        if (partition_is_mounted("uma0:")) {
            target->free_space_bytes = 1024ULL * 1024ULL * 512ULL;
            return 0;
        }
        return -1;
    }

    if (medium == STORAGE_UX0) {
        target->medium = STORAGE_UX0;
        target->is_sd2vita = false;
        snprintf(target->root_path, sizeof(target->root_path), "ux0:");
        snprintf(target->itls_dir, sizeof(target->itls_dir), UX0_ITLS_DIR);
        if (partition_is_mounted("ux0:")) {
            target->free_space_bytes = 1024ULL * 1024ULL * 512ULL;
            return 0;
        }
        return -1;
    }

    target->medium = STORAGE_UNKNOWN;
    snprintf(target->itls_dir, sizeof(target->itls_dir), UX0_ITLS_DIR);
    return -1;
}

int storage_detect_target(StorageTarget *target) {
    if (!target) {
        return -1;
    }

    /* 1. Priority: Probe uma0: (SD2Vita Adapter / Secondary Mass Storage) */
    if (storage_configure_target(STORAGE_UMA0, target) == 0) {
        return 0;
    }

    /* 2. Fallback: ux0: (Primary Sony Memory Card or Internal Storage) */
    if (storage_configure_target(STORAGE_UX0, target) == 0) {
        return 0;
    }

    target->medium = STORAGE_UNKNOWN;
    snprintf(target->itls_dir, sizeof(target->itls_dir), UX0_ITLS_DIR);
    return -1;
}

void storage_format_size(uint64_t bytes, char *out_str, size_t out_max) {
    if (!out_str || out_max == 0) {
        return;
    }
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double val = (double)bytes;
    while (val >= 1024.0 && idx < 4) {
        val /= 1024.0;
        idx++;
    }
    snprintf(out_str, out_max, "%.2f %s", val, units[idx]);
}
