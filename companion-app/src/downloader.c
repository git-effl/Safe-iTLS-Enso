#include "downloader.h"
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/io/fcntl.h>
#include <psp2/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NET_INIT_SIZE         (1 * 1024 * 1024)
#define HTTP_HEADER_BUF_SIZE  8192
#define HTTP_MAX_REDIRECTS    4
#define HTTP_HOST_MAX         256
#define HTTP_PATH_MAX         512

static bool s_net_initialized = false;
static bool s_sysmod_net_loaded = false;
static void *s_net_memory = NULL;
static volatile bool s_cancel_requested = false;

static int ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int ascii_strncasecmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = ascii_tolower((unsigned char)a[i]);
        int cb = ascii_tolower((unsigned char)b[i]);
        if (ca != cb) {
            return ca - cb;
        }
        if (ca == '\0') {
            return 0;
        }
    }
    return 0;
}

static int parse_http_url(const char *url, char *host, size_t host_max,
                          uint16_t *port, char *path, size_t path_max) {
    if (!url || !host || !port || !path) {
        return -1;
    }

    const char *p = url;
    if (ascii_strncasecmp(p, "http://", 7) == 0) {
        p += 7;
    } else {
        return -1; /* Only plain HTTP is supported on raw SceNet sockets */
    }

    const char *slash = strchr(p, '/');
    const char *host_end = slash ? slash : (p + strlen(p));
    const char *colon = NULL;

    for (const char *c = p; c < host_end; c++) {
        if (*c == ':') {
            colon = c;
            break;
        }
    }

    size_t host_len = colon ? (size_t)(colon - p) : (size_t)(host_end - p);
    if (host_len == 0 || host_len >= host_max) {
        return -1;
    }

    memcpy(host, p, host_len);
    host[host_len] = '\0';

    if (colon) {
        unsigned long parsed_port = strtoul(colon + 1, NULL, 10);
        if (parsed_port == 0 || parsed_port > 65535UL) {
            return -1;
        }
        *port = (uint16_t)parsed_port;
    } else {
        *port = 80;
    }

    if (slash) {
        if (strlen(slash) >= path_max) {
            return -1;
        }
        snprintf(path, path_max, "%s", slash);
    } else {
        snprintf(path, path_max, "/");
    }

    return 0;
}

static int send_all(int sock, const void *buf, size_t len) {
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t sent_total = 0;

    while (sent_total < len) {
        int sent = sceNetSend(sock, ptr + sent_total, (unsigned int)(len - sent_total), 0);
        if (sent <= 0) {
            return (sent < 0) ? sent : -1;
        }
        sent_total += (size_t)sent;
    }

    return 0;
}

static bool extract_header_value(const char *headers, const char *header_name,
                                 char *out, size_t out_max) {
    size_t name_len = strlen(header_name);
    const char *line = headers;

    while (line && *line != '\0') {
        const char *line_end = strstr(line, "\r\n");
        size_t line_len = line_end ? (size_t)(line_end - line) : strlen(line);
        if (line_len == 0) {
            break;
        }

        if (line_len > name_len &&
            ascii_strncasecmp(line, header_name, name_len) == 0 &&
            line[name_len] == ':') {
            const char *val = line + name_len + 1;
            const char *end = line + line_len;
            while (val < end && (*val == ' ' || *val == '\t')) {
                val++;
            }
            while (end > val && (end[-1] == ' ' || end[-1] == '\t')) {
                end--;
            }

            size_t copy_len = (size_t)(end - val);
            if (copy_len >= out_max) {
                copy_len = out_max - 1;
            }
            memcpy(out, val, copy_len);
            out[copy_len] = '\0';
            return true;
        }

        if (!line_end) {
            break;
        }
        line = line_end + 2;
    }

    return false;
}

