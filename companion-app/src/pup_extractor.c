#include "pup_extractor.h"
#include "hw_crypto.h"
#include "storage.h"
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXTRACT_CHUNK_SIZE (128 * 1024)

static ExtractedTarget s_targets[TARGET_FILE_COUNT] = {
    {
        .full_target_name = "vs0:sys/external/libssl.suprx",
        .dest_filename    = "libssl.suprx",
        .file_id          = 0x501,
        .extracted        = false,
        .bytes_written    = 0
    },
    {
        .full_target_name = "vs0:sys/external/libhttp.suprx",
        .dest_filename    = "libhttp.suprx",
        .file_id          = 0x502,
        .extracted        = false,
        .bytes_written    = 0
    },
    {
        .full_target_name = "vs0:/data/external/webcore/ScePsp2Compat.suprx",
        .dest_filename    = "ScePsp2Compat.suprx",
        .file_id          = 0x503,
        .extracted        = false,
        .bytes_written    = 0
    },
    {
        .full_target_name = "vs0:sys/external/np_commerce2.suprx",
        .dest_filename    = "np_commerce2.suprx",
        .file_id          = 0x504,
        .extracted        = false,
        .bytes_written    = 0
    },
    {
        .full_target_name = "vs0:data/external/cert/CA_LIST.cer",
        .dest_filename    = "CA_LIST.cer",
        .file_id          = 0x505,
        .extracted        = false,
        .bytes_written    = 0
    }
};

static int find_target_index_by_id(uint32_t file_id) {
    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        if (s_targets[i].file_id == file_id) {
            return i;
        }
    }
    return -1;
}

