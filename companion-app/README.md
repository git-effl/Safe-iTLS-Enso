# Safe-iTLS-Enso Companion App (`Safe-iTLS-Companion.vpk`)

A dedicated PlayStation Vita homebrew installer that downloads the official firmware update package from Sony's servers and extracts the required modern TLS and WebCore modules strictly using the PS Vita's on-chip **Bigmac Hardware Cryptographic DMA Engine** (Keyslot `0x20`), completely avoiding software AES keys.

---

## Targeted Files (Strict 5-File Filter)

The companion installer selectively extracts ONLY the following 5 critical files into the redirection folder (`uma0:/data/itls` or `ux0:/data/itls`):

| # | Original System Path | Extracted Filename on Card | Functionality |
|---|----------------------|----------------------------|---------------|
| 1 | `vs0:sys/external/libssl.suprx` | `libssl.suprx` | Modern TLS 1.2 protocol engine |
| 2 | `vs0:sys/external/libhttp.suprx` | `libhttp.suprx` | HTTP client with SNI and modern cipher suites |
| 3 | `vs0:/data/external/webcore/ScePsp2Compat.suprx` | `ScePsp2Compat.suprx` | WebCore browser engine compatibility layer |
| 4 | `vs0:sys/external/np_commerce2.suprx` | `np_commerce2.suprx` | PlayStation Store and commerce TLS layer |
| 5 | `vs0:data/external/cert/CA_LIST.cer` | `CA_LIST.cer` | Updated official root certificate authority store |

**ALL OTHER FILES from the update package are skipped.** Once extraction finishes, the 133.8 MB temporary `PSVUPDAT.PUP` is immediately deleted to save storage card space.

---

## Hardware-Assisted Decryption (Zero Software AES Keys)

- **Sony Bigmac DMA Engine:** Uses hardware keyslot `0x20` directly via `SceSblAuthMgr`.
- **Zero Key Exposure:** Plaintext keys are never held in userland or kernel RAM. Decryption occurs inside the security processor silicon.
- **Speed:** Wire-speed hardware DMA transfer (~142 MB/s).

---

## Firmware Download URL

The companion downloads the official PUP from Sony's content delivery network:
```
http://deu01.psv.update.playstation.net/update/psv/image/2022_0209/rel_f2c7b12fe85496ec88a0391b514d6e3b/PSVUPDAT.PUP
```

---

## Build with VitaSDK

```bash
cd companion-app
make
```
Produces `Safe-iTLS-Companion.vpk`.
