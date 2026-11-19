#ifndef STORAGE_H
#define STORAGE_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdbool.h>

#define PATH_MAX_LEN 256

#define UMA0_ITLS_DIR "uma0:/data/itls"
#define UX0_ITLS_DIR  "ux0:/data/itls"

typedef enum {
    STORAGE_UNKNOWN = 0,
    STORAGE_UX0 = 1,       /* Sony Official Memory Card or internal storage */
    STORAGE_UMA0 = 2       /* SD2Vita or USB mass storage adapter */
} StorageMedium;

typedef enum {
    PARTITION_CHOICE_BOTH = 0, /* Install to BOTH uma0:/data/itls and ux0:/data/itls */
    PARTITION_CHOICE_UMA0 = 1, /* Install to uma0:/data/itls only */
    PARTITION_CHOICE_UX0  = 2  /* Install to ux0:/data/itls only */
} StoragePartitionChoice;

typedef struct {
    bool uma0_mounted;
    bool ux0_mounted;
} StorageAvailability;

typedef struct {
    StorageMedium medium;
    char root_path[16];           /* "ux0:" or "uma0:" */
    char itls_dir[PATH_MAX_LEN];  /* "ux0:/data/itls" or "uma0:/data/itls" */
    uint64_t free_space_bytes;
    uint64_t total_space_bytes;
    bool is_sd2vita;
} StorageTarget;

/**
 * @brief Probes whether uma0: and ux0: partitions are currently mounted.
 */
void storage_probe_partitions(StorageAvailability *avail);

/**
 * @brief Configures a StorageTarget struct for a specific partition (STORAGE_UMA0 or STORAGE_UX0).
 * @return 0 if the requested partition is mounted, -1 otherwise.
 */
int storage_configure_target(StorageMedium medium, StorageTarget *target);

/**
 * @brief Auto-probes connected storage media (uma0: -> ux0:) and selects default target.
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
 * @brief Stream-copy a single file from src_path to dst_path (used when mirroring to both partitions).
 * @return 0 on success, negative error code on failure.
 */
int storage_copy_file(const char *src_path, const char *dst_path);

/**
 * @brief Formats human-readable byte sizes (e.g., "128.4 MB").
 */
void storage_format_size(uint64_t bytes, char *out_str, size_t out_max);

#endif /* STORAGE_H */