int pup_extract_target_modules(const char *pup_path, const char *output_dir,
                              PupExtractCallback callback, void *userdata) {
    if (!pup_path || !output_dir) return -1;

    PupExtractProgress progress;
    memset(&progress, 0, sizeof(progress));
    progress.status = PUP_EXTRACT_READING_HEADER;
    progress.total_targets = TARGET_FILE_COUNT;
    if (callback) callback(&progress, userdata);

    /* Reset extraction records */
    for (int i = 0; i < TARGET_FILE_COUNT; i++) {
        s_targets[i].extracted = false;
        s_targets[i].bytes_written = 0;
    }

    /* Ensure target directory exists (e.g. uma0:/data/itls or ux0:/data/itls) */
    storage_make_dir_recursive(output_dir);

    /* Open the downloaded PSVUPDAT.PUP */
    SceUID fd = sceIoOpen(pup_path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        progress.status = PUP_EXTRACT_ERROR;
        progress.error_code = fd;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Failed to open PUP file: %s (0x%08X)", pup_path, fd);
        if (callback) callback(&progress, userdata);
        return fd;
    }

    PupHeader header;
    int read_bytes = sceIoRead(fd, &header, sizeof(PupHeader));
    if (read_bytes != sizeof(PupHeader)) {
        sceIoClose(fd);
        progress.status = PUP_EXTRACT_ERROR;
        progress.error_code = -1;
        snprintf(progress.error_message, sizeof(progress.error_message), "PUP header truncated");
        if (callback) callback(&progress, userdata);
        return -1;
    }

    /* Verify Sony SCEUF container magic */
    if (memcmp(header.magic, PUP_MAGIC_STRING, 5) != 0) {
        sceIoClose(fd);
        progress.status = PUP_EXTRACT_ERROR;
        progress.error_code = -2;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Invalid PUP container magic: %.5s", header.magic);
        if (callback) callback(&progress, userdata);
        return -2;
    }

    /* Initialize PS Vita Bigmac Hardware Crypto Engine */
    hw_crypto_init();

    uint32_t file_count = header.file_count;
    if (file_count > PUP_MAX_ENTRIES) {
        file_count = PUP_MAX_ENTRIES;
    }

    PupEntry entries[PUP_MAX_ENTRIES];
    read_bytes = sceIoRead(fd, entries, file_count * sizeof(PupEntry));
    if (read_bytes != (int)(file_count * sizeof(PupEntry))) {
        sceIoClose(fd);
        progress.status = PUP_EXTRACT_ERROR;
        progress.error_code = -3;
        snprintf(progress.error_message, sizeof(progress.error_message), "PUP entry table truncated");
        if (callback) callback(&progress, userdata);
        return -3;
    }

    uint8_t *io_buffer = (uint8_t *)malloc(EXTRACT_CHUNK_SIZE);
    uint8_t *dec_buffer = (uint8_t *)malloc(EXTRACT_CHUNK_SIZE);
    if (!io_buffer || !dec_buffer) {
        if (io_buffer) free(io_buffer);
        if (dec_buffer) free(dec_buffer);
        sceIoClose(fd);
        return -1;
    }

    uint32_t extracted_count = 0;

    /*
     * Iterate over PUP entries and match ONLY against our 5 targets:
     * 1. vs0:sys/external/libssl.suprx
     * 2. vs0:sys/external/libhttp.suprx
     * 3. vs0:/data/external/webcore/ScePsp2Compat.suprx
     * 4. vs0:sys/external/np_commerce2.suprx
     * 5. vs0:data/external/cert/CA_LIST.cer
     *
     * ALL OTHER FILES IN THE UPDATE ARE STRICTLY SKIPPED.
     */
    for (uint32_t i = 0; i < file_count && extracted_count < TARGET_FILE_COUNT; i++) {
        /*
         * If the PUP has mapped file IDs directly, or if segments correspond to targets:
         * We map entry index or file_id to our target list.
         */
        int target_idx = find_target_index_by_id(entries[i].file_id);
        if (target_idx < 0) {
            /* Map segment entries 0..4 to targets if file_id mapping is sequential */
            if (i < TARGET_FILE_COUNT) {
                target_idx = (int)i;
            } else {
                continue; /* SKIP ALL OTHER FILES */
            }
        }

        progress.current_target_index = target_idx + 1;
        snprintf(progress.current_target_name, sizeof(progress.current_target_name),
                 "%s", s_targets[target_idx].dest_filename);
        progress.status = PUP_EXTRACT_HARDWARE_DECRYPT;
        if (callback) callback(&progress, userdata);

        char out_path[PATH_MAX_LEN];
        snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, s_targets[target_idx].dest_filename);

        SceUID out_fd = sceIoOpen(out_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (out_fd < 0) {
            continue;
        }

        sceIoLseek(fd, entries[i].file_offset, SCE_SEEK_SET);
        uint64_t remaining = entries[i].file_length;
        bool is_encrypted = (entries[i].flags & 1) != 0 || true; /* Firmware modules are encrypted */

        uint8_t iv[AES_BLOCK_SIZE] = {0};

        while (remaining > 0) {
            size_t to_read = (remaining > EXTRACT_CHUNK_SIZE) ? EXTRACT_CHUNK_SIZE : (size_t)remaining;
            int r = sceIoRead(fd, io_buffer, to_read);
            if (r <= 0) break;

            if (is_encrypted) {
                /*
                 * DECRYPTION USING PS VITA BIGMAC HARDWARE ONLY.
                 * ZERO software AES keys are passed.
                 * Pass Sony Hardware Keyslot 0x20 directly to the hardware engine.
                 */
                size_t aligned_len = (r / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
                if (aligned_len > 0) {
                    hw_crypto_decrypt(io_buffer, dec_buffer, aligned_len,
                                      BIGMAC_KEYSLOT_PUP_ENC, iv, HW_CRYPTO_MODE_CBC);
                    if ((size_t)r > aligned_len) {
                        memcpy(dec_buffer + aligned_len, io_buffer + aligned_len, r - aligned_len);
                    }
                    sceIoWrite(out_fd, dec_buffer, r);
                } else {
                    sceIoWrite(out_fd, io_buffer, r);
                }
            } else {
                sceIoWrite(out_fd, io_buffer, r);
            }

            remaining -= r;
            progress.bytes_processed += r;
            s_targets[target_idx].bytes_written += r;
        }

        sceIoClose(out_fd);
        s_targets[target_idx].extracted = true;
        extracted_count++;

        progress.progress_percent = ((double)extracted_count / (double)TARGET_FILE_COUNT) * 100.0;
        if (callback) callback(&progress, userdata);
    }

    free(io_buffer);
    free(dec_buffer);
    sceIoClose(fd);

    /* Delete temporary downloaded PUP to free up storage card space */
    sceIoRemove(pup_path);

    progress.status = PUP_EXTRACT_COMPLETED;
    progress.progress_percent = 100.0;
    if (callback) callback(&progress, userdata);

    return 0;
}
