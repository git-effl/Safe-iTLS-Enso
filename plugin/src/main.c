#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/io/fcntl.h>
#include <taihen.h>
#include "redirect.h"

#define PLUGIN_NAME    "Safe-iTLS-Enso Kernel Plugin"
#define PLUGIN_VERSION "1.3.0"

/* Verified VitaSDK kernel library & function NIDs (FW 3.60 / 3.63-3.74) */
#define NID_LIB_MODULEMGR_DRIVER      0xD4A60A52
#define NID_LIB_MODULEMGR_KERNEL_360  0xC445FA63
#define NID_LIB_MODULEMGR_KERNEL_363  0x92C9FFC2
#define NID_LIB_IOFILEMGR_DRIVER      0x40FD29C7

#define NID_LOAD_START_MOD_FOR_PID         0x9D953C22
#define NID_LOAD_START_SHARED_MOD_FOR_PID  0xE2ADEF8D
#define NID_LOAD_MOD_FOR_PID_360           0xFA21D8CB
#define NID_LOAD_MOD_FOR_PID_363           0x4E85022D
#define NID_IO_OPEN                        0x75192972
#define NID_IO_OPEN_FOR_PID                0xC3D34965

static SceUID s_hook_load_start_mod = -1;
static SceUID s_hook_load_start_shared_mod = -1;
static SceUID s_hook_load_mod = -1;
static SceUID s_hook_io_open = -1;
static SceUID s_hook_io_open_for_pid = -1;

static tai_hook_ref_t s_ref_load_start_mod;
static tai_hook_ref_t s_ref_load_start_shared_mod;
static tai_hook_ref_t s_ref_load_mod;
static tai_hook_ref_t s_ref_io_open;
static tai_hook_ref_t s_ref_io_open_for_pid;

/**
 * @brief Hooked ksceKernelLoadStartModuleForPid
 * Intercepts module load/start requests and strictly redirects ONLY the 5 allowed target files.
 */
static SceUID ksceKernelLoadStartModuleForPid_patched(
    SceUID pid, const char *path, SceSize args, void *argp,
    int flags, SceKernelLMOption *option, int *status
) {
    char redirected_path[MAX_PATH_LEN];

    if (path && redirect_resolve_path(path, redirected_path, sizeof(redirected_path))) {
        return TAI_CONTINUE(SceUID, s_ref_load_start_mod,
                            pid, redirected_path, args, argp, flags, option, status);
    }

    return TAI_CONTINUE(SceUID, s_ref_load_start_mod,
                        pid, path, args, argp, flags, option, status);
}

/**
 * @brief Hooked ksceKernelLoadStartSharedModuleForPid
 * Intercepts shared module load/start requests for the exact 5 allowed targets.
 */
static SceUID ksceKernelLoadStartSharedModuleForPid_patched(
    SceUID pid, const char *path, SceSize args, void *argp,
    int flags, SceKernelLMOption *option, int *status
) {
    char redirected_path[MAX_PATH_LEN];

    if (path && redirect_resolve_path(path, redirected_path, sizeof(redirected_path))) {
        return TAI_CONTINUE(SceUID, s_ref_load_start_shared_mod,
                            pid, redirected_path, args, argp, flags, option, status);
    }

    return TAI_CONTINUE(SceUID, s_ref_load_start_shared_mod,
                        pid, path, args, argp, flags, option, status);
}

/**
 * @brief Hooked ksceKernelLoadModuleForPid
 * Intercepts deferred module loads for the exact 5 allowed targets.
 */
static SceUID ksceKernelLoadModuleForPid_patched(
    SceUID pid, const char *path, int flags, SceKernelLMOption *option
) {
    char redirected_path[MAX_PATH_LEN];

    if (path && redirect_resolve_path(path, redirected_path, sizeof(redirected_path))) {
        return TAI_CONTINUE(SceUID, s_ref_load_mod,
                            pid, redirected_path, flags, option);
    }

    return TAI_CONTINUE(SceUID, s_ref_load_mod, pid, path, flags, option);
}

/**
 * @brief Hooked ksceIoOpen
 * Intercepts kernel file opens and strictly redirects ONLY the 5 allowed target files
 * (including CA_LIST.cer root certificate bundle).
 */
