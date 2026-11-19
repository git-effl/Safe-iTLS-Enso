#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_screen.h"
#include "storage.h"
#include "downloader.h"
#include "hw_crypto.h"
#include "pup_extractor.h"

typedef enum {
    INSTALLER_STATE_SELECT_PARTITION = 0,
    INSTALLER_STATE_DOWNLOADING,
    INSTALLER_STATE_EXTRACTING_HW,
    INSTALLER_STATE_SUCCESS,
    INSTALLER_STATE_ERROR
} InstallerState;

static InstallerState s_state = INSTALLER_STATE_SELECT_PARTITION;
static StorageAvailability s_avail;
static StoragePartitionChoice s_choice = PARTITION_CHOICE_UX0;
static char s_pup_path[PATH_MAX_LEN];
static char s_status_line[128] = "Use UP/DOWN to choose uma0: or ux0:, then press (X) to Install.";

static const char *get_choice_label(StoragePartitionChoice choice) {
    return (choice == PARTITION_CHOICE_UMA0)
        ? "uma0:/data/itls (SD2Vita / USB)"
        : "ux0:/data/itls (Memory Card / Primary)";
}

static void on_download_progress(const DownloadProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == DOWNLOAD_STATUS_DOWNLOADING) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Downloading Sony PUP: %.1f%% (%llu MB) @ %.1f KB/s [ETA: %us]",
                 progress->progress_percent,
                 (unsigned long long)(progress->downloaded_bytes / (1024 * 1024)),
                 progress->speed_kb_s,
                 (unsigned int)progress->eta_seconds);
        psvDebugScreenPrintf("\r    %s", s_status_line);
    } else if (progress->status == DOWNLOAD_STATUS_CONNECTING) {
        snprintf(s_status_line, sizeof(s_status_line), "Connecting to official PlayStation Update Server...");
        psvDebugScreenPrintf("    %s\n", s_status_line);
    } else if (progress->status == DOWNLOAD_STATUS_COMPLETED) {
        psvDebugScreenPrintf("\n");
    } else if (progress->status == DOWNLOAD_STATUS_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Download error: %s", progress->error_message);
        psvDebugScreenPrintf("\n[-] %s\n", s_status_line);
    }
}

static void on_extract_progress(const PupExtractProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == PUP_EXTRACT_HARDWARE_DECRYPT) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Hardware Decrypting [%u/%u]: %s (%.1f%%)",
                 (unsigned int)progress->current_target_index,
                 (unsigned int)progress->total_targets,
                 progress->current_target_name,
                 progress->progress_percent);
        psvDebugScreenPrintf("    %s\n", s_status_line);
    } else if (progress->status == PUP_EXTRACT_COMPLETED) {
        snprintf(s_status_line, sizeof(s_status_line), "Extraction Completed! Modules ready for Safe-iTLS.");
    } else if (progress->status == PUP_EXTRACT_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Extraction error: %s", progress->error_message);
        psvDebugScreenPrintf("[-] %s\n", s_status_line);
    }
}

