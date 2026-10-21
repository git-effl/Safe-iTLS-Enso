#include "redirect.h"
#include <psp2kern/kernel/clib.h>
#include <psp2kern/io/stat.h>

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
    if (!path || path[0] == '\0') return false;
    SceIoStat stat;
    int res = ksceIoGetstat(path, &stat);
    return (res >= 0 && stat.st_size > 0);
}

static bool probe_directory(const char *dir, RedirectTarget target) {
    int available_count = 0;
    char test_path[MAX_PATH_LEN];

    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        ksceClibSnprintf(test_path, sizeof(test_path), "%s/%s", dir, s_status.items[i].card_filename);
        bool exists = file_exists_kernel(test_path);
        s_status.items[i].available = exists;
        if (exists) {
            available_count++;
        }
    }

    if (available_count > 0) {
        s_status.active_target = target;
        ksceClibStrncpy(s_status.base_dir, dir, sizeof(s_status.base_dir) - 1);
        return true;
    }

    return false;
}

void redirect_refresh_storage(void) {
    s_status.active_target = REDIRECT_TARGET_NONE;
    s_status.base_dir[0] = '\0';

    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        s_status.items[i].available = false;
    }

    /* 1. Priority: Probe SD2Vita (uma0:data/itls) */
    if (probe_directory(SD2VITA_ITLS_DIR, REDIRECT_TARGET_SD2VITA)) {
        return;
    }

    /* 2. Fallback: Probe Official Memory Card (ux0:data/itls) */
    probe_directory(MEMCARD_ITLS_DIR, REDIRECT_TARGET_MEMCARD);
}

int redirect_init(void) {
    redirect_refresh_storage();
    return 0;
}

bool redirect_resolve_path(const char *orig_path, char *output_path, size_t max_len) {
    if (!orig_path || !output_path || max_len == 0) return false;

    /* If no storage card directory has extracted modules, pass through to vs0: */
    if (s_status.active_target == REDIRECT_TARGET_NONE) {
        s_status.fallback_count++;
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
    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        if (ksceClibStrstr(orig_path, s_status.items[i].orig_pattern_1) != NULL ||
            ksceClibStrstr(orig_path, s_status.items[i].orig_pattern_2) != NULL) {

            /* File matched one of the 5 targets! Verify presence on storage card */
            if (s_status.items[i].available) {
                ksceClibSnprintf(output_path, max_len, "%s/%s",
                                 s_status.base_dir, s_status.items[i].card_filename);
                s_status.items[i].redirect_count++;
                s_status.total_redirects++;
                return true;
            } else {
                /* Target file missing on card; safe fallback to original vs0: */
                s_status.fallback_count++;
                return false;
            }
        }
    }

    /* Any other path: strictly NOT redirected */
    return false;
}

void redirect_get_status(RedirectStatus *status) {
    if (!status) return;
    ksceClibMemcpy(status, &s_status, sizeof(RedirectStatus));
}
