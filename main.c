#include <taihen.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/io/fcntl.h>
#include <string.h>

static SceUID ioOpenHookID = -1;
static tai_hook_ref_t ioOpenHookRef;

// Explicit function pointer typedef for clean direct casting
typedef SceUID (*ioOpenFuncPtr)(const char *file, int flags, SceMode mode);

static SceUID ioOpenPatched(const char *file, int flags, SceMode mode) {
    if (file != NULL) {
        if (strstr(file, "vs0:sys/external/libhttp.suprx")) {
            file = "uma0:itls/libhttp.suprx";
        } else if (strstr(file, "vs0:sys/external/libssl.suprx")) {
            file = "uma0:itls/libssl.suprx";
        } else if (strstr(file, "vs0:data/external/webcore/ScePsp2Compat.suprx")) {
            file = "uma0:itls/ScePsp2Compat.suprx";
        } else if (strstr(file, "vs0:sys/external/np_commerce2.suprx")) {
            file = "uma0:itls/np_commerce2.suprx";
        } else if (strstr(file, "vs0:data/external/cert/CA_LIST.cer")) {
            file = "uma0:itls/CA_LIST.cer";
        }
    }

    // Cast the taiHEN hook reference directly to your target function pointer
    ioOpenFuncPtr orig_func = (ioOpenFuncPtr)ioOpenHookRef;
    return orig_func(file, flags, mode);
}

int _start(SceSize args, void *argp) {
    ioOpenHookID = taiHookFunctionImport(&ioOpenHookRef, TAI_MAIN_MODULE, 0xCAE9ACE6, 0x6C2224A7, ioOpenPatched);
    return SCE_KERNEL_START_SUCCESS;
}

int module_start(SceSize args, void *argp) __attribute__((alias("_start")));

int module_stop(SceSize args, void *argp) {
    if (ioOpenHookID >= 0)
        taiHookRelease(ioOpenHookID, ioOpenHookRef);
    return SCE_KERNEL_STOP_SUCCESS;
}
