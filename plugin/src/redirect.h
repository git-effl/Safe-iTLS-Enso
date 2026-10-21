#ifndef REDIRECT_H
#define REDIRECT_H

#include <psp2kern/types.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_PATH_LEN 256

/* Storage medium priority paths */
#define SD2VITA_ITLS_DIR "uma0:data/itls"
#define MEMCARD_ITLS_DIR "ux0:data/itls"

/*
 * The EXACT 5 target files allowed for redirection:
 * 1. vs0:sys/external/libssl.suprx
 * 2. vs0:sys/external/libhttp.suprx
 * 3. vs0:/data/external/webcore/ScePsp2Compat.suprx
 * 4. vs0:sys/external/np_commerce2.suprx
 * 5. vs0:data/external/cert/CA_LIST.cer
 * NO OTHER FILES SHALL BE REDIRECTED.
 */

#define TARGET_FILE_COUNT 5

typedef struct {
    const char *orig_pattern_1; /* e.g. "vs0:sys/external/libssl.suprx" */
    const char *orig_pattern_2; /* with or without leading slash e.g. "vs0:/sys/external/libssl.suprx" */
    const char *card_filename;  /* e.g. "libssl.suprx" */
    bool available;
    uint32_t redirect_count;
} RedirectItem;

typedef enum {
    REDIRECT_TARGET_NONE = 0,
    REDIRECT_TARGET_SD2VITA,   /* uma0:data/itls */
    REDIRECT_TARGET_MEMCARD    /* ux0:data/itls */
} RedirectTarget;

typedef struct {
    RedirectTarget active_target;
    char base_dir[64];
    RedirectItem items[TARGET_FILE_COUNT];
    uint32_t total_redirects;
    uint32_t fallback_count;
} RedirectStatus;

/**
 * @brief Initialize redirect table and probe storage card.
 * Priority: uma0:data/itls -> ux0:data/itls.
 */
int redirect_init(void);

/**
 * @brief Checks if given path targets one of the EXACT 5 allowed files.
 * If verified on card, rewrites to card path. Otherwise returns false.
 */
bool redirect_resolve_path(const char *orig_path, char *output_path, size_t max_len);

/**
 * @brief Get live redirection status and statistics.
 */
void redirect_get_status(RedirectStatus *status);

/**
 * @brief Re-probe storage card.
 */
void redirect_refresh_storage(void);

#endif /* REDIRECT_H */
