#include "downloader.h"
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NET_INIT_SIZE (1 * 1024 * 1024)
#define HTTP_POOL_SIZE (512 * 1024)

static bool s_net_initialized = false;
static void *s_net_memory = NULL;
static int s_http_template = -1;
static volatile bool s_cancel_requested = false;

int downloader_init(void) {
    if (s_net_initialized) return 0;

    s_net_memory = malloc(NET_INIT_SIZE);
    if (!s_net_memory) return -1;

    SceNetInitParam net_param;
    net_param.memory = s_net_memory;
    net_param.size = NET_INIT_SIZE;
    net_param.flags = 0;

    int res = sceNetInit(&net_param);
    if (res < 0) {
        free(s_net_memory);
        s_net_memory = NULL;
        return res;
    }

    res = sceNetCtlInit();
    if (res < 0) {
        sceNetTerm();
        free(s_net_memory);
        s_net_memory = NULL;
        return res;
    }

    res = sceHttpInit(HTTP_POOL_SIZE);
    if (res < 0) {
        sceNetCtlTerm();
        sceNetTerm();
        free(s_net_memory);
        s_net_memory = NULL;
        return res;
    }

    s_http_template = sceHttpCreateTemplate("Safe-iTLS-Companion/1.0.0", 2, 1);
    if (s_http_template < 0) {
        sceHttpTerm();
        sceNetCtlTerm();
        sceNetTerm();
        free(s_net_memory);
        s_net_memory = NULL;
        return s_http_template;
    }

    s_net_initialized = true;
    s_cancel_requested = false;
    return 0;
}

void downloader_cancel(void) {
    s_cancel_requested = true;
}

int downloader_fetch_pup(const char *url, const char *destination_path,
                         DownloadProgressCallback callback, void *userdata) {
    if (!url) url = OFFICIAL_PUP_URL;
    if (!destination_path) return -1;

    DownloadProgress progress;
    memset(&progress, 0, sizeof(progress));
    progress.status = DOWNLOAD_STATUS_CONNECTING;
    if (callback) callback(&progress, userdata);

    if (!s_net_initialized) {
        int init_res = downloader_init();
        if (init_res < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = init_res;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Failed to initialize Vita network stack (0x%08X)", init_res);
            if (callback) callback(&progress, userdata);
            return init_res;
        }
    }

    int conn_id = sceHttpCreateConnectionWithURL(s_http_template, url, 0);
    if (conn_id < 0) {
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = conn_id;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Failed to create HTTP connection to CDN (0x%08X)", conn_id);
        if (callback) callback(&progress, userdata);
        return conn_id;
    }

    int req_id = sceHttpCreateRequestWithURL(conn_id, SCE_HTTP_METHOD_GET, url, 0);
    if (req_id < 0) {
        sceHttpDeleteConnection(conn_id);
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = req_id;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Failed to create HTTP request (0x%08X)", req_id);
        if (callback) callback(&progress, userdata);
        return req_id;
    }

    int send_res = sceHttpSendRequest(req_id, NULL, 0);
    if (send_res < 0) {
        sceHttpDeleteRequest(req_id);
        sceHttpDeleteConnection(conn_id);
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = send_res;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Failed to send HTTP request (0x%08X)", send_res);
        if (callback) callback(&progress, userdata);
        return send_res;
    }

    uint64_t content_length = 0;
    int len_res = sceHttpGetContentLength(req_id, &content_length);
    if (len_res >= 0) {
        progress.total_bytes = content_length;
    } else {
        /* Approximate standard 3.65/3.68 update PUP size if chunked */
        progress.total_bytes = 133880832ULL;
    }

    /* Open target destination file for streaming write */
    SceUID fd = sceIoOpen(destination_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
        sceHttpDeleteRequest(req_id);
        sceHttpDeleteConnection(conn_id);
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = fd;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Failed to create destination file: %s (0x%08X)", destination_path, fd);
        if (callback) callback(&progress, userdata);
        return fd;
    }

    uint8_t *chunk_buffer = (uint8_t *)malloc(DOWNLOAD_CHUNK_SIZE);
    if (!chunk_buffer) {
        sceIoClose(fd);
        sceHttpDeleteRequest(req_id);
        sceHttpDeleteConnection(conn_id);
        return -1;
    }

    progress.status = DOWNLOAD_STATUS_DOWNLOADING;
    s_cancel_requested = false;

    SceRtcTick start_tick, now_tick, last_calc_tick;
    sceRtcGetCurrentTick(&start_tick);
    last_calc_tick = start_tick;

    uint64_t bytes_since_last_calc = 0;

    while (!s_cancel_requested) {
        int read_bytes = sceHttpReadData(req_id, chunk_buffer, DOWNLOAD_CHUNK_SIZE);
        if (read_bytes < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = read_bytes;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "HTTP data stream read error (0x%08X)", read_bytes);
            break;
        }
        if (read_bytes == 0) {
            /* Download completed */
            progress.status = DOWNLOAD_STATUS_COMPLETED;
            progress.progress_percent = 100.0;
            break;
        }

        int written = sceIoWrite(fd, chunk_buffer, read_bytes);
        if (written != read_bytes) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -1;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Disk write failure (disk full or write protected)");
            break;
        }

        progress.downloaded_bytes += read_bytes;
        bytes_since_last_calc += read_bytes;

        sceRtcGetCurrentTick(&now_tick);
        uint64_t interval_micro = now_tick.tick - last_calc_tick.tick;

        if (interval_micro >= 500000) { /* Update stats every 500ms */
            double seconds = (double)interval_micro / 1000000.0;
            progress.speed_kb_s = ((double)bytes_since_last_calc / 1024.0) / seconds;
            bytes_since_last_calc = 0;
            last_calc_tick = now_tick;

            if (progress.total_bytes > 0) {
                progress.progress_percent = ((double)progress.downloaded_bytes / (double)progress.total_bytes) * 100.0;
                if (progress.speed_kb_s > 0) {
                    uint64_t remaining_bytes = progress.total_bytes - progress.downloaded_bytes;
                    progress.eta_seconds = (uint32_t)((double)remaining_bytes / (progress.speed_kb_s * 1024.0));
                }
            }

            if (callback) callback(&progress, userdata);
        }
    }

    if (s_cancel_requested && progress.status != DOWNLOAD_STATUS_ERROR) {
        progress.status = DOWNLOAD_STATUS_CANCELLED;
        snprintf(progress.error_message, sizeof(progress.error_message), "Download cancelled by user");
    }

    free(chunk_buffer);
    sceIoClose(fd);
    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);

    if (callback) callback(&progress, userdata);

    return (progress.status == DOWNLOAD_STATUS_COMPLETED) ? 0 : -1;
}

void downloader_term(void) {
    if (!s_net_initialized) return;

    if (s_http_template >= 0) {
        sceHttpDeleteTemplate(s_http_template);
        s_http_template = -1;
    }

    sceHttpTerm();
    sceNetCtlTerm();
    sceNetTerm();

    if (s_net_memory) {
        free(s_net_memory);
        s_net_memory = NULL;
    }

    s_net_initialized = false;
}