static SceUID ksceIoOpen_patched(const char *file, int flags, SceMode mode) {
    char redirected_file[MAX_PATH_LEN];

    if (file && redirect_resolve_path(file, redirected_file, sizeof(redirected_file))) {
        return TAI_CONTINUE(SceUID, s_ref_io_open, redirected_file, flags, mode);
    }

    return TAI_CONTINUE(SceUID, s_ref_io_open, file, flags, mode);
}

/**
 * @brief Hooked ksceIoOpenForPid
 * Intercepts per-process file opens (4-arg signature) and strictly redirects ONLY the 5 targets.
 */
static SceUID ksceIoOpenForPid_patched(SceUID pid, const char *file, int flags, SceMode mode) {
    char redirected_file[MAX_PATH_LEN];

    if (file && redirect_resolve_path(file, redirected_file, sizeof(redirected_file))) {
        return TAI_CONTINUE(SceUID, s_ref_io_open_for_pid, pid, redirected_file, flags, mode);
    }

    return TAI_CONTINUE(SceUID, s_ref_io_open_for_pid, pid, file, flags, mode);
}

void _start() __attribute__((weak, alias("module_start")));

int module_start(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    /* Initial storage card probe (uma0: -> ux0:; lazily re-probed if mounted later) */
    redirect_init();

    /* Hook SceKernelModulemgr -> SceModulemgrForDriver exports */
    s_hook_load_start_mod = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_load_start_mod,
        "SceKernelModulemgr",
        NID_LIB_MODULEMGR_DRIVER,
        NID_LOAD_START_MOD_FOR_PID,
        ksceKernelLoadStartModuleForPid_patched
    );

    s_hook_load_start_shared_mod = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_load_start_shared_mod,
        "SceKernelModulemgr",
        NID_LIB_MODULEMGR_DRIVER,
        NID_LOAD_START_SHARED_MOD_FOR_PID,
        ksceKernelLoadStartSharedModuleForPid_patched
    );

    /* Hook SceKernelModulemgr -> SceModulemgrForKernel (FW 3.60, fallback to FW 3.63-3.74) */
    s_hook_load_mod = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_load_mod,
        "SceKernelModulemgr",
        NID_LIB_MODULEMGR_KERNEL_360,
        NID_LOAD_MOD_FOR_PID_360,
        ksceKernelLoadModuleForPid_patched
    );
    if (s_hook_load_mod < 0) {
        s_hook_load_mod = taiHookFunctionExportForKernel(
            KERNEL_PID,
            &s_ref_load_mod,
            "SceKernelModulemgr",
            NID_LIB_MODULEMGR_KERNEL_363,
            NID_LOAD_MOD_FOR_PID_363,
            ksceKernelLoadModuleForPid_patched
        );
    }

    /* Hook SceIofilemgr -> SceIofilemgrForDriver exports */
    s_hook_io_open = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_io_open,
        "SceIofilemgr",
        NID_LIB_IOFILEMGR_DRIVER,
        NID_IO_OPEN,
        ksceIoOpen_patched
    );

    s_hook_io_open_for_pid = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &s_ref_io_open_for_pid,
        "SceIofilemgr",
        NID_LIB_IOFILEMGR_DRIVER,
        NID_IO_OPEN_FOR_PID,
        ksceIoOpenForPid_patched
    );

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    if (s_hook_load_start_mod >= 0) {
        taiHookReleaseForKernel(s_hook_load_start_mod, s_ref_load_start_mod);
        s_hook_load_start_mod = -1;
    }

    if (s_hook_load_start_shared_mod >= 0) {
        taiHookReleaseForKernel(s_hook_load_start_shared_mod, s_ref_load_start_shared_mod);
        s_hook_load_start_shared_mod = -1;
    }

    if (s_hook_load_mod >= 0) {
        taiHookReleaseForKernel(s_hook_load_mod, s_ref_load_mod);
        s_hook_load_mod = -1;
    }

    if (s_hook_io_open >= 0) {
        taiHookReleaseForKernel(s_hook_io_open, s_ref_io_open);
        s_hook_io_open = -1;
    }

    if (s_hook_io_open_for_pid >= 0) {
        taiHookReleaseForKernel(s_hook_io_open_for_pid, s_ref_io_open_for_pid);
        s_hook_io_open_for_pid = -1;
    }

    return SCE_KERNEL_STOP_SUCCESS;
}
