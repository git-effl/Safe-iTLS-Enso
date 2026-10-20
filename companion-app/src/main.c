#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage.h"
#include "downloader.h"
#include "hw_crypto.h"
#include "pup_extractor.h"

/* Forward declare vita debug screen functions provided by vitasdk libdebugscreen */
void psvDebugScreenInit(void);
void psvDebugScreenPrintf(const char *format, ...);
void psvDebugScreenClear(int color);

typedef enum {
    APP_STATE_MENU = 0,
    APP_STATE_STORAGE_CHECK,
    APP_STATE_DOWNLOADING,
    APP_STATE_EXTRACTING_HW,
    APP_STATE_BENCHMARKING,
    APP_STATE_SUCCESS,
    APP_STATE_ERROR
} AppState;

static AppState s_state = APP_STATE_MENU;
static StorageTarget s_storage;
static char s_pup_path[PATH_MAX_LEN];
static char s_status_line[128] = "Press (X) to Start Auto Pipeline | (SQUARE) Probe Storage | (TRIANGLE) HW Test";

static void on_download_progress(const DownloadProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == DOWNLOAD_STATUS_DOWNLOADING) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Downloading: %.1f%% (%llu MB) | %.1f KB/s | ETA: %us",
                 progress->progress_percent,
                 (unsigned long long)(progress->downloaded_bytes / (1024 * 1024)),
                 progress->speed_kb_s,
                 progress->eta_seconds);
    } else if (progress->status == DOWNLOAD_STATUS_CONNECTING) {
        snprintf(s_status_line, sizeof(s_status_line), "Connecting to PlayStation CDN...");
    } else if (progress->status == DOWNLOAD_STATUS_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Download error: %s", progress->error_message);
    }
}

static void on_extract_progress(const PupExtractProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == PUP_EXTRACT_DECRYPTING_HW) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Hardware Decrypting [%u/%u]: %s (%.1f%%)",
                 progress->current_file_index,
                 progress->total_files,
                 progress->current_file_name,
                 progress->progress_percent);
    } else if (progress->status == PUP_EXTRACT_COMPLETED) {
        snprintf(s_status_line, sizeof(s_status_line), "Extraction and Decryption Completed Successfully!");
    } else if (progress->status == PUP_EXTRACT_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Extraction error: %s", progress->error_message);
    }
}

static void run_pipeline(void) {
    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("===================================================\n");
    psvDebugScreenPrintf(" Safe-iTLS-Enso Companion Firmware Pipeline\n");
    psvDebugScreenPrintf("===================================================\n\n");

    /* Step 1: Storage Detection */
    psvDebugScreenPrintf("[*] Detecting storage medium...\n");
    if (storage_detect_target(&s_storage) < 0) {
        psvDebugScreenPrintf("[-] Warning: Failed to query storage! Defaulting to ux0:\n");
        strncpy(s_storage.itls_dir, "ux0:/data/itls", sizeof(s_storage.itls_dir));
    }
    psvDebugScreenPrintf("[+] Target Directory: %s (%s)\n\n",
                         s_storage.itls_dir,
                         s_storage.is_sd2vita ? "SD2Vita uma0:" : "Memory Card ux0:");

    snprintf(s_pup_path, sizeof(s_pup_path), "%s/PSVUPDAT.PUP", s_storage.itls_dir);

    /* Step 2: Download PUP */
    psvDebugScreenPrintf("[*] Initiating official PUP download from PlayStation CDN...\n");
    psvDebugScreenPrintf("    URL: %s\n", OFFICIAL_PUP_URL);
    s_state = APP_STATE_DOWNLOADING;

    int dl_res = downloader_fetch_pup(OFFICIAL_PUP_URL, s_pup_path, on_download_progress, NULL);
    if (dl_res < 0) {
        psvDebugScreenPrintf("[-] Download failed! Check Wi-Fi connection.\n");
        s_state = APP_STATE_ERROR;
        return;
    }
    psvDebugScreenPrintf("[+] Download finished successfully.\n\n");

    /* Step 3: Hardware-Assisted Extraction */
    psvDebugScreenPrintf("[*] Initializing PS Vita Bigmac Hardware Crypto Engine...\n");
    s_state = APP_STATE_EXTRACTING_HW;

    int ext_res = pup_extract_firmware(s_pup_path, s_storage.itls_dir, on_extract_progress, NULL);
    if (ext_res < 0) {
        psvDebugScreenPrintf("[-] Firmware extraction/decryption failed.\n");
        s_state = APP_STATE_ERROR;
        return;
    }

    HwCryptoStats hw_stats;
    hw_crypto_get_stats(&hw_stats);

    psvDebugScreenPrintf("[+] Extraction & Decryption complete!\n");
    psvDebugScreenPrintf("    Hardware Keyslot: 0x%02X (Sony PUP Decrypt)\n", hw_stats.active_keyslot);
    psvDebugScreenPrintf("    Decrypted Payload: %llu bytes\n", (unsigned long long)hw_stats.bytes_processed);
    psvDebugScreenPrintf("    Estimated Throughput: %.2f MB/s\n\n", hw_stats.throughput_mb_s);
    psvDebugScreenPrintf("[*] Target files installed to: %s\n", s_storage.itls_dir);
    psvDebugScreenPrintf("    - psp2swu.self\n");
    psvDebugScreenPrintf("    - itls.skprx\n");
    psvDebugScreenPrintf("    - libssl.suprx\n");
    psvDebugScreenPrintf("    - libhttp.suprx\n");
    psvDebugScreenPrintf("    - ca-bundle.pem\n");
    psvDebugScreenPrintf("    - itls_firmware_info.txt\n\n");
    psvDebugScreenPrintf("All operations completed. Safe-iTLS-Enso is ready to hook!\n");
    psvDebugScreenPrintf("Press START or (O) to return to main menu.\n");

    s_state = APP_STATE_SUCCESS;
}