int downloader_init(void) {
    if (s_net_initialized) {
        return 0;
    }

    /*
     * Load ONLY SCE_SYSMODULE_NET and initialize SceNet directly.
     * Avoids SceLibHttp (libhttp.suprx) and unused SceNetCtl (libnetctl.suprx / NID 0xD943CE15).
     */
    int res = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if (res < 0) {
        return res;
    }
    s_sysmod_net_loaded = true;

    s_net_memory = malloc(NET_INIT_SIZE);
    if (!s_net_memory) {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
        s_sysmod_net_loaded = false;
        return -1;
    }

    SceNetInitParam net_param;
    net_param.memory = s_net_memory;
    net_param.size = NET_INIT_SIZE;
    net_param.flags = 0;

    res = sceNetInit(&net_param);
    if (res < 0) {
        free(s_net_memory);
        s_net_memory = NULL;
        sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
        s_sysmod_net_loaded = false;
        return res;
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
                     "Failed to initialize SceNet stack (0x%08X)", (unsigned int)init_res);
            if (callback) callback(&progress, userdata);
            return init_res;
        }
    }

    char current_url[HTTP_HOST_MAX + HTTP_PATH_MAX + 32];
    snprintf(current_url, sizeof(current_url), "%s", url);

    char *header_buf = (char *)malloc(HTTP_HEADER_BUF_SIZE + 1);
    uint8_t *chunk_buffer = (uint8_t *)malloc(DOWNLOAD_CHUNK_SIZE);
    if (!header_buf || !chunk_buffer) {
        free(header_buf);
        free(chunk_buffer);
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = -1;
        snprintf(progress.error_message, sizeof(progress.error_message), "Out of memory");
        if (callback) callback(&progress, userdata);
        return -1;
    }

    int sock = -1;
    size_t header_received = 0;
    const char *header_end = NULL;
    uint64_t content_length = 0;
    bool has_content_length = false;

    for (int redirect_hop = 0; redirect_hop <= HTTP_MAX_REDIRECTS; redirect_hop++) {
        char host[HTTP_HOST_MAX];
        char path[HTTP_PATH_MAX];
        uint16_t port = 80;

        if (parse_http_url(current_url, host, sizeof(host), &port, path, sizeof(path)) < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -2;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Invalid HTTP URL format");
            goto fail;
        }

        int rid = sceNetResolverCreate("itls_dns", NULL, 0);
        if (rid < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = rid;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "DNS resolver create failed (0x%08X)", (unsigned int)rid);
            goto fail;
        }

        SceNetInAddr resolved_addr;
        memset(&resolved_addr, 0, sizeof(resolved_addr));
        int dns_res = sceNetResolverStartNtoa(rid, host, &resolved_addr, 0, 0, 0);
        sceNetResolverDestroy(rid);
        if (dns_res < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = dns_res;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "DNS lookup failed for %s (0x%08X)", host, (unsigned int)dns_res);
            goto fail;
        }

        sock = sceNetSocket("itls_http", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
        if (sock < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = sock;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Socket creation failed (0x%08X)", (unsigned int)sock);
            goto fail;
        }

        SceNetSockaddrIn server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_len = sizeof(server_addr);
        server_addr.sin_family = SCE_NET_AF_INET;
        server_addr.sin_port = sceNetHtons(port);
        server_addr.sin_addr = resolved_addr;

        int conn_res = sceNetConnect(sock, (const SceNetSockaddr *)&server_addr, sizeof(server_addr));
        if (conn_res < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = conn_res;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "TCP connect failed to %s:%u (0x%08X)", host, (unsigned int)port, (unsigned int)conn_res);
            goto fail;
        }

        char req_buf[1024];
        int req_len = snprintf(req_buf, sizeof(req_buf),
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: Safe-iTLS-Companion/1.3.0\r\n"
                               "Accept: */*\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               path, host);
        if (req_len <= 0 || (size_t)req_len >= sizeof(req_buf)) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -3;
            snprintf(progress.error_message, sizeof(progress.error_message), "HTTP request too long");
            goto fail;
        }

        int send_res = send_all(sock, req_buf, (size_t)req_len);
        if (send_res < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = send_res;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Failed to send HTTP request (0x%08X)", (unsigned int)send_res);
            goto fail;
        }

        /* Receive HTTP response headers until "\r\n\r\n" */
        header_received = 0;
        header_end = NULL;
        while (header_received < HTTP_HEADER_BUF_SIZE) {
            int r = sceNetRecv(sock, header_buf + header_received,
                               (unsigned int)(HTTP_HEADER_BUF_SIZE - header_received), 0);
            if (r < 0) {
                progress.status = DOWNLOAD_STATUS_ERROR;
                progress.error_code = r;
                snprintf(progress.error_message, sizeof(progress.error_message),
                         "HTTP header read error (0x%08X)", (unsigned int)r);
                goto fail;
            }
            if (r == 0) {
                break;
            }
            header_received += (size_t)r;
            header_buf[header_received] = '\0';
            header_end = strstr(header_buf, "\r\n\r\n");
            if (header_end) {
                break;
            }
        }

        if (!header_end) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -4;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Incomplete HTTP response headers from server");
            goto fail;
        }

        /* Parse status line: e.g. "HTTP/1.1 200 OK" */
        int status_code = 0;
        if (sscanf(header_buf, "HTTP/%*d.%*d %d", &status_code) != 1) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -5;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Malformed HTTP status line");
            goto fail;
        }

        if (status_code == 301 || status_code == 302 || status_code == 303 ||
            status_code == 307 || status_code == 308) {
            char location[HTTP_HOST_MAX + HTTP_PATH_MAX + 32];
            if (!extract_header_value(header_buf, "Location", location, sizeof(location))) {
                progress.status = DOWNLOAD_STATUS_ERROR;
                progress.error_code = status_code;
                snprintf(progress.error_message, sizeof(progress.error_message),
                         "HTTP %d redirect missing Location header", status_code);
                goto fail;
            }
            sceNetSocketClose(sock);
            sock = -1;
            snprintf(current_url, sizeof(current_url), "%s", location);
            continue;
        }

        if (status_code != 200) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = status_code;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Unexpected HTTP status code: %d", status_code);
            goto fail;
        }

        char content_len_str[64];
        if (extract_header_value(header_buf, "Content-Length", content_len_str, sizeof(content_len_str))) {
            content_length = (uint64_t)strtoull(content_len_str, NULL, 10);
            if (content_length > 0) {
                has_content_length = true;
            }
        }

        break;
    }

    if (sock < 0 || !header_end) {
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = -6;
        snprintf(progress.error_message, sizeof(progress.error_message), "Too many HTTP redirects");
        goto fail;
    }

    progress.total_bytes = has_content_length ? content_length : 133834240ULL;

    /* Open target destination file for streaming write */
    SceUID fd = sceIoOpen(destination_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
        progress.status = DOWNLOAD_STATUS_ERROR;
        progress.error_code = fd;
        snprintf(progress.error_message, sizeof(progress.error_message),
                 "Failed to create destination file: %s (0x%08X)", destination_path, (unsigned int)fd);
        goto fail;
    }

    progress.status = DOWNLOAD_STATUS_DOWNLOADING;
    s_cancel_requested = false;

    SceRtcTick start_tick, now_tick, last_calc_tick;
    sceRtcGetCurrentTick(&start_tick);
    last_calc_tick = start_tick;
    uint64_t bytes_since_last_calc = 0;

    /* Write any initial body bytes already read right after "\r\n\r\n" */
    size_t body_offset = (size_t)((header_end + 4) - header_buf);
    if (header_received > body_offset) {
        size_t initial_body_len = header_received - body_offset;
        if (has_content_length && (uint64_t)initial_body_len > content_length) {
            initial_body_len = (size_t)content_length;
        }
        int written = sceIoWrite(fd, header_buf + body_offset, (SceSize)initial_body_len);
        if (written != (int)initial_body_len) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -1;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Disk write failure (disk full or write protected)");
            sceIoClose(fd);
            sceIoRemove(destination_path);
            goto fail;
        }
        progress.downloaded_bytes += (uint64_t)initial_body_len;
        bytes_since_last_calc += (uint64_t)initial_body_len;
    }

    while (!s_cancel_requested) {
        if (has_content_length && progress.downloaded_bytes >= content_length) {
            progress.status = DOWNLOAD_STATUS_COMPLETED;
            progress.progress_percent = 100.0;
            break;
        }

        unsigned int to_recv = DOWNLOAD_CHUNK_SIZE;
        if (has_content_length) {
            uint64_t remaining = content_length - progress.downloaded_bytes;
            if (remaining < (uint64_t)to_recv) {
                to_recv = (unsigned int)remaining;
            }
        }

        int read_bytes = sceNetRecv(sock, chunk_buffer, to_recv, 0);
        if (read_bytes < 0) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = read_bytes;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Network stream read error (0x%08X)", (unsigned int)read_bytes);
            break;
        }
        if (read_bytes == 0) {
            if (progress.downloaded_bytes == 0 ||
                (has_content_length && progress.downloaded_bytes < content_length)) {
                progress.status = DOWNLOAD_STATUS_ERROR;
                progress.error_code = -7;
                snprintf(progress.error_message, sizeof(progress.error_message),
                         "Connection closed early (%llu / %llu bytes)",
                         (unsigned long long)progress.downloaded_bytes,
                         (unsigned long long)progress.total_bytes);
            } else {
                progress.status = DOWNLOAD_STATUS_COMPLETED;
                progress.progress_percent = 100.0;
            }
            break;
        }

        int written = sceIoWrite(fd, chunk_buffer, (SceSize)read_bytes);
        if (written != read_bytes) {
            progress.status = DOWNLOAD_STATUS_ERROR;
            progress.error_code = -1;
            snprintf(progress.error_message, sizeof(progress.error_message),
                     "Disk write failure (disk full or write protected)");
            break;
        }

        progress.downloaded_bytes += (uint64_t)read_bytes;
        bytes_since_last_calc += (uint64_t)read_bytes;

        sceRtcGetCurrentTick(&now_tick);
        uint64_t interval_micro = now_tick.tick - last_calc_tick.tick;

        if (interval_micro >= 500000ULL) { /* Update stats every 500ms */
            double seconds = (double)interval_micro / 1000000.0;
            progress.speed_kb_s = ((double)bytes_since_last_calc / 1024.0) / seconds;
            bytes_since_last_calc = 0;
            last_calc_tick = now_tick;

            if (progress.total_bytes > 0) {
                double pct = ((double)progress.downloaded_bytes / (double)progress.total_bytes) * 100.0;
                progress.progress_percent = (pct > 100.0) ? 100.0 : pct;
                if (progress.speed_kb_s > 0.0) {
                    uint64_t remaining_bytes = (progress.total_bytes > progress.downloaded_bytes)
                        ? (progress.total_bytes - progress.downloaded_bytes)
                        : 0;
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

    sceIoClose(fd);
    if (progress.status != DOWNLOAD_STATUS_COMPLETED) {
        sceIoRemove(destination_path);
    }

    sceNetSocketClose(sock);
    free(chunk_buffer);
    free(header_buf);

    if (callback) callback(&progress, userdata);
    return (progress.status == DOWNLOAD_STATUS_COMPLETED) ? 0 : -1;

fail:
    if (sock >= 0) {
        sceNetSocketClose(sock);
    }
    free(chunk_buffer);
    free(header_buf);
    if (callback) callback(&progress, userdata);
    return -1;
}

void downloader_term(void) {
    if (!s_net_initialized) {
        return;
    }

    sceNetTerm();

    if (s_net_memory) {
        free(s_net_memory);
        s_net_memory = NULL;
    }

    if (s_sysmod_net_loaded) {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
        s_sysmod_net_loaded = false;
    }

    s_net_initialized = false;
}