static void draw_partition_menu(void) {
    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("***************************************************\n");
    psvDebugScreenPrintf("  Safe-iTLS-Enso Firmware Installer (v1.2.3)\n");
    psvDebugScreenPrintf("  Official Sony Server Download & Hardware Decrypt\n");
    psvDebugScreenPrintf("***************************************************\n\n");
    psvDebugScreenPrintf(" Decryption:      Hardware Bigmac DMAC5 (Keyslot 0x20)\n");
    psvDebugScreenPrintf(" Software Keys:   NONE (0%% key exposure)\n");
    psvDebugScreenPrintf(" vs0: Safety:     100%% Untouched (Zero vs0: writes)\n\n");

    psvDebugScreenPrintf(" Which partition do you want to use?\n");
    psvDebugScreenPrintf("   %s [1] uma0:/data/itls  (SD2Vita / USB)        [%s]\n",
                         (s_choice == PARTITION_CHOICE_UMA0) ? ">>" : "  ",
                         s_avail.uma0_mounted ? "MOUNTED" : "NOT MOUNTED");
    psvDebugScreenPrintf("   %s [2] ux0:/data/itls   (Memory Card / ux0:)   [%s]\n\n",
                         (s_choice == PARTITION_CHOICE_UX0) ? ">>" : "  ",
                         s_avail.ux0_mounted ? "MOUNTED" : "NOT MOUNTED");

    psvDebugScreenPrintf(" Exact 5 Target Files to Extract:\n");
    psvDebugScreenPrintf("   1. vs0:sys/external/libssl.suprx\n");
    psvDebugScreenPrintf("   2. vs0:sys/external/libhttp.suprx\n");
    psvDebugScreenPrintf("   3. vs0:/data/external/webcore/ScePsp2Compat.suprx\n");
    psvDebugScreenPrintf("   4. vs0:sys/external/np_commerce2.suprx\n");
    psvDebugScreenPrintf("   5. vs0:data/external/cert/CA_LIST.cer\n\n");

    psvDebugScreenPrintf(" Controls:\n");
    psvDebugScreenPrintf("   [UP / DOWN] - Select Partition (uma0: or ux0:)\n");
    psvDebugScreenPrintf("   [X]         - Download from Sony & Extract to Selected Partition\n");
    psvDebugScreenPrintf("   [START]     - Exit to LiveArea\n\n");
    psvDebugScreenPrintf(" Status: %s\n", s_status_line);
}

