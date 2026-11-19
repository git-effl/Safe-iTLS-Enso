#ifndef HW_CRYPTO_H
#define HW_CRYPTO_H

#include <psp2/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Sony PS Vita Bigmac DMAC5 Hardware Crypto Co-Processor Keyslots
 * Decryption executes strictly in hardware silicon via SceSblDmac5Mgr.
 * ZERO SOFTWARE AES KEYS ARE USED, LOADED, OR HELD IN RAM.
 */
#define BIGMAC_KEYSLOT_PUP_ENC  0x20U  /* Console PUP Package Hardware AuthMgr Key ID */
#define AES_BLOCK_SIZE          16U

typedef struct {
    bool hardware_engine_ready;
    uint32_t keyslot_used;
    uint64_t bytes_decrypted;
    uint32_t blocks_processed;
} HwCryptoStats;

/**
 * @brief Initializes the PS Vita on-chip Bigmac DMAC5 hardware crypto engine.
 * @return 0 on success, negative Vita error code on failure.
 */
int hw_crypto_init(void);

/**
 * @brief Decrypts data purely using the PS Vita's on-chip DMAC5/Bigmac hardware co-processor.
 *
 * Hardware Mechanism:
 * - Invokes sceSblDmac5EncDecKeyGen from <psp2/dmac5.h> (SceSblSsMgr_stub).
 * - Hardware AuthMgr key identifier 0x20 derives key material inside the SCE security enclave.
 * - NO software AES key buffers, key expansions, or S-boxes are ever present in RAM.
 *
 * @param src Pointer to ciphertext buffer.
 * @param dst Pointer to plaintext output buffer.
 * @param size Byte length of data (multiple of 16).
 * @param iv 16-byte Initialization Vector (updated in place with next CBC chaining block).
 * @return 0 on success, negative Vita error code on failure.
 */
int hw_crypto_hardware_decrypt_pup(const uint8_t *src, uint8_t *dst, size_t size, uint8_t *iv);

/**
 * @brief Query hardware crypto co-processor statistics.
 */
void hw_crypto_get_stats(HwCryptoStats *stats);

/**
 * @brief Teardown hardware crypto subsystem.
 */
void hw_crypto_term(void);

#endif /* HW_CRYPTO_H */
