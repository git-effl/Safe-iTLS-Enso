# Safe-iTLS-Enso Companion App

A specialized PS Vita homebrew companion utility that automates official firmware retrieval, hardware-accelerated PUP decryption using the PlayStation Vita's on-chip Bigmac security co-processor, and storage-aware deployment of modern TLS 1.2/1.3 modules for **Safe-iTLS-Enso**.

---

## Key Features

1. **Automated Official Firmware Download**
   - Direct streaming download from the official PlayStation CDN:
     `http://deu01.psv.update.playstation.net/update/psv/image/2022_0209/rel_f2c7b12fe85496ec88a0391b514d6e3b/PSVUPDAT.PUP`
   - Memory-efficient 64 KB chunk buffering using `SceHttp` and `SceNet`.
   - Real-time download rate, percentage, and ETA computation.

2. **Hardware-Assisted Extraction & Decryption**
   - Utilizes Sony's **Bigmac DMA Hardware Cryptographic Engine** via `SceSblAuthMgr`.
   - Uses hardware keyslot `0x20` (PUP container AES key) directly in hardware silicon, eliminating software key exposure and delivering wire-speed DMA throughput.
   - Extracts essential system modules required by Safe-iTLS (`psp2swu.self`, `itls.skprx`, `libssl.suprx`, `libhttp.suprx`, `ca-bundle.pem`).

3. **Dynamic Storage Routing**
   - Automatically detects available storage hardware:
     - **SD2Vita Adapter**: Routed to `uma0:/data/itls`
     - **Official Memory Card / Internal**: Routed to `ux0:/data/itls`
   - Recursive directory creation and verification.
   - Generates `itls_firmware_info.txt` metadata manifest.

4. **Interactive Controls**
   - **[X]**: Automated Pipeline (Storage Detect -> Download -> Hardware Decrypt -> Install).
   - **[SQUARE]**: Force re-probe of storage media.
   - **[TRIANGLE]**: Run Bigmac Hardware Crypto Benchmark (4 MB DMA test).
   - **[START]**: Exit back to PS Vita LiveArea.

---

## Directory Structure

```
companion-app/
├── CMakeLists.txt              # CMake build configuration for VitaSDK
├── Makefile                    # Standard Makefile for VitaSDK
├── README.md                   # This documentation
├── sce_sys/
│   └── livearea/
│       └── contents/
│           └── template.xml    # LiveArea layout configuration
└── src/
    ├── downloader.c            # Streaming SceHttp downloader
    ├── downloader.h
    ├── hw_crypto.c             # Bigmac hardware crypto accelerator driver
    ├── hw_crypto.h
    ├── main.c                  # Controller input, UI, and state machine
    ├── pup_extractor.c         # PS Vita PUP container parser and extractor
    ├── pup_extractor.h
    ├── storage.c               # Dynamic storage detection (ux0: vs uma0:)
    └── storage.h
```

---

## Building with VitaSDK

### Prerequisites
- [VitaSDK](https://vitasdk.org/) installed and configured on your system.
- Environment variable `VITASDK` pointing to your toolchain installation (e.g. `/usr/local/vitasdk`).
- `PATH` contains `$VITASDK/bin`.

### Building with GNU Make
```bash
cd companion-app
make
```
This produces `Safe-iTLS-Companion.vpk` in the project root.

### Building with CMake
```bash
cd companion-app
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake ..
make
```

### Installation on PS Vita
1. Copy `Safe-iTLS-Companion.vpk` to your PS Vita via VitaShell (FTP or USB).
2. Navigate to the `.vpk` file in VitaShell and press **(X)** to install.
3. Launch **Safe-iTLS Companion** from your LiveArea.

---

## Git Integration & Pushing to Dedicated Branch

To push these changes to a dedicated branch using the provided credentials:

```bash
# Initialize or checkout new branch
git checkout -b feature/firmware-companion-downloader

# Stage companion app files
git add companion-app/

# Configure committer
git config user.name "Gemini"
git config user.email "gemini@google.com"

# Commit changes
git commit -m "feat(companion): add firmware downloader and hardware-accelerated PUP extractor"

# Push to remote repository
git push https://x-access-token:<PAT>@github.com/git-effl/Safe-iTLS-Enso.git feature/firmware-companion-downloader
```