static void run_installer(StoragePartitionChoice choice) {
    /* Refresh partition mount state right before installing */
    storage_probe_partitions(&s_avail);

    StorageTarget target;
    StorageMedium medium = (choice == PARTITION_CHOICE_UMA0) ? STORAGE_UMA0 : STORAGE_UX0;

    if (storage_configure_target(medium, &target) < 0) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "%s is NOT mounted! Choose the other partition.",
                 (choice == PARTITION_CHOICE_UMA0) ? "uma0:" : "ux0:");
        draw_partition_menu();
        return;
    }

    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("===================================================\n");
    psvDebugScreenPrintf(" Safe-iTLS-Enso Firmware Installer\n");
    psvDebugScreenPrintf(" Zero vs0: Modifications - Hardware Bigmac Decrypt\n");
    psvDebugScreenPrintf("===================================================\n\n");

    /* Step 1: Ensure chosen destination directory exists before streaming PUP */
    storage_make_dir_recursive(target.itls_dir);
    psvDebugScreenPrintf("[+] Selected Partition: %s (%s)\n\n",
                         target.itls_dir,
                         target.is_sd2vita ? "SD2Vita / USB" : "Memory Card / ux0:");

    snprintf(s_pup_path, sizeof(s_pup_path), "%s/PSVUPDAT.PUP", target.itls_dir);

    /* Step 2: Download Sony Official PUP */
    psvDebugScreenPrintf("[*] Downloading official Sony update PUP package...\n");
    psvDebugScreenPrintf("    Server: deu01.psv.update.playstation.net\n");
    s_state = INSTALLER_STATE_DOWNLOADING;

    int dl_res = downloader_fetch_pup(OFFICIAL_PUP_URL, s_pup_path, on_download_progress, NULL);
    if (dl_res < 0) {
        psvDebugScreenPrintf("[-] Download failed! Please check your network connection.\n");
        psvDebugScreenPrintf("Press [TRIANGLE] for Partition Menu, or [START]/[O] to Exit.\n");
        s_state = INSTALLER_STATE_ERROR;
        return;
    }
    psvDebugScreenPrintf("[+] Firmware PUP downloaded successfully.\n\n");

    /* Step 3: Hardware-Assisted Extraction (NO SOFTWARE AES KEYS) */
    psvDebugScreenPrintf("[*] Initializing PS Vita Bigmac DMAC5 Hardware Engine...\n");
    psvDebugScreenPrintf("[*] Using Hardware Keyslot 0x20 (Zero software AES keys)...\n");
    psvDebugScreenPrintf("[*] Extracting ONLY the 5 required system files:\n");
    psvDebugScreenPrintf("    1. vs0:sys/external/libssl.suprx\n");
    psvDebugScreenPrintf("    2. vs0:sys/external/libhttp.suprx\n");
    psvDebugScreenPrintf("    3. vs0:/data/external/webcore/ScePsp2Compat.suprx\n");
    psvDebugScreenPrintf("    4. vs0:sys/external/np_commerce2.suprx\n");
    psvDebugScreenPrintf("    5. vs0:data/external/cert/CA_LIST.cer\n\n");

    s_state = INSTALLER_STATE_EXTRACTING_HW;

    int ext_res = pup_extract_target_modules(s_pup_path, target.itls_dir, on_extract_progress, NULL);
    if (ext_res < 0) {
        psvDebugScreenPrintf("[-] Extraction failed.\n");
        psvDebugScreenPrintf("Press [TRIANGLE] for Partition Menu, or [START]/[O] to Exit.\n");
        s_state = INSTALLER_STATE_ERROR;
        return;
    }

    psvDebugScreenPrintf("\n[+] Hardware extraction complete!\n");
    psvDebugScreenPrintf("[+] Temporary PUP file deleted to free storage space.\n\n");
    psvDebugScreenPrintf("---------------------------------------------------\n");
    psvDebugScreenPrintf(" All 5 modules installed in: %s\n", target.itls_dir);
    psvDebugScreenPrintf(" - libssl.suprx          (Modern TLS 1.2 stack)\n");
    psvDebugScreenPrintf(" - libhttp.suprx         (Modern HTTP client)\n");
    psvDebugScreenPrintf(" - ScePsp2Compat.suprx   (WebCore TLS compatibility)\n");
    psvDebugScreenPrintf(" - np_commerce2.suprx    (PS Store TLS commerce)\n");
    psvDebugScreenPrintf(" - CA_LIST.cer           (Root certificate store)\n");
    psvDebugScreenPrintf("---------------------------------------------------\n\n");
    psvDebugScreenPrintf("Next step: Ensure ur0:tai/config.txt contains:\n");
    psvDebugScreenPrintf("  *KERNEL\n");
    psvDebugScreenPrintf("  ur0:tai/itls.skprx\n\n");
    psvDebugScreenPrintf("Press [TRIANGLE] to pick another partition, or [START]/[O] to Exit.\n");

    s_state = INSTALLER_STATE_SUCCESS;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Boost clocks during download and hardware decryption */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);

    psvDebugScreenInit();
    storage_probe_partitions(&s_avail);

    /* Pre-highlight uma0: if mounted, otherwise ux0: */
    s_choice = s_avail.uma0_mounted ? PARTITION_CHOICE_UMA0 : PARTITION_CHOICE_UX0;

    SceCtrlData pad, old_pad;
    memset(&pad, 0, sizeof(pad));
    memset(&old_pad, 0, sizeof(old_pad));

    draw_partition_menu();

    bool running = true;
    while (running) {
        old_pad = pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~old_pad.buttons;

        if (s_state == INSTALLER_STATE_SELECT_PARTITION) {
            if (pressed & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER)) {
                s_choice = (s_choice == PARTITION_CHOICE_UMA0)
                    ? PARTITION_CHOICE_UX0
                    : PARTITION_CHOICE_UMA0;
                snprintf(s_status_line, sizeof(s_status_line), "Selected: %s", get_choice_label(s_choice));
                draw_partition_menu();
            } else if (pressed & SCE_CTRL_CROSS) {
                run_installer(s_choice);
            } else if (pressed & SCE_CTRL_START) {
                running = false;
            }
        } else if (s_state == INSTALLER_STATE_SUCCESS || s_state == INSTALLER_STATE_ERROR) {
            if (pressed & SCE_CTRL_TRIANGLE) {
                storage_probe_partitions(&s_avail);
                snprintf(s_status_line, sizeof(s_status_line),
                         "Use UP/DOWN to choose uma0: or ux0:, then press (X) to Install.");
                s_state = INSTALLER_STATE_SELECT_PARTITION;
                draw_partition_menu();
            } else if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_START)) {
                running = false;
            }
        }

        sceKernelDelayThread(30000);
    }

    downloader_term();
    hw_crypto_term();
    psvDebugScreenTerm();
    sceKernelExitProcess(0);
    return 0;
}
