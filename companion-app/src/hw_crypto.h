#ifndef HW_CRYPTO_H
#define HW_CRYPTO_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Sony PS Vita Bigmac Hardware Engine Keyslots */
#define BIGMAC_KEYSLOT_PUP_ENC      0x20    /* Firmware package payload AES key */
#define BIGMAC_KEYSLOT_PUP_META     0x21    /* Firmware segment header / TOC key */
#define BIGMAC_KEYSLOT_PFS_APP      0x28    /* PFS encrypted package key */
#define BIGMAC_KEYSLOT_USER_CUSTOM  0x30    /* Custom software key load slot */

#define AES_BLOCK_SIZE 16

typedef enum {
    HW_CRYPTO_MODE_CBC = 0,
    HW_CRYPTO_MODE_CTR = 1,
    HW_CRYPTO_MODE_ECB = 2
} HwCryptoCipherMode;

typedef struct {
    bool hardware_engine_ready;
    bool bigmac_dma_available;
    bool sbl_authmgr_linked;
    uint32_t active_keyslot;
    uint64_t bytes_processed;
    double throughput_mb_s;
} HwCryptoStats;

/**
 * @brief Initialize connection to PS Vita hardware crypto subsystem (Bigmac/SceSblAuthMgr).
 * @return 0 on success, negative error code on failure.
 */
int hw_crypto_init(void);

/**
 * @brief Decrypt data block utilizing the PS Vita's on-chip hardware cryptographic DMA engine.
 * The operation executes inside the Vita's hardware security enclave, avoiding software key exposure.
 *
 * @param src Pointer to encrypted source ciphertext (16-byte aligned).
 * @param dst Pointer to destination plaintext buffer (16-byte aligned).
 * @param size Data size in bytes (must be a multiple of 16).
 * @param keyslot Sony hardware keyslot ID (e.g., BIGMAC_KEYSLOT_PUP_ENC).
 * @param iv 16-byte Initialization Vector.
 * @param mode Cipher mode (HW_CRYPTO_MODE_CBC or HW_CRYPTO_MODE_CTR).
 * @return 0 on success, negative Vita error code on failure.
 */
int hw_crypto_decrypt(const uint8_t *src, uint8_t *dst, size_t size,
                      uint32_t keyslot, const uint8_t *iv,
                      HwCryptoCipherMode mode);

/**
 * @brief Fetch current operational status and hardware acceleration statistics.
 */
void hw_crypto_get_stats(HwCryptoStats *stats);

/**
 * @brief Teardown hardware crypto engine context.
 */
void hw_crypto_term(void);

#endif /* HW_CRYPTO_H */
