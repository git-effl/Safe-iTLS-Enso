#ifndef REDIRECT_H
#define REDIRECT_H

#include <psp2kern/types.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_PATH_LEN 256

/* Storage medium priority paths */
#define SD2VITA_ITLS_DIR "uma0:data/itls"
#define MEMCARD_ITLS_DIR "ux0:data/itls"

/* System modules to be redirected to SD card */
#define SYS_LIBSSL_PATH  "vs0:sys/external/libssl.suprx"
#define SYS_LIBHTTP_PATH "vs0:sys/external/libhttp.suprx"
#define SYS_CERT_DIR     "vs0:data/external/cert"

typedef enum {
    REDIRECT_TARGET_NONE = 0,
    REDIRECT_TARGET_SD2VITA,   /* uma0:/data/itls */
    REDIRECT_TARGET_MEMCARD    /* ux0:/data/itls */
} RedirectTarget;

typedef struct {
    RedirectTarget active_target;
    char base_dir[64];
    bool libssl_available;
    bool libhttp_available;
    bool cert_available;
    uint32_t redirect_count;
    uint32_t fallback_count;
} RedirectStatus;

/**
 * @brief Initialize redirect table and probe available storage media on SD card.
 * Priority: uma0: (SD2Vita) -> ux0: (Memory Card).
 */
int redirect_init(void);

/**
 * @brief Check if given path targets a redirected TLS module or certificate,
 * and if the replacement exists on the SD card, rewrite output_path.
 *
 * @param orig_path Original system path requested by application (e.g. vs0:sys/external/libssl.suprx).
 * @param output_path Buffer to store redirected path (e.g. uma0:data/itls/libssl.suprx).
 * @param max_len Maximum length of output_path.
 * @return true if redirection was performed, false if path should use original vs0: source.
 */
bool redirect_resolve_path(const char *orig_path, char *output_path, size_t max_len);

/**
 * @brief Get live redirection status and statistics.
 */
void redirect_get_status(RedirectStatus *status);

/**
 * @brief Re-probe storage card if card state changes.
 */
void redirect_refresh_storage(void);

#endif /* REDIRECT_H */