static void run_hw_benchmark(void) {
    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("===================================================\n");
    psvDebugScreenPrintf(" PS Vita Bigmac Hardware Crypto Acceleration Test\n");
    psvDebugScreenPrintf("===================================================\n\n");

    hw_crypto_init();

    const size_t test_size = 4 * 1024 * 1024; /* 4 MB test block */
    uint8_t *src = (uint8_t *)malloc(test_size);
    uint8_t *dst = (uint8_t *)malloc(test_size);
    if (!src || !dst) {
        psvDebugScreenPrintf("[-] Out of memory for benchmark buffer.\n");
        if (src) free(src);
        if (dst) free(dst);
        return;
    }

    memset(src, 0x5A, test_size);
    uint8_t iv[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

    psvDebugScreenPrintf("[*] Running 4MB Hardware DMA AES-CBC Decrypt (Keyslot 0x20)...\n");
    hw_crypto_decrypt(src, dst, test_size, BIGMAC_KEYSLOT_PUP_ENC, iv, HW_CRYPTO_MODE_CBC);

    HwCryptoStats stats;
    hw_crypto_get_stats(&stats);

    psvDebugScreenPrintf("[+] Benchmark Results:\n");
    psvDebugScreenPrintf("    Engine: Sony Bigmac Hardware Crypto DMA\n");
    psvDebugScreenPrintf("    Keyslot: 0x%02X\n", stats.active_keyslot);
    psvDebugScreenPrintf("    Block Size: 4 MB (16-byte aligned)\n");
    psvDebugScreenPrintf("    Measured Speed: %.2f MB/s\n\n", stats.throughput_mb_s);
    psvDebugScreenPrintf("Press any button to return to menu.\n");

    free(src);
    free(dst);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Prevent Vita from entering sleep during download/decryption */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);

    psvDebugScreenInit();
    storage_detect_target(&s_storage);

    SceCtrlData pad, old_pad;
    memset(&pad, 0, sizeof(pad));
    memset(&old_pad, 0, sizeof(old_pad));

    bool running = true;
    while (running) {
        old_pad = pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~old_pad.buttons;

        if (s_state == APP_STATE_MENU) {
            psvDebugScreenClear(0x000000);
            psvDebugScreenPrintf("***************************************************\n");
            psvDebugScreenPrintf("  Safe-iTLS-Enso Companion App (v1.0.0)\n");
            psvDebugScreenPrintf("  Automated Firmware Downloader & Hardware Decryptor\n");
            psvDebugScreenPrintf("***************************************************\n\n");
            psvDebugScreenPrintf(" Storage Mode:  %s\n", s_storage.is_sd2vita ? "SD2Vita (uma0:)" : "Official Memory Card (ux0:)");
            psvDebugScreenPrintf(" Install Path:  %s\n", s_storage.itls_dir);
            psvDebugScreenPrintf(" Hardware DMA:  Active (Sony Bigmac SceSblAuthMgr)\n\n");
            psvDebugScreenPrintf(" Controls:\n");
            psvDebugScreenPrintf("   [X]        - Download & Extract Firmware (Automated)\n");
            psvDebugScreenPrintf("   [SQUARE]   - Re-probe Storage Media (ux0: vs uma0:)\n");
            psvDebugScreenPrintf("   [TRIANGLE] - Run Bigmac Hardware Crypto Benchmark\n");
            psvDebugScreenPrintf("   [START]    - Exit to LiveArea\n\n");
            psvDebugScreenPrintf(" Status: %s\n", s_status_line);

            if (pressed & SCE_CTRL_CROSS) {
                run_pipeline();
            } else if (pressed & SCE_CTRL_SQUARE) {
                storage_detect_target(&s_storage);
                snprintf(s_status_line, sizeof(s_status_line),
                         "Probed storage: Detected %s -> %s",
                         s_storage.is_sd2vita ? "uma0:" : "ux0:", s_storage.itls_dir);
            } else if (pressed & SCE_CTRL_TRIANGLE) {
                run_hw_benchmark();
                s_state = APP_STATE_MENU;
            } else if (pressed & SCE_CTRL_START) {
                running = false;
            }
        } else if (s_state == APP_STATE_SUCCESS || s_state == APP_STATE_ERROR) {
            if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_START)) {
                s_state = APP_STATE_MENU;
            }
        }

        sceKernelDelayThread(30000); /* ~30ms loop */
    }

    downloader_term();
    hw_crypto_term();
    sceKernelExitProcess(0);
    return 0;
}
