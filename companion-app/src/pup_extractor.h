#ifndef PUP_EXTRACTOR_H
#define PUP_EXTRACTOR_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "hw_crypto.h"

#define PUP_MAGIC_STRING "SCEUF"
#define PUP_MAX_ENTRIES  128

/*
 * EXACT 5 TARGET FILES EXTRACTED FROM FIRMWARE PACKAGE:
 * 1. vs0:sys/external/libssl.suprx          -> libssl.suprx
 * 2. vs0:sys/external/libhttp.suprx         -> libhttp.suprx
 * 3. vs0:/data/external/webcore/ScePsp2Compat.suprx -> ScePsp2Compat.suprx
 * 4. vs0:sys/external/np_commerce2.suprx    -> np_commerce2.suprx
 * 5. vs0:data/external/cert/CA_LIST.cer     -> CA_LIST.cer
 *
 * ALL OTHER FILES IN THE UPDATE PACKAGE MUST BE SKIPPED.
 */

#define TARGET_FILE_COUNT 5

typedef struct {
    const char *full_target_name;
    const char *dest_filename;
    uint32_t file_id;
    bool extracted;
    uint64_t bytes_written;
} ExtractedTarget;

#pragma pack(push, 1)
typedef struct {
    char magic[8];           /* "SCEUF\0\0\0" */
    uint32_t package_version;/* Format version */
    uint32_t image_version;  /* Firmware revision (e.g. 0x03650011) */
    uint32_t file_count;     /* Total packed segments in PUP */
    uint32_t header_length;  /* Size of header + segment table */
    uint64_t package_length; /* Total size of PUP container */
} PupHeader;

typedef struct {
    uint32_t file_id;        /* Component identifier */
    uint64_t file_offset;    /* Offset within PUP archive */
    uint64_t file_length;    /* Size of component in bytes */
    uint32_t flags;          /* Bit 0: Encrypted with Bigmac hardware keyslot */
    uint32_t reserved;
} PupEntry;
#pragma pack(pop)

typedef enum {
    PUP_EXTRACT_IDLE = 0,
    PUP_EXTRACT_READING_HEADER,
    PUP_EXTRACT_HARDWARE_DECRYPT,
    PUP_EXTRACT_COMPLETED,
    PUP_EXTRACT_ERROR
} PupExtractStatus;

typedef struct {
    PupExtractStatus status;
    uint32_t current_target_index;
    uint32_t total_targets;
    char current_target_name[64];
    uint64_t bytes_processed;
    double progress_percent;
    int error_code;
    char error_message[128];
} PupExtractProgress;

typedef void (*PupExtractCallback)(const PupExtractProgress *progress, void *userdata);

/**
 * @brief Selectively extracts ONLY the 5 required system files from PSVUPDAT.PUP
 * strictly using the PS Vita's Bigmac hardware crypto accelerator (Keyslot 0x20).
 * ZERO software AES keys are used.
 *
 * Extracted files are placed directly into output_dir (e.g. "uma0:/data/itls" or "ux0:/data/itls").
 */
int pup_extract_target_modules(const char *pup_path, const char *output_dir,
                              PupExtractCallback callback, void *userdata);

#endif /* PUP_EXTRACTOR_H */
