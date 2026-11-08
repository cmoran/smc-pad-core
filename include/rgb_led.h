/**
 * @file rgb_led.h
 * @brief WS2812/SK6812 RGB LED Driver for M-VAVE SMC-Pad 4x4 Matrix
 *
 * Drives the 16 RGB LEDs daisy-chained on PB1 via SPI1 single-wire DMA.
 * Hardware mapping translates logical pad indices (0..15) to physical chain indices.
 */

#ifndef SMC_RGB_LED_H
#define SMC_RGB_LED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "smc_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_RGB_NUM_PADS        SMC_NUM_PADS    /* 16 */
#define SMC_RGB_DMA_BUFFER_SIZE 548             /* 2 lead bytes + 16*24 bytes + 162 reset bytes */
#define SMC_RGB_SPI_BAUD_HZ     6000000u        /* 6.0 MHz bit rate */

/* Color helper macro 0xRRGGBB */
#define SMC_RGB(r, g, b)        (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/* Preset visual colors */
#define SMC_COLOR_BLACK         0x000000u
#define SMC_COLOR_RED           0xFF0000u
#define SMC_COLOR_GREEN         0x00FF00u
#define SMC_COLOR_BLUE          0x0000FFu
#define SMC_COLOR_YELLOW        0xFFFF00u
#define SMC_COLOR_CYAN          0x00FFFFu
#define SMC_COLOR_MAGENTA       0xFF00FFu
#define SMC_COLOR_WHITE         0xFFFFFFu
#define SMC_COLOR_DIM_BLUE      0x000020u
#define SMC_COLOR_DIM_GREEN     0x002000u
#define SMC_COLOR_DIM_ORANGE    0x201000u
#define SMC_COLOR_DIM_AMBER     0x201000u
#define SMC_COLOR_DIM_MAGENTA   0x200020u
#define SMC_COLOR_DIM_CYAN      0x002020u

typedef struct {
    uint32_t idle_color;
    uint32_t hit_color;
} smc_bank_palette_t;

extern const smc_bank_palette_t smc_bank_palettes[4];

/**
 * @brief Get color palette for a given virtual bank (0..3).
 */
const smc_bank_palette_t *smc_rgb_led_get_bank_palette(uint8_t bank);

/**
 * @brief Apply the idle color theme for the given bank across all 16 pads.
 */
void smc_rgb_led_set_bank_theme(uint8_t bank);

/* Logical pad to physical WS2812 daisy-chain index mapping table */
extern const uint8_t smc_pad_to_led_map[SMC_RGB_NUM_PADS];

/**
 * @brief Initialize the RGB LED driver subsystem.
 */
void smc_rgb_led_init(void);

/**
 * @brief Configure base clock for SPI baud rate calculation.
 *
 * @param lsb_clock_hz Clock frequency in Hz (typically 24, 48, or 96 MHz).
 */
void smc_rgb_led_set_clock(uint32_t lsb_clock_hz);

/**
 * @brief Set color for a logical pad (0..15).
 *
 * @param pad_idx Logical pad index (0..15).
 * @param rgb 24-bit RGB color (0xRRGGBB).
 */
void smc_rgb_led_set_pad(uint8_t pad_idx, uint32_t rgb);

/**
 * @brief Set color for all 16 pads.
 *
 * @param rgb 24-bit RGB color (0xRRGGBB).
 */
void smc_rgb_led_set_all(uint32_t rgb);

/**
 * @brief Get current color for a logical pad (0..15).
 *
 * @param pad_idx Logical pad index (0..15).
 * @return uint32_t 24-bit RGB color (0xRRGGBB).
 */
uint32_t smc_rgb_led_get_pad(uint8_t pad_idx);

/**
 * @brief Serialize current pad colors into WS2812 GRB SPI DMA buffer.
 *
 * @param dest_buf Destination buffer (minimum 548 bytes).
 * @param max_len Maximum length of destination buffer.
 * @return size_t Actual bytes written.
 */
size_t smc_rgb_led_serialize(uint8_t *dest_buf, size_t max_len);

/**
 * @brief Check if SPI DMA transfer is currently in progress.
 *
 * @return true if busy, false if idle and ready for new frame.
 */
bool smc_rgb_led_is_busy(void);

/**
 * @brief Transmit the serialized color buffer to physical LEDs via SPI1 DMA.
 * Non-blocking on hardware if previous transfer has completed.
 */
void smc_rgb_led_show(void);

/**
 * @brief Render the Shift Status Dashboard across the 4x4 matrix.
 *
 * @param bank Active bank index (0..3).
 * @param velocity_curve Active velocity curve (0..3).
 * @param octave_offset Current octave offset (-48..+48).
 * @param semitone_offset Current semitone transpose offset (-12..+12).
 */
void smc_rgb_led_render_shift_status(uint8_t bank,
                                     uint8_t velocity_curve,
                                     int8_t octave_offset,
                                     int8_t semitone_offset);

/**
 * @brief Perform the startup boot animation (center-outward spiral flare).
 *
 * @param delay_fn Callback to delay milliseconds (e.g. smcpad_delay or NULL in tests).
 * @param show_fn Callback to show frame (e.g. smcpad_rgb_show or NULL).
 */
void smc_rgb_led_boot_animation(void (*delay_fn)(uint32_t ms), void (*show_fn)(void));

#ifdef __cplusplus
}
#endif

#endif /* SMC_RGB_LED_H */
