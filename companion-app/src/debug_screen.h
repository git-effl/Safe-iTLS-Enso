#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

#include <stdint.h>

/**
 * @brief Initialize PS Vita CDRAM framebuffer console (960x544 RGBA8888).
 * @return 0 on success, negative Vita error code on failure.
 */
int psvDebugScreenInit(void);

/**
 * @brief Clear framebuffer with an RGB color (0x00RRGGBB) and reset cursor to (0, 0).
 */
void psvDebugScreenClear(uint32_t color);

/**
 * @brief Print formatted ASCII text to the PS Vita framebuffer console.
 */
int psvDebugScreenPrintf(const char *format, ...);

/**
 * @brief Free framebuffer memory block on exit.
 */
void psvDebugScreenTerm(void);

#endif /* DEBUG_SCREEN_H */
