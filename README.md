# Safe-iTLS-Enso

A brick-safe PlayStation Vita homebrew tool that redirects outdated system TLS 1.0/1.1 calls to modern TLS 1.2/1.3 modules located on the storage card (`uma0:` / `ux0:`), making zero modifications to the console's read-only `vs0:` OS partition.

---

## Architecture Overview

```
                      +---------------------------------------+
                      |  PlayStation Vita App / Game / Store   |
                      +---------------------------------------+
                                          |
                   Requests: vs0:sys/external/libssl.suprx
                                          |
                                          v
                      +---------------------------------------+
                      |   Safe-iTLS Kernel Plugin (itls.skprx) |
                      |    (taiHEN Hook in SceKernelModulemgr)|
                      +---------------------------------------+
                                          |
                                Rewrites path to:
                      uma0:/data/itls/libssl.suprx (SD2Vita)
                         or ux0:/data/itls/libssl.suprx
                                          |
                                          v
                      +---------------------------------------+
                      |  Modern TLS 1.2/1.3 & Mozilla CA Store |
                      |   (Extracted via Companion App)       |
                      +---------------------------------------+
```

---

## Repository Structure

- [`plugin/`](./plugin/): The taiHEN kernel plugin source (`itls.skprx`). Hooks `SceKernelModulemgr` (`sceKernelLoadStartModuleForPid`) and `SceIofilemgr` (`ksceIoOpen`) to redirect calls to the card.
- [`companion-app/`](./companion-app/): PS Vita homebrew application (`Safe-iTLS-Companion.vpk`) that downloads the official firmware PUP from PlayStation CDN and extracts `libssl.suprx`, `libhttp.suprx`, and root CAs using the PS Vita's on-chip **Bigmac DMA Hardware Crypto Accelerator** (Keyslot `0x20`).

---

## Quick Start & Installation

### 1. Extract Modules with Companion App
1. Install and launch `Safe-iTLS-Companion.vpk` on your PS Vita.
2. Press **(X)** to automatically download the official firmware and extract the required modules directly to `uma0:/data/itls` (SD2Vita) or `ux0:/data/itls` (Memory Card).

### 2. Enable Kernel Redirection Plugin
1. Copy `itls.skprx` to `ur0:tai/itls.skprx`.
2. Edit `ur0:tai/config.txt` and add the plugin under `*KERNEL`:
   ```ini
   *KERNEL
   ur0:tai/itls.skprx
   ```
3. Reboot your PS Vita.

Now all applications, browser, PKGj, and PSN services will securely negotiate TLS 1.2/1.3 without touching `vs0:`.
