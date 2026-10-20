#ifndef STORAGE_H
#define STORAGE_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdbool.h>

#define PATH_MAX_LEN 256

typedef enum {
    STORAGE_UNKNOWN = 0,
    STORAGE_UX0 = 1,       /* Sony Official Memory Card or internal storage */
    STORAGE_UMA0 = 2       /* SD2Vita or USB mass storage adapter */
} StorageMedium;

typedef struct {
    StorageMedium medium;
    char root_path[16];          /* "ux0:" or "uma0:" */
    char itls_dir[PATH_MAX_LEN];  /* "ux0:/data/itls" or "uma0:/data/itls" */
    uint64_t free_space_bytes;
    uint64_t total_space_bytes;
    bool is_sd2vita;
} StorageTarget;

/**
 * @brief Probes connected storage media (ux0: vs uma0:) and selects optimal target.
 * Priority: If uma0: (SD2Vita) is mounted and writable, select uma0:/data/itls.
 * Otherwise, fallback gracefully to ux0:/data/itls.
 */
int storage_detect_target(StorageTarget *target);

/**
 * @brief Ensures directory path exists recursively (mkdir -p).
 */
int storage_make_dir_recursive(const char *dir_path);

/**
 * @brief Check if a file or directory exists.
 */
bool storage_path_exists(const char *path);

/**
 * @brief Formats human-readable byte sizes (e.g., "128.4 MB").
 */
void storage_format_size(uint64_t bytes, char *out_str, size_t out_max);

#endif /* STORAGE_H */
