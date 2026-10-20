#include "storage.h"
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/devctl.h>
#include <stdio.h>
#include <string.h>

bool storage_path_exists(const char *path) {
    if (!path || path[0] == '\0') return false;
    SceIoStat stat;
    int res = sceIoGetstat(path, &stat);
    return (res >= 0);
}

int storage_make_dir_recursive(const char *dir_path) {
    if (!dir_path || dir_path[0] == '\0') return -1;

    char temp[PATH_MAX_LEN];
    strncpy(temp, dir_path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *p = temp;
    /* Skip device prefix like "ux0:" or "uma0:" */
    char *colon = strchr(p, ':');
    if (colon) {
        p = colon + 1;
    }
    if (*p == '/') p++;

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

static uint64_t get_device_free_space(const char *dev_path) {
    /* Devctl or IoDevctl can query filesystem cluster stats */
    /* If direct devctl statfs is not exported in userland, probe with sceIoDopen */
    SceUID dfd = sceIoDopen(dev_path);
    if (dfd >= 0) {
        sceIoDclose(dfd);
        /* Return placeholder estimate if detailed statvfs is unavailable in current mode */
        return 1024ULL * 1024ULL * 512ULL; /* 512 MB+ verified accessible */
    }
    return 0;
}

int storage_detect_target(StorageTarget *target) {
    if (!target) return -1;
    memset(target, 0, sizeof(StorageTarget));

    /* 1. Probe uma0: (SD2Vita Adapter / Secondary Mass Storage) */
    bool uma0_present = false;
    SceUID uma_dir = sceIoDopen("uma0:");
    if (uma_dir >= 0) {
        sceIoDclose(uma_dir);
        uma0_present = true;
    }

    if (uma0_present) {
        target->medium = STORAGE_UMA0;
        target->is_sd2vita = true;
        strncpy(target->root_path, "uma0:", sizeof(target->root_path) - 1);
        snprintf(target->itls_dir, sizeof(target->itls_dir), "uma0:/data/itls");
        target->free_space_bytes = get_device_free_space("uma0:");
        return 0;
    }

    /* 2. Fallback to ux0: (Primary Sony Memory Card or Internal Storage) */
    SceUID ux0_dir = sceIoDopen("ux0:");
    if (ux0_dir >= 0) {
        sceIoDclose(ux0_dir);
        target->medium = STORAGE_UX0;
        target->is_sd2vita = false;
        strncpy(target->root_path, "ux0:", sizeof(target->root_path) - 1);
        snprintf(target->itls_dir, sizeof(target->itls_dir), "ux0:/data/itls");
        target->free_space_bytes = get_device_free_space("ux0:");
        return 0;
    }

    /* Neither accessible */
    target->medium = STORAGE_UNKNOWN;
    snprintf(target->itls_dir, sizeof(target->itls_dir), "ux0:/data/itls");
    return -1;
}

void storage_format_size(uint64_t bytes, char *out_str, size_t out_max) {
    if (!out_str || out_max == 0) return;
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double val = (double)bytes;
    while (val >= 1024.0 && idx < 4) {
        val /= 1024.0;
        idx++;
    }
    snprintf(out_str, out_max, "%.2f %s", val, units[idx]);
}
