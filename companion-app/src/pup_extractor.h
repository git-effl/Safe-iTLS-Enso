#ifndef PUP_EXTRACTOR_H
#define PUP_EXTRACTOR_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "hw_crypto.h"

#define PUP_MAGIC_STRING "SCEUF"
#define PUP_MAX_ENTRIES  64

#pragma pack(push, 1)
typedef struct {
    char magic[8];           /* "SCEUF\0\0\0" */
    uint32_t package_version;/* Package format version (1 or 2) */
    uint32_t image_version;  /* Firmware image revision */
    uint32_t file_count;     /* Total number of packed file segments */
    uint32_t header_length;  /* Header size including file table */
    uint64_t package_length; /* Total size in bytes of the PUP container */
} PupHeader;

typedef struct {
    uint32_t file_id;        /* Firmware component identifier */
    uint64_t file_offset;    /* Offset within PUP archive */
    uint64_t file_length;    /* Size of component in bytes */
    uint32_t flags;          /* Bit 0: Encrypted with Bigmac hardware keyslot */
    uint32_t reserved;
} PupEntry;
#pragma pack(pop)

typedef enum {
    PUP_EXTRACT_IDLE = 0,
    PUP_EXTRACT_READING_HEADER,
    PUP_EXTRACT_DECRYPTING_HW,
    PUP_EXTRACT_WRITING_FILES,
    PUP_EXTRACT_COMPLETED,
    PUP_EXTRACT_ERROR
} PupExtractStatus;

typedef struct {
    PupExtractStatus status;
    uint32_t current_file_index;
    uint32_t total_files;
    char current_file_name[64];
    uint64_t bytes_extracted;
    uint64_t total_payload_bytes;
    double progress_percent;
    bool hw_acceleration_active;
    int error_code;
    char error_message[128];
} PupExtractProgress;

typedef void (*PupExtractCallback)(const PupExtractProgress *progress, void *userdata);

/**
 * @brief Parses and extracts required Safe-iTLS firmware modules from PSVUPDAT.PUP
 * utilizing the PS Vita's on-chip Bigmac hardware crypto accelerator.
 *
 * @param pup_path Path to the downloaded PSVUPDAT.PUP.
 * @param output_dir Destination directory (e.g., "ux0:/data/itls" or "uma0:/data/itls").
 * @param callback Progress notification callback.
 * @param userdata User pointer.
 * @return 0 on success, negative error code on failure.
 */
int pup_extract_firmware(const char *pup_path, const char *output_dir,
                         PupExtractCallback callback, void *userdata);

/**
 * @brief Identifies human-readable name for a given PUP entry file ID.
 */
const char *pup_get_entry_name(uint32_t file_id);

#endif /* PUP_EXTRACTOR_H */
