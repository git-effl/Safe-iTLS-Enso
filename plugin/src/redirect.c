#include "redirect.h"
#include <psp2kern/kernel/clib.h>
#include <psp2kern/io/stat.h>

static RedirectStatus s_status = {0};

static bool file_exists_kernel(const char *path) {
    if (!path || path[0] == '\0') return false;
    SceIoStat stat;
    int res = ksceIoGetstat(path, &stat);
    return (res >= 0 && stat.st_size > 0);
}

static void probe_target_directory(const char *dir, RedirectTarget target) {
    char test_path[MAX_PATH_LEN];

    ksceClibSnprintf(test_path, sizeof(test_path), "%s/libssl.suprx", dir);
    bool has_ssl = file_exists_kernel(test_path);

    ksceClibSnprintf(test_path, sizeof(test_path), "%s/libhttp.suprx", dir);
    bool has_http = file_exists_kernel(test_path);

    ksceClibSnprintf(test_path, sizeof(test_path), "%s/ca-bundle.pem", dir);
    bool has_cert = file_exists_kernel(test_path);

    if (has_ssl || has_http || has_cert) {
        s_status.active_target = target;
        ksceClibStrncpy(s_status.base_dir, dir, sizeof(s_status.base_dir) - 1);
        s_status.libssl_available = has_ssl;
        s_status.libhttp_available = has_http;
        s_status.cert_available = has_cert;
    }
}

void redirect_refresh_storage(void) {
    s_status.active_target = REDIRECT_TARGET_NONE;
    s_status.base_dir[0] = '\0';
    s_status.libssl_available = false;
    s_status.libhttp_available = false;
    s_status.cert_available = false;

    /* 1. Priority: Probe SD2Vita (uma0:data/itls) */
    probe_target_directory(SD2VITA_ITLS_DIR, REDIRECT_TARGET_SD2VITA);

    /* 2. Fallback: Probe Official Memory Card (ux0:data/itls) */
    if (s_status.active_target == REDIRECT_TARGET_NONE) {
        probe_target_directory(MEMCARD_ITLS_DIR, REDIRECT_TARGET_MEMCARD);
    }
}

int redirect_init(void) {
    ksceClibMemset(&s_status, 0, sizeof(s_status));
    redirect_refresh_storage();
    return 0;
}

bool redirect_resolve_path(const char *orig_path, char *output_path, size_t max_len) {
    if (!orig_path || !output_path || max_len == 0) return false;

    /* If no SD card target with valid extracted modules is available, fallback to vs0: */
    if (s_status.active_target == REDIRECT_TARGET_NONE) {
        s_status.fallback_count++;
        return false;
    }

    /* Check for libssl.suprx redirection */
    if (ksceClibStrstr(orig_path, "vs0:sys/external/libssl.suprx") != NULL ||
        ksceClibStrstr(orig_path, "vs0:/sys/external/libssl.suprx") != NULL) {
        if (s_status.libssl_available) {
            ksceClibSnprintf(output_path, max_len, "%s/libssl.suprx", s_status.base_dir);
            s_status.redirect_count++;
            return true;
        }
    }

    /* Check for libhttp.suprx redirection */
    if (ksceClibStrstr(orig_path, "vs0:sys/external/libhttp.suprx") != NULL ||
        ksceClibStrstr(orig_path, "vs0:/sys/external/libhttp.suprx") != NULL) {
        if (s_status.libhttp_available) {
            ksceClibSnprintf(output_path, max_len, "%s/libhttp.suprx", s_status.base_dir);
            s_status.redirect_count++;
            return true;
        }
    }

    /* Check for system SSL root certificates redirection */
    if (ksceClibStrstr(orig_path, "vs0:data/external/cert") != NULL ||
        ksceClibStrstr(orig_path, "vs0:/data/external/cert") != NULL) {
        if (s_status.cert_available) {
            ksceClibSnprintf(output_path, max_len, "%s/ca-bundle.pem", s_status.base_dir);
            s_status.redirect_count++;
            return true;
        }
    }

    return false;
}

void redirect_get_status(RedirectStatus *status) {
    if (!status) return;
    ksceClibMemcpy(status, &s_status, sizeof(RedirectStatus));
}
