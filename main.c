#include <taihen.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/iofilemgr.h>
#include <string.h>
#include <stdio.h>

// Hook IDs
static SceUID ioOpenHookID = -1;
static SceUID ioOpenForProcHookID = -1;
static SceUID kernelLoadModuleHookID = -1;
static SceUID kernelStartModuleHookID = -1;

// Hook references
static tai_hook_ref_t ioOpenHookRef;
static tai_hook_ref_t ioOpenForProcHookRef;
static tai_hook_ref_t kernelLoadModuleHookRef;
static tai_hook_ref_t kernelStartModuleHookRef;

// Function pointer typedefs
typedef SceUID (*ioOpenFuncPtr)(const char *file, int flags, SceMode mode);
typedef SceUID (*ioOpenForProcFuncPtr)(SceUID pid, const char *file, int flags, SceMode mode);
typedef SceUID (*kernelLoadModuleFuncPtr)(const char *path, int flags, SceKernelLMOption *option);
typedef int (*kernelStartModuleFuncPtr)(SceUID modid, SceSize args, void *argp, int priority, SceKernelModuleInfo *info);

// Path redirection helper
static const char* redirectPath(const char *file) {
    if (file == NULL) return NULL;

    if (strstr(file, "vs0:sys/external/libhttp.suprx")) {
        return "ux0:data/itls/libhttp.suprx";
    } else if (strstr(file, "vs0:sys/external/libssl.suprx")) {
        return "ux0:data/itls/libssl.suprx";
    } else if (strstr(file, "vs0:data/external/webcore/ScePsp2Compat.suprx")) {
        return "ux0:data/itls/ScePsp2Compat.suprx";
    } else if (strstr(file, "vs0:sys/external/np_commerce2.suprx")) {
        return "ux0:data/itls/np_commerce2.suprx";
    } else if (strstr(file, "vs0:data/external/cert/CA_LIST.cer")) {
        return "ux0:data/itls/CA_LIST.cer";
    }
    
    return file;
}

// sceIoOpen hook
static SceUID ioOpenPatched(const char *file, int flags, SceMode mode) {
    const char *redirected = redirectPath(file);
    ioOpenFuncPtr orig_func = (ioOpenFuncPtr)ioOpenHookRef;
    return orig_func(redirected, flags, mode);
}

// sceIoOpenForProc hook (for multi-process file access)
static SceUID ioOpenForProcPatched(SceUID pid, const char *file, int flags, SceMode mode) {
    const char *redirected = redirectPath(file);
    ioOpenForProcFuncPtr orig_func = (ioOpenForProcFuncPtr)ioOpenForProcHookRef;
    return orig_func(pid, redirected, flags, mode);
}

// sceKernelLoadModule hook (intercept module loads)
static SceUID kernelLoadModulePatched(const char *path, int flags, SceKernelLMOption *option) {
    const char *redirected = redirectPath(path);
    kernelLoadModuleFuncPtr orig_func = (kernelLoadModuleFuncPtr)kernelLoadModuleHookRef;
    return orig_func(redirected, flags, option);
}

// sceKernelStartModule hook (for completeness)
static int kernelStartModulePatched(SceUID modid, SceSize args, void *argp, int priority, SceKernelModuleInfo *info) {
    kernelStartModuleFuncPtr orig_func = (kernelStartModuleFuncPtr)kernelStartModuleHookRef;
    return orig_func(modid, args, argp, priority, info);
}

int _start(SceSize args, void *argp) {
    // Hook sceIoOpen (0xCAE9ACE6, 0x6C2224A7)
    ioOpenHookID = taiHookFunctionImport(&ioOpenHookRef, TAI_MAIN_MODULE, 0xCAE9ACE6, 0x6C2224A7, ioOpenPatched);
    
    // Hook sceIoOpenForProc (0xCAE9ACE6, 0xA71930C0)
    ioOpenForProcHookID = taiHookFunctionImport(&ioOpenForProcHookRef, TAI_MAIN_MODULE, 0xCAE9ACE6, 0xA71930C0, ioOpenForProcPatched);
    
    // Hook sceKernelLoadModule (0x3AD58B8C, 0x2DCC759C)
    kernelLoadModuleHookID = taiHookFunctionImport(&kernelLoadModuleHookRef, TAI_MAIN_MODULE, 0x3AD58B8C, 0x2DCC759C, kernelLoadModulePatched);
    
    // Hook sceKernelStartModule (0x3AD58B8C, 0xD3434235) - optional but useful
    kernelStartModuleHookID = taiHookFunctionImport(&kernelStartModuleHookRef, TAI_MAIN_MODULE, 0x3AD58B8C, 0xD3434235, kernelStartModulePatched);
    
    return SCE_KERNEL_START_SUCCESS;
}

int module_start(SceSize args, void *argp) __attribute__((alias("_start")));

int module_stop(SceSize args, void *argp) {
    if (ioOpenHookID >= 0)
        taiHookRelease(ioOpenHookID, ioOpenHookRef);
    if (ioOpenForProcHookID >= 0)
        taiHookRelease(ioOpenForProcHookID, ioOpenForProcHookRef);
    if (kernelLoadModuleHookID >= 0)
        taiHookRelease(kernelLoadModuleHookID, kernelLoadModuleHookRef);
    if (kernelStartModuleHookID >= 0)
        taiHookRelease(kernelStartModuleHookID, kernelStartModuleHookRef);
    
    return SCE_KERNEL_STOP_SUCCESS;
}
