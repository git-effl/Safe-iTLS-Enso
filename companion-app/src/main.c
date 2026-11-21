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
static char s_status_line[128] = "Select a partition and press (X) to install.";

static const char *get_choice_label(StoragePartitionChoice choice) {
    return (choice == PARTITION_CHOICE_UMA0)
        ? "uma0:/data/itls"
        : "ux0:/data/itls";
}

static void on_download_progress(const DownloadProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == DOWNLOAD_STATUS_DOWNLOADING) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "Downloading: %.1f%% (%llu MB)",
                 progress->progress_percent,
                 (unsigned long long)(progress->downloaded_bytes / (1024 * 1024)));
        psvDebugScreenPrintf("\r  %s   ", s_status_line);
    } else if (progress->status == DOWNLOAD_STATUS_CONNECTING) {
        psvDebugScreenPrintf("  Connecting to update server...\n");
    } else if (progress->status == DOWNLOAD_STATUS_COMPLETED) {
        psvDebugScreenPrintf("\r  Downloading: 100.0%%              \n");
    } else if (progress->status == DOWNLOAD_STATUS_ERROR) {
        snprintf(s_status_line, sizeof(s_status_line), "Download error: %s", progress->error_message);
        psvDebugScreenPrintf("\n  Error: %s\n", progress->error_message);
    }
}

static void on_extract_progress(const PupExtractProgress *progress, void *userdata) {
    (void)userdata;
    if (progress->status == PUP_EXTRACT_HARDWARE_DECRYPT) {
        psvDebugScreenPrintf("  [%u/%u] %s\n",
                             (unsigned int)progress->current_target_index,
                             (unsigned int)progress->total_targets,
                             progress->current_target_name);
    } else if (progress->status == PUP_EXTRACT_ERROR) {
        psvDebugScreenPrintf("  Error: %s\n", progress->error_message);
    }
}

static void draw_partition_menu(void) {
    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("Safe-iTLS Installer (v1.3.0)\n");
    psvDebugScreenPrintf("----------------------------\n\n");

    psvDebugScreenPrintf("Where do you want to install iTLS files?\n\n");
    psvDebugScreenPrintf("  %s uma0:/data/itls  (SD2Vita / USB)   [%s]\n",
                         (s_choice == PARTITION_CHOICE_UMA0) ? ">" : " ",
                         s_avail.uma0_mounted ? "READY" : "NOT FOUND");
    psvDebugScreenPrintf("  %s ux0:/data/itls   (Memory Card)     [%s]\n\n",
                         (s_choice == PARTITION_CHOICE_UX0) ? ">" : " ",
                         s_avail.ux0_mounted ? "READY" : "NOT FOUND");

    psvDebugScreenPrintf("  [UP/DOWN] Choose Partition\n");
    psvDebugScreenPrintf("  [X]       Install\n");
    psvDebugScreenPrintf("  [START]   Exit\n\n");
    psvDebugScreenPrintf("  %s\n", s_status_line);
}

static void run_installer(StoragePartitionChoice choice) {
    storage_probe_partitions(&s_avail);

    StorageTarget target;
    StorageMedium medium = (choice == PARTITION_CHOICE_UMA0) ? STORAGE_UMA0 : STORAGE_UX0;

    if (storage_configure_target(medium, &target) < 0) {
        snprintf(s_status_line, sizeof(s_status_line),
                 "%s is not available. Pick another partition.",
                 (choice == PARTITION_CHOICE_UMA0) ? "uma0:" : "ux0:");
        draw_partition_menu();
        return;
    }

    psvDebugScreenClear(0x000000);
    psvDebugScreenPrintf("Safe-iTLS Installer\n");
    psvDebugScreenPrintf("-------------------\n\n");
    psvDebugScreenPrintf("  Destination: %s\n\n", target.itls_dir);

    storage_make_dir_recursive(target.itls_dir);
    snprintf(s_pup_path, sizeof(s_pup_path), "%s/PSVUPDAT.PUP", target.itls_dir);

    s_state = INSTALLER_STATE_DOWNLOADING;
    if (downloader_fetch_pup(OFFICIAL_PUP_URL, s_pup_path, on_download_progress, NULL) < 0) {
        psvDebugScreenPrintf("\n  Download failed. Check your Wi-Fi connection.\n\n");
        psvDebugScreenPrintf("  [TRIANGLE] Back    [START/O] Exit\n");
        s_state = INSTALLER_STATE_ERROR;
        return;
    }

    psvDebugScreenPrintf("\n  Extracting TLS files...\n");
    s_state = INSTALLER_STATE_EXTRACTING_HW;

    if (pup_extract_target_modules(s_pup_path, target.itls_dir, on_extract_progress, NULL) < 0) {
        psvDebugScreenPrintf("\n  Installation failed.\n\n");
        psvDebugScreenPrintf("  [TRIANGLE] Back    [START/O] Exit\n");
        s_state = INSTALLER_STATE_ERROR;
        return;
    }

    psvDebugScreenPrintf("\n  Done! Files installed to %s\n", target.itls_dir);
    psvDebugScreenPrintf("  Enable ur0:tai/itls.skprx and reboot your Vita.\n\n");
    psvDebugScreenPrintf("  [TRIANGLE] Back    [START/O] Exit\n");

    s_state = INSTALLER_STATE_SUCCESS;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);

    psvDebugScreenInit();
    storage_probe_partitions(&s_avail);

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
                         "Select a partition and press (X) to install.");
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
