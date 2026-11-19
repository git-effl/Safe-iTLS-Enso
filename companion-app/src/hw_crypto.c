#include "hw_crypto.h"
#include <psp2/dmac5.h>
#include <string.h>

/*
 * Forward-declare sceSblDmac5EncDecKeyGen (exported by SceSblDmac5Mgr / SceSblSsMgr_stub,
 * NID 0x5BF4F924) so compilation succeeds on VitaSDK releases whose <psp2/dmac5.h> predates
 * the prototype addition as well as newer VitaSDK releases (identical redeclaration is valid C11).
 */
int sceSblDmac5EncDecKeyGen(const SceSblDmac5EncDecParam *param, SceUInt32 key_id, SceUInt32 command);

/*
 * PS Vita DMAC5 (Bigmac) AES-128-CBC Decrypt Command Word (from <psp2/dmac5.h>):
 *   bit 1 (2)  = Decrypt
 *   bit 3 (8)  = CBC mode
 *   keysize128 = (((128 << 2) - 0x100) & 0x300) = 0x100
 */
#define DMAC5_CMD_AES128_CBC_DEC (2U | 8U | (((128U << 2) - 0x100U) & 0x300U))

static bool s_initialized = false;
static HwCryptoStats s_stats;

int hw_crypto_init(void) {
    if (s_initialized) {
        return 0;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.hardware_engine_ready = true;
    s_stats.keyslot_used = BIGMAC_KEYSLOT_PUP_ENC;
    s_initialized = true;
    return 0;
}

int hw_crypto_hardware_decrypt_pup(const uint8_t *src, uint8_t *dst, size_t size, uint8_t *iv) {
    if (!src || !dst || size == 0 || (size % AES_BLOCK_SIZE) != 0) {
        return -1;
    }

    if (!s_initialized) {
        int init_res = hw_crypto_init();
        if (init_res < 0) {
            return init_res;
        }
    }

    uint8_t local_iv[AES_BLOCK_SIZE];
    if (iv) {
        memcpy(local_iv, iv, AES_BLOCK_SIZE);
    } else {
        memset(local_iv, 0, AES_BLOCK_SIZE);
    }

    /*
     * Pure Hardware Decryption via official VitaSDK SceSblDmac5Mgr:
     * Calls sceSblDmac5EncDecKeyGen with AuthMgr hardware key_id 0x20.
     * No software AES key bytes are loaded or held in application memory.
     */
    SceSblDmac5EncDecParam param;
    memset(&param, 0, sizeof(param));
    param.src     = src;
    param.dst     = dst;
    param.length  = (SceSize)size;
    param.key     = NULL;
    param.keysize = 128;
    param.iv      = local_iv;

    int res = sceSblDmac5EncDecKeyGen(&param, BIGMAC_KEYSLOT_PUP_ENC, DMAC5_CMD_AES128_CBC_DEC);
    if (res >= 0) {
        if (iv) {
            memcpy(iv, local_iv, AES_BLOCK_SIZE);
        }
        s_stats.bytes_decrypted += (uint64_t)size;
        s_stats.blocks_processed += (uint32_t)(size / AES_BLOCK_SIZE);
    }

    return res;
}

void hw_crypto_get_stats(HwCryptoStats *stats) {
    if (!stats) {
        return;
    }
    memcpy(stats, &s_stats, sizeof(HwCryptoStats));
}

void hw_crypto_term(void) {
    s_initialized = false;
}
