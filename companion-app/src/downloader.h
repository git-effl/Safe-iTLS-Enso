#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define OFFICIAL_PUP_URL "http://deu01.psv.update.playstation.net/update/psv/image/2022_0209/rel_f2c7b12fe85496ec88a0391b514d6e3b/PSVUPDAT.PUP"
#define DOWNLOAD_CHUNK_SIZE (64 * 1024) /* 64 KB chunks for balanced I/O & memory */

typedef enum {
    DOWNLOAD_STATUS_IDLE = 0,
    DOWNLOAD_STATUS_CONNECTING,
    DOWNLOAD_STATUS_DOWNLOADING,
    DOWNLOAD_STATUS_COMPLETED,
    DOWNLOAD_STATUS_ERROR,
    DOWNLOAD_STATUS_CANCELLED
} DownloadStatus;

typedef struct {
    DownloadStatus status;
    uint64_t total_bytes;
    uint64_t downloaded_bytes;
    double progress_percent;
    double speed_kb_s;
    uint32_t eta_seconds;
    int error_code;
    char error_message[128];
} DownloadProgress;

typedef void (*DownloadProgressCallback)(const DownloadProgress *progress, void *userdata);

/**
 * @brief Initialize PS Vita Network and HTTP subsystems (SceNet, SceNetCtl, SceHttp, SceSsl).
 */
int downloader_init(void);

/**
 * @brief Download official PUP firmware file from PlayStation CDN directly to target destination.
 *
 * @param url Remote HTTP/HTTPS URL (or pass NULL to use OFFICIAL_PUP_URL).
 * @param destination_path Target local file path (e.g. "ux0:/data/itls/PSVUPDAT.PUP").
 * @param callback Progress update callback.
 * @param userdata User pointer passed back into callback.
 * @return 0 on success, negative error code on failure.
 */
int downloader_fetch_pup(const char *url, const char *destination_path,
                         DownloadProgressCallback callback, void *userdata);

/**
 * @brief Request cancellation of ongoing download.
 */
void downloader_cancel(void);

/**
 * @brief Teardown Network and HTTP subsystems.
 */
void downloader_term(void);

#endif /* DOWNLOADER_H */
