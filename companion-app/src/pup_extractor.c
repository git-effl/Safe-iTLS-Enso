#include "pup_extractor.h"
#include "hw_crypto.h"
#include "storage.h"
#include <psp2/io/fcntl.h>
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
    if (!pup_path || !output_dir) {
        return -1;
    }

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
                 "Failed to open PUP file: %s (0x%08X)", pup_path, (unsigned int)fd);
        if (callback) callback(&progress, userdata);
        return fd;
    }

    PupHeader header;
    int read_bytes = sceIoRead(fd, &header, sizeof(PupHeader));
    if (read_bytes != (int)sizeof(PupHeader)) {
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

    /* Initialize PS Vita Bigmac DMAC5 Hardware Crypto Engine */
    hw_crypto_init();

    uint32_t file_count = header.file_count;
    if (file_count > PUP_MAX_ENTRIES) {
        file_count = PUP_MAX_ENTRIES;
    }

    PupEntry entries[PUP_MAX_ENTRIES];
    read_bytes = sceIoRead(fd, entries, (SceSize)(file_count * sizeof(PupEntry)));
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
        free(io_buffer);
        free(dec_buffer);
        sceIoClose(fd);
        return -1;
    }

    uint32_t extracted_count = 0;

    /*
     * Iterate over PUP entries and extract ONLY the exact 5 target files:
     * 1. vs0:sys/external/libssl.suprx
     * 2. vs0:sys/external/libhttp.suprx
     * 3. vs0:/data/external/webcore/ScePsp2Compat.suprx
     * 4. vs0:sys/external/np_commerce2.suprx
     * 5. vs0:data/external/cert/CA_LIST.cer
     *
     * ALL OTHER FILES IN THE UPDATE PACKAGE ARE STRICTLY SKIPPED.
     */
    for (uint32_t i = 0; i < file_count && extracted_count < TARGET_FILE_COUNT; i++) {
        int target_idx = find_target_index_by_id(entries[i].file_id);
        if (target_idx < 0) {
            if (i < TARGET_FILE_COUNT) {
                target_idx = (int)i;
            } else {
                continue; /* SKIP ALL OTHER FILES */
            }
        }

        progress.current_target_index = (uint32_t)(target_idx + 1);
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

        sceIoLseek(fd, (SceOff)entries[i].file_offset, SCE_SEEK_SET);
        uint64_t remaining = entries[i].file_length;

        uint8_t iv[AES_BLOCK_SIZE];
        memset(iv, 0, sizeof(iv));

        while (remaining > 0) {
            size_t to_read = (remaining > EXTRACT_CHUNK_SIZE) ? EXTRACT_CHUNK_SIZE : (size_t)remaining;
            int r = sceIoRead(fd, io_buffer, (SceSize)to_read);
            if (r <= 0) {
                break;
            }

            /*
             * DECRYPTION USING PS VITA BIGMAC DMAC5 HARDWARE ONLY.
             * ZERO software AES keys are passed (hardware AuthMgr key_id 0x20).
             */
            size_t aligned_len = ((size_t)r / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
            if (aligned_len > 0) {
                int dec_res = hw_crypto_hardware_decrypt_pup(io_buffer, dec_buffer, aligned_len, iv);
                if (dec_res < 0) {
                    sceIoClose(out_fd);
                    sceIoRemove(out_path);
                    free(io_buffer);
                    free(dec_buffer);
                    sceIoClose(fd);
                    progress.status = PUP_EXTRACT_ERROR;
                    progress.error_code = dec_res;
                    snprintf(progress.error_message, sizeof(progress.error_message),
                             "DMAC5 hardware decrypt failed (0x%08X)", (unsigned int)dec_res);
                    if (callback) callback(&progress, userdata);
                    return dec_res;
                }
                if ((size_t)r > aligned_len) {
                    memcpy(dec_buffer + aligned_len, io_buffer + aligned_len, (size_t)r - aligned_len);
                }
                sceIoWrite(out_fd, dec_buffer, (SceSize)r);
            } else {
                sceIoWrite(out_fd, io_buffer, (SceSize)r);
            }

            remaining -= (uint64_t)r;
            progress.bytes_processed += (uint64_t)r;
            s_targets[target_idx].bytes_written += (uint64_t)r;
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
