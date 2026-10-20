#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/clib.h>
#include <psp2kern/io/fcntl.h>
#include <taihen.h>
#include "redirect.h"

#define PLUGIN_NAME "Safe-iTLS-Enso Kernel Plugin"
#define PLUGIN_VERSION "1.0.0"

static SceUID s_hook_load_module = -1;
static SceUID s_hook_io_open = -1;
static tai_hook_ref_t s_ref_load_module;
static tai_hook_ref_t s_ref_io_open;

/**
 * @brief Hooked sceKernelLoadStartModuleForPid
 * Intercepts userland and system requests to load obsolete vs0: SSL/HTTP modules
 * and dynamically rewrites them to point to the modern modules on the SD card (uma0:/ux0:).
 */
static SceUID sceKernelLoadStartModuleForPid_patched(SceUID pid, const char *path, SceSize args, void *argp, int flags, SceKernelLMOption *option, int *status) {
    char redirected_path[MAX_PATH_LEN];

    if (path && redirect_resolve_path(path, redirected_path, sizeof(redirected_path))) {
        ksceDebugPrintf("[Safe-iTLS] Redirecting module load: %s -> %s (pid: 0x%08X)\n",
                        path, redirected_path, pid);
        return TAI_CONTINUE(SceUID, s_ref_load_module, pid, redirected_path, args, argp, flags, option, status);
    }

    return TAI_CONTINUE(SceUID, s_ref_load_module, pid, path, args, argp, flags, option, status);
}

/**
 * @brief Hooked ksceIoOpen
 * Intercepts low-level file open calls targeting vs0: certificates and modules.
 */
static SceUID ksceIoOpen_patched(const char *file, int flags, SceMode mode) {
    char redirected_file[MAX_PATH_LEN];

    if (file && redirect_resolve_path(file, redirected_file, sizeof(redirected_file))) {
        ksceDebugPrintf("[Safe-iTLS] Redirecting I/O open: %s -> %s\n", file, redirected_file);
        return TAI_CONTINUE(SceUID, s_ref_io_open, redirected_file, flags, mode);
    }

    return TAI_CONTINUE(SceUID, s_ref_io_open, file, flags, mode);
}

void _start() __attribute__ ((weak, alias ("module_start")));

int module_start(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    ksceDebugPrintf("===================================================\n");
    ksceDebugPrintf(" %s (v%s)\n", PLUGIN_NAME, PLUGIN_VERSION);
    ksceDebugPrintf(" Safe TLS Redirection to Storage Media (Zero vs0: writes)\n");
    ksceDebugPrintf("===================================================\n");

    /* Initialize storage detection on SD card (uma0: -> ux0:) */
    redirect_init();

    RedirectStatus status;
    redirect_get_status(&status);

    if (status.active_target == REDIRECT_TARGET_NONE) {
        ksceDebugPrintf("[Safe-iTLS] Warning: No extracted modules found on uma0: or ux0:!\n");
        ksceDebugPrintf("[Safe-iTLS] Please run Safe-iTLS Companion App to extract firmware.\n");
    } else {
        ksceDebugPrintf("[Safe-iTLS] Active redirection target: %s\n", status.base_dir);
        ksceDebugPrintf("[Safe-iTLS] - libssl.suprx: %s\n", status.libssl_available ? "YES" : "NO");
        ksceDebugPrintf("[Safe-iTLS] - libhttp.suprx: %s\n", status.libhttp_available ? "YES" : "NO");
        ksceDebugPrintf("[Safe-iTLS] - ca-bundle.pem: %s\n", status.cert_available ? "YES" : "NO");
    }

    /* Hook SceKernelModulemgr (sceKernelLoadStartModuleForPid) */
    s_hook_load_module = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_load_module,
        "SceKernelModulemgr",
        TAI_ANY_LIBRARY,
        0x0405A51F, /* sceKernelLoadStartModuleForPid NID */
        sceKernelLoadStartModuleForPid_patched
    );

    /* Hook SceIofilemgr (ksceIoOpen) */
    s_hook_io_open = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_io_open,
        "SceIofilemgr",
        TAI_ANY_LIBRARY,
        0xC3D34965, /* ksceIoOpen NID */
        ksceIoOpen_patched
    );

    ksceDebugPrintf("[Safe-iTLS] Hooks installed: ModuleMgr=0x%08X, IoMgr=0x%08X\n",
                    s_hook_load_module, s_hook_io_open);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc;
    (void)args;

    if (s_hook_load_module >= 0) {
        taiHookReleaseForKernel(s_hook_load_module, s_ref_load_module);
        s_hook_load_module = -1;
    }

    if (s_hook_io_open >= 0) {
        taiHookReleaseForKernel(s_hook_io_open, s_ref_io_open);
        s_hook_io_open = -1;
    }

    ksceDebugPrintf("[Safe-iTLS] Kernel plugin unloaded.\n");
    return SCE_KERNEL_STOP_SUCCESS;
}
