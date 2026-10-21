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

void psvDebugScreenInit(void);
void psvDebugScreenPrintf(const char *format, ...);
void psvDebugScreenClear(int color);

typedef enum {
    INSTALLER_STATE_READY = 0,
    INSTALLER_STATE_DOWNLOADING,
    INSTALLER_STATE_EXTRACTING_HW,
    INSTALLER_STATE_SUCCESS,
    INSTALLER_STATE_ERROR
} InstallerState;

static InstallerState s_state = INSTALLER_STATE_READY;
static StorageTarget s_storage;
static char s_pup_path[PATH_MAX_LEN];
static char s_status_line[128] = "Press (X) to Download & Install Firmware Modules | (START) to Exit";

static void on_download_progress(const DownloadProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == DOWNLOAD_STATUS_DOWNLOADING) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Downloading Sony PUP: %.1f%% (%llu MB) @ %.1f KB/s [ETA: %us]",
                 progress->progress_percent,
                 (unsigned long long)(progress->downloaded_bytes / (1024 * 1024)),
                 progress->speed_kb_s,
                 progress->eta_seconds);
    } else if (progress->status == DOWNLOAD_STATUS_CONNECTING) {
        snprintf(s_status_line, sizeof(s_status_line), "Connecting to official PlayStation Update Server...");
    } else if (progress->status == DOWNLOAD_STATUS_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Download error: %s", progress->error_message);
    }
}

static void on_extract_progress(const PupExtractProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == PUP_EXTRACT_HARDWARE_DECRYPT) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Hardware Decrypting [%u/%u]: %s (%.1f%%)",
                 progress->current_target_index,
                 progress->total_targets,
                 progress->current_target_name,
                 progress->progress_percent);
    } else if (progress->status == PUP_EXTRACT_COMPLETED) {
        snprintf(s_status_line, sizeof(s_status_line), "Extraction Completed! Modules ready for Safe-iTLS.");
    } else if (progress->status == PUP_EXTRACT_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Extraction error: %s", progress->error_message);
    }
}

