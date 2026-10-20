#include "pup_extractor.h"
#include "hw_crypto.h"
#include "storage.h"
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXTRACT_CHUNK_SIZE (128 * 1024)

const char *pup_get_entry_name(uint32_t file_id) {
    switch (file_id) {
        case 0x100: return "psp2swu.self";
        case 0x200: return "system.eop.cex";
        case 0x201: return "system.pup";
        case 0x300: return "preinst.pup";
        case 0x400: return "package.cat";
        case 0x500: return "itls.skprx";
        case 0x501: return "libssl.suprx";
        case 0x502: return "libhttp.suprx";
        case 0x503: return "ca-bundle.pem";
        default:    return "firmware_component.bin";
    }
}

int pup_extract_firmware(const char *pup_path, const char *output_dir,
                         PupExtractCallback callback, void *userdata) {
    if (!pup_path || !output_dir) return -1;

    PupExtractProgress progress;
    memset(&progress, 0, sizeof(progress));
    progress.status = PUP_EXTRACT_READING_HEADER;
    progress.hw_acceleration_active = true;
    if (callback) callback(&progress, userdata);

    /* Ensure output directory exists */
    storage_make_dir_recursive(output_dir);

    /* Open the downloaded PUP file */
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
        snprintf(progress.error_message, sizeof(progress.error_message), "PUP header too short");
        if (callback) callback(&progress, userdata);
        return -1;
    }

    /* Check magic */
    if (memcmp(header.magic, PUP_MAGIC_STRING, 5) != 0) {
        sceIoClose(fd);
        progress.status = PUP_EXTRACT_ERROR;
        progress.error_code = -2;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Invalid PUP container magic: %.5s", header.magic);
        if (callback) callback(&progress, userdata);
        return -2;
    }

    /* Initialize hardware crypto accelerator */
    hw_crypto_init();

    uint32_t file_count = header.file_count;
    if (file_count > PUP_MAX_ENTRIES) {
        file_count = PUP_MAX_ENTRIES;
    }
    progress.total_files = file_count;

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

    /* Iterate over each PUP entry */
    for (uint32_t i = 0; i < file_count; i++) {
        progress.current_file_index = i + 1;
        const char *entry_name = pup_get_entry_name(entries[i].file_id);
        snprintf(progress.current_file_name, sizeof(progress.current_file_name), "%s", entry_name);
        progress.status = PUP_EXTRACT_DECRYPTING_HW;
        if (callback) callback(&progress, userdata);

        char out_file_path[PATH_MAX_LEN];
        snprintf(out_file_path, sizeof(out_file_path), "%s/%s", output_dir, entry_name);

        SceUID out_fd = sceIoOpen(out_file_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (out_fd < 0) {
            continue; /* Skip inaccessible components */
        }

        sceIoLseek(fd, entries[i].file_offset, SCE_SEEK_SET);
        uint64_t remaining = entries[i].file_length;
        bool is_encrypted = (entries[i].flags & 1) != 0;

        uint8_t iv[AES_BLOCK_SIZE] = {0};

        while (remaining > 0) {
            size_t to_read = (remaining > EXTRACT_CHUNK_SIZE) ? EXTRACT_CHUNK_SIZE : (size_t)remaining;
            /* Ensure alignment for hardware AES block if needed */
            int r = sceIoRead(fd, io_buffer, to_read);
            if (r <= 0) break;

            if (is_encrypted) {
                /* Hardware-accelerated Bigmac AES decryption */
                size_t aligned_len = (r / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
                if (aligned_len > 0) {
                    hw_crypto_decrypt(io_buffer, dec_buffer, aligned_len,
                                      BIGMAC_KEYSLOT_PUP_ENC, iv, HW_CRYPTO_MODE_CBC);
                    /* Copy remaining unaligned bytes if any */
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
            progress.bytes_extracted += r;
        }

        sceIoClose(out_fd);
        progress.progress_percent = ((double)(i + 1) / (double)file_count) * 100.0;
        if (callback) callback(&progress, userdata);
    }

    /* Write metadata manifest for Safe-iTLS-Enso */
    char manifest_path[PATH_MAX_LEN];
    snprintf(manifest_path, sizeof(manifest_path), "%s/itls_firmware_info.txt", output_dir);
    SceUID mfd = sceIoOpen(manifest_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (mfd >= 0) {
        char manifest_content[512];
        HwCryptoStats stats;
        hw_crypto_get_stats(&stats);
        snprintf(manifest_content, sizeof(manifest_content),
                 "[Safe-iTLS-Enso Firmware Package Manifest]\n"
                 "PUP Image Version: 0x%08X\n"
                 "Target Storage: %s\n"
                 "Hardware Acceleration: PS Vita Bigmac DMA (Keyslot 0x%02X)\n"
                 "Decrypted Bytes: %llu\n"
                 "Status: Ready for TLS redirect hook installation\n",
                 header.image_version,
                 output_dir,
                 stats.active_keyslot,
                 (unsigned long long)stats.bytes_processed);
        sceIoWrite(mfd, manifest_content, strlen(manifest_content));
        sceIoClose(mfd);
    }

    free(io_buffer);
    free(dec_buffer);
    sceIoClose(fd);

    progress.status = PUP_EXTRACT_COMPLETED;
    progress.progress_percent = 100.0;
    if (callback) callback(&progress, userdata);

    return 0;
}
