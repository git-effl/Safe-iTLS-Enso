# Safe-iTLS-Enso Kernel Plugin (`itls.skprx`)

A taiHEN kernel plugin that intercepts PlayStation Vita TLS and HTTP module loading and dynamically redirects calls from outdated `vs0:` system files to modern modules extracted on your storage card (`uma0:/data/itls` or `ux0:/data/itls`).

---

## Why "Safe" iTLS-Enso?

Traditional iTLS installations permanently replaced `libssl.suprx` and `libhttp.suprx` on `vs0:` (the PlayStation Vita's protected OS partition). If `vs0:` was corrupted, the console could fail to boot or require a factory reflash.

**Safe-iTLS-Enso makes ZERO modifications to `vs0:`.**
- All system partitions remain 100% factory original and read-only.
- The kernel plugin dynamically hooks module loading in memory.
- If the SD card is removed or unmounted, the plugin safely falls back to standard system libraries without crashing.

---

## How it Works

1. **Storage Probe:**
   - On boot, the plugin checks for extracted firmware components:
     1. Priority: `uma0:data/itls/` (SD2Vita / USB adapter)
     2. Fallback: `ux0:data/itls/` (Official Sony Memory Card)
2. **taiHEN Kernel Hooks:**
   - `SceKernelModulemgr` (`sceKernelLoadStartModuleForPid`): When an application or system process loads `vs0:sys/external/libssl.suprx` or `vs0:sys/external/libhttp.suprx`, the path is transparently rewritten to the SD card.
   - `SceIofilemgr` (`ksceIoOpen`): File opens for system root certificates (`vs0:data/external/cert/`) are redirected to `ca-bundle.pem` on the SD card.
3. **Safety Fallback:**
   - If the target file on the SD card is missing, truncated, or unreadable, the hook passes through to the original `vs0:` file.

---

## Installation

1. Copy `itls.skprx` to `ur0:tai/itls.skprx`.
2. Add the path to `ur0:tai/config.txt` under `*KERNEL`:
   ```ini
   *KERNEL
   ur0:tai/itls.skprx
   ```
3. Ensure the extracted modules are present in `uma0:/data/itls` or `ux0:/data/itls` (use the **Safe-iTLS Companion App** to download and extract them using Vita hardware acceleration).
4. Reboot the PS Vita.