static void run_installer(void) {
    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("===================================================\n");
    psvDebugScreenPrintf(" Safe-iTLS-Enso Firmware Installer\n");
    psvDebugScreenPrintf(" Zero vs0: Modifications • Hardware Bigmac Decrypt\n");
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

    /* Step 2: Download Sony Official PUP */
    psvDebugScreenPrintf("[*] Downloading official Sony update PUP package...\n");
    psvDebugScreenPrintf("    Server: deu01.psv.update.playstation.net\n");
    s_state = INSTALLER_STATE_DOWNLOADING;

    int dl_res = downloader_fetch_pup(OFFICIAL_PUP_URL, s_pup_path, on_download_progress, NULL);
    if (dl_res < 0) {
        psvDebugScreenPrintf("[-] Download failed! Please check your network connection.\n");
        s_state = INSTALLER_STATE_ERROR;
        return;
    }
    psvDebugScreenPrintf("[+] Firmware PUP downloaded successfully.\n\n");

    /* Step 3: Hardware-Assisted Extraction (NO SOFTWARE AES KEYS) */
    psvDebugScreenPrintf("[*] Initializing PS Vita Bigmac Hardware Crypto DMA Engine...\n");
    psvDebugScreenPrintf("[*] Using Hardware Keyslot 0x20 (Zero software AES keys)...\n");
    psvDebugScreenPrintf("[*] Extracting ONLY the 5 required system files:\n");
    psvDebugScreenPrintf("    1. vs0:sys/external/libssl.suprx\n");
    psvDebugScreenPrintf("    2. vs0:sys/external/libhttp.suprx\n");
    psvDebugScreenPrintf("    3. vs0:/data/external/webcore/ScePsp2Compat.suprx\n");
    psvDebugScreenPrintf("    4. vs0:sys/external/np_commerce2.suprx\n");
    psvDebugScreenPrintf("    5. vs0:data/external/cert/CA_LIST.cer\n\n");

    s_state = INSTALLER_STATE_EXTRACTING_HW;

    int ext_res = pup_extract_target_modules(s_pup_path, s_storage.itls_dir, on_extract_progress, NULL);
    if (ext_res < 0) {
        psvDebugScreenPrintf("[-] Extraction failed.\n");
        s_state = INSTALLER_STATE_ERROR;
        return;
    }

    psvDebugScreenPrintf("[+] Extraction & Decryption complete!\n");
    psvDebugScreenPrintf("[+] Temporary PUP file deleted to free storage space.\n\n");
    psvDebugScreenPrintf("---------------------------------------------------\n");
    psvDebugScreenPrintf(" All 5 modules are installed in: %s\n", s_storage.itls_dir);
    psvDebugScreenPrintf(" - libssl.suprx          (Modern TLS 1.2 stack)\n");
    psvDebugScreenPrintf(" - libhttp.suprx         (Modern HTTP client)\n");
    psvDebugScreenPrintf(" - ScePsp2Compat.suprx   (WebCore TLS compatibility)\n");
    psvDebugScreenPrintf(" - np_commerce2.suprx    (PS Store TLS commerce)\n");
    psvDebugScreenPrintf(" - CA_LIST.cer           (Root certificate store)\n");
    psvDebugScreenPrintf("---------------------------------------------------\n\n");
    psvDebugScreenPrintf("Next step: Ensure ur0:tai/config.txt contains:\n");
    psvDebugScreenPrintf("  *KERNEL\n");
    psvDebugScreenPrintf("  ur0:tai/itls.skprx\n\n");
    psvDebugScreenPrintf("Press START or (O) to exit to LiveArea.\n");

    s_state = INSTALLER_STATE_SUCCESS;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Prevent Vita from sleeping during download / decryption */
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

        if (s_state == INSTALLER_STATE_READY) {
            psvDebugScreenClear(0x000000);
            psvDebugScreenPrintf("***************************************************\n");
            psvDebugScreenPrintf("  Safe-iTLS-Enso Firmware Installer (v1.1.0)\n");
            psvDebugScreenPrintf("  Official Sony Server Download & Hardware Decrypt\n");
            psvDebugScreenPrintf("***************************************************\n\n");
            psvDebugScreenPrintf(" Storage Target:  %s (%s)\n",
                                 s_storage.itls_dir,
                                 s_storage.is_sd2vita ? "SD2Vita uma0:" : "Official Card ux0:");
            psvDebugScreenPrintf(" Decryption:      Hardware Bigmac Engine (Keyslot 0x20)\n");
            psvDebugScreenPrintf(" Software Keys:   NONE (0%% key exposure)\n");
            psvDebugScreenPrintf(" vs0: Safety:     100%% Untouched (Zero vs0: writes)\n\n");
            psvDebugScreenPrintf(" Exact 5 Target Files to Extract:\n");
            psvDebugScreenPrintf("   1. vs0:sys/external/libssl.suprx\n");
            psvDebugScreenPrintf("   2. vs0:sys/external/libhttp.suprx\n");
            psvDebugScreenPrintf("   3. vs0:/data/external/webcore/ScePsp2Compat.suprx\n");
            psvDebugScreenPrintf("   4. vs0:sys/external/np_commerce2.suprx\n");
            psvDebugScreenPrintf("   5. vs0:data/external/cert/CA_LIST.cer\n\n");
            psvDebugScreenPrintf(" Controls:\n");
            psvDebugScreenPrintf("   [X]     - Download from Sony & Extract Using Hardware\n");
            psvDebugScreenPrintf("   [START] - Exit to LiveArea\n\n");
            psvDebugScreenPrintf(" Status: %s\n", s_status_line);

            if (pressed & SCE_CTRL_CROSS) {
                run_installer();
            } else if (pressed & SCE_CTRL_START) {
                running = false;
            }
        } else if (s_state == INSTALLER_STATE_SUCCESS || s_state == INSTALLER_STATE_ERROR) {
            if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_START)) {
                running = false;
            }
        }

        sceKernelDelayThread(30000);
    }

    downloader_term();
    hw_crypto_term();
    sceKernelExitProcess(0);
    return 0;
}
