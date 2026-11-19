# Safe-iTLS-Enso

A brick-safe PlayStation Vita homebrew suite that redirects outdated system TLS 1.0/1.1 calls to modern TLS 1.2/1.3 modules located on the storage card (`uma0:` / `ux0:`), making **zero modifications** to the console's read-only `vs0:` OS partition.

---

## The 5 Redirected Targets

Both the Companion Extractor and the Kernel Plugin strictly target ONLY these 5 files:

| # | System Path on `vs0:` | Storage Card Target (`uma0:` / `ux0:`) | Purpose |
|---|-----------------------|----------------------------------------|---------|
| 1 | `vs0:sys/external/libssl.suprx` | `data/itls/libssl.suprx` | Modern TLS 1.2 stack |
| 2 | `vs0:sys/external/libhttp.suprx` | `data/itls/libhttp.suprx` | HTTP client with SNI support |
| 3 | `vs0:/data/external/webcore/ScePsp2Compat.suprx` | `data/itls/ScePsp2Compat.suprx` | WebCore browser engine compatibility |
| 4 | `vs0:sys/external/np_commerce2.suprx` | `data/itls/np_commerce2.suprx` | PlayStation Store commerce authentication |
| 5 | `vs0:data/external/cert/CA_LIST.cer` | `data/itls/CA_LIST.cer` | Official root certificate authority bundle |

**No other system files are touched or redirected.**

---

## Hardware Decryption (Zero Software AES Keys)

The companion installer fetches Sony's official update package:
`http://deu01.psv.update.playstation.net/update/psv/image/2022_0209/rel_f2c7b12fe85496ec88a0391b514d6e3b/PSVUPDAT.PUP`

It decrypts the container using the PlayStation Vita's on-chip **Bigmac DMAC5 Hardware Cryptographic Co-processor** via `SceSblDmac5Mgr` (`sceSblDmac5EncDecKeyGen`, AuthMgr Key ID `0x20`). No plaintext software AES keys are ever stored or exposed in software memory.

---

## Building from Source (VitaSDK)

Ensure `$VITASDK` is installed and `$VITASDK/bin` is in your `PATH`:

### Using Make
```bash
make               # Builds both plugin/itls.skprx and companion-app/Safe-iTLS-Companion.vpk
make -C plugin     # Builds plugin/itls.skprx only
make -C companion-app  # Builds companion-app/Safe-iTLS-Companion.vpk only
```

### Using CMake
```bash
cmake -S plugin -B build/plugin && cmake --build build/plugin
cmake -S companion-app -B build/companion-app && cmake --build build/companion-app
```

---

## Quick Start & Installation

1. **Install and run `Safe-iTLS-Companion.vpk`:**
   Use **UP / DOWN** to select your target partition (**Both `uma0:` and `ux0:`**, `uma0:` only, or `ux0:` only), then press **(X)** to download the update from Sony's servers and hardware-extract the 5 modules.
2. **Install `itls.skprx`:**
   Copy `itls.skprx` to `ur0:tai/itls.skprx`.
3. **Add to `ur0:tai/config.txt`:**
   ```ini
   *KERNEL
   ur0:tai/itls.skprx
   ```
4. **Reboot PS Vita.**
