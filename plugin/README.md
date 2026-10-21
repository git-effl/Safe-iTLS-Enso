# Safe-iTLS-Enso Kernel Plugin (`itls.skprx`)

A taiHEN kernel plugin that intercepts PlayStation Vita TLS and HTTP module loading and dynamically redirects calls from outdated `vs0:` system files to modern modules extracted on your storage card (`uma0:/data/itls` or `ux0:/data/itls`).

---

## Strict 5-Target Redirection Filter

The kernel plugin strictly intercepts and redirects ONLY these 5 system targets:

1. `vs0:sys/external/libssl.suprx` -> `<card>/data/itls/libssl.suprx`
2. `vs0:sys/external/libhttp.suprx` -> `<card>/data/itls/libhttp.suprx`
3. `vs0:/data/external/webcore/ScePsp2Compat.suprx` -> `<card>/data/itls/ScePsp2Compat.suprx`
4. `vs0:sys/external/np_commerce2.suprx` -> `<card>/data/itls/np_commerce2.suprx`
5. `vs0:data/external/cert/CA_LIST.cer` -> `<card>/data/itls/CA_LIST.cer`

**Zero other files are affected.**

---

## Zero `vs0:` Modifications Guarantee

- **No OS partition writes:** `vs0:` remains 100% factory read-only.
- **Safety Fallback:** If the SD card is removed or unmounted, the plugin automatically falls back to standard system files without crashing or bootlooping.

---

## Installation

1. Copy `itls.skprx` to `ur0:tai/itls.skprx`.
2. Add the path to `ur0:tai/config.txt` under `*KERNEL`:
   ```ini
   *KERNEL
   ur0:tai/itls.skprx
   ```
3. Run **Safe-iTLS Companion App** to download and hardware-decrypt the 5 modules.
4. Reboot the PS Vita.
