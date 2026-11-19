#include "redirect.h"
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/io/stat.h>

static volatile int s_in_probe = 0;

static RedirectStatus s_status = {
    .active_target = REDIRECT_TARGET_NONE,
    .base_dir = "",
    .items = {
        {
            .orig_pattern_1 = "vs0:sys/external/libssl.suprx",
            .orig_pattern_2 = "vs0:/sys/external/libssl.suprx",
            .card_filename  = "libssl.suprx",
            .available      = false,
            .redirect_count = 0
        },
        {
            .orig_pattern_1 = "vs0:sys/external/libhttp.suprx",
            .orig_pattern_2 = "vs0:/sys/external/libhttp.suprx",
            .card_filename  = "libhttp.suprx",
            .available      = false,
            .redirect_count = 0
        },
        {
            .orig_pattern_1 = "vs0:data/external/webcore/ScePsp2Compat.suprx",
            .orig_pattern_2 = "vs0:/data/external/webcore/ScePsp2Compat.suprx",
            .card_filename  = "ScePsp2Compat.suprx",
            .available      = false,
            .redirect_count = 0
        },
        {
            .orig_pattern_1 = "vs0:sys/external/np_commerce2.suprx",
            .orig_pattern_2 = "vs0:/sys/external/np_commerce2.suprx",
            .card_filename  = "np_commerce2.suprx",
            .available      = false,
            .redirect_count = 0
        },
        {
            .orig_pattern_1 = "vs0:data/external/cert/CA_LIST.cer",
            .orig_pattern_2 = "vs0:/data/external/cert/CA_LIST.cer",
            .card_filename  = "CA_LIST.cer",
            .available      = false,
            .redirect_count = 0
        }
    },
    .total_redirects = 0,
    .fallback_count = 0
};

static bool file_exists_kernel(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    SceIoStat stat;
    memset(&stat, 0, sizeof(stat));
    int res = ksceIoGetstat(path, &stat);
    return (res >= 0 && stat.st_size > 0);
}

static bool probe_directory(const char *dir, RedirectTarget target) {
    int available_count = 0;
    char test_path[MAX_PATH_LEN];

    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        snprintf(test_path, sizeof(test_path), "%s/%s", dir, s_status.items[i].card_filename);
        bool exists = file_exists_kernel(test_path);
        s_status.items[i].available = exists;
        if (exists) {
            available_count++;
        }
    }

    if (available_count > 0) {
        s_status.active_target = target;
        snprintf(s_status.base_dir, sizeof(s_status.base_dir), "%s", dir);
        return true;
    }

    return false;
}

void redirect_refresh_storage(void) {
    if (s_in_probe) {
        return;
    }

    s_in_probe = 1;
    s_status.active_target = REDIRECT_TARGET_NONE;
    s_status.base_dir[0] = '\0';

    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        s_status.items[i].available = false;
    }

    /* 1. Priority: Probe SD2Vita (uma0:data/itls) */
    if (probe_directory(SD2VITA_ITLS_DIR, REDIRECT_TARGET_SD2VITA)) {
        s_in_probe = 0;
        return;
    }

    /* 2. Fallback: Probe Official Memory Card (ux0:data/itls) */
    probe_directory(MEMCARD_ITLS_DIR, REDIRECT_TARGET_MEMCARD);
    s_in_probe = 0;
}

int redirect_init(void) {
    redirect_refresh_storage();
    return 0;
}

bool redirect_resolve_path(const char *orig_path, char *output_path, size_t max_len) {
    if (s_in_probe || !orig_path || !output_path || max_len == 0) {
        return false;
    }

    /*
     * STRICT FILTER:
     * Check ONLY against the 5 specified target files:
     * 1. vs0:sys/external/libssl.suprx
     * 2. vs0:sys/external/libhttp.suprx
     * 3. vs0:/data/external/webcore/ScePsp2Compat.suprx
     * 4. vs0:sys/external/np_commerce2.suprx
     * 5. vs0:data/external/cert/CA_LIST.cer
     *
     * DO NOT REDIRECT ANY OTHER SYSTEM FILES.
     */
    int matched_idx = -1;
    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        if (strcmp(orig_path, s_status.items[i].orig_pattern_1) == 0 ||
            strcmp(orig_path, s_status.items[i].orig_pattern_2) == 0) {
            matched_idx = i;
            break;
        }
    }

    /* Any other path: strictly NOT redirected */
    if (matched_idx < 0) {
        return false;
    }

    /*
     * At *KERNEL boot time, uma0:/ux0: may not be mounted when module_start runs.
     * Lazily probe storage on the first matching target request.
     */
    if (s_status.active_target == REDIRECT_TARGET_NONE) {
        redirect_refresh_storage();
    }

    if (s_status.active_target != REDIRECT_TARGET_NONE &&
        s_status.items[matched_idx].available) {
        snprintf(output_path, max_len, "%s/%s",
                 s_status.base_dir, s_status.items[matched_idx].card_filename);
        s_status.items[matched_idx].redirect_count++;
        s_status.total_redirects++;
        return true;
    }

    /* Target file missing or card ejected; safe fallback to original vs0: */
    s_status.fallback_count++;
    return false;
}

void redirect_get_status(RedirectStatus *status) {
    if (!status) {
        return;
    }
    memcpy(status, &s_status, sizeof(RedirectStatus));
}
