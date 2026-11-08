/**
 * @file test_rgb_led.c
 * @brief Host unit test for SMC-Pad WS2812 RGB LED Driver
 */

#include "rgb_led.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_initialization_and_defaults(void)
{
    printf("Running test_initialization_and_defaults...\n");
    smc_rgb_led_init();

    for (uint8_t i = 0; i < SMC_RGB_NUM_PADS; i++) {
        assert(smc_rgb_led_get_pad(i) == SMC_COLOR_BLACK);
    }
    printf("PASS: test_initialization_and_defaults\n");
}

static void test_pad_mapping_and_colors(void)
{
    printf("Running test_pad_mapping_and_colors...\n");
    smc_rgb_led_init();

    /* Set Pad 0 to Red (Chain index 11) */
    smc_rgb_led_set_pad(0, SMC_COLOR_RED);
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_RED);
    assert(smc_pad_to_led_map[0] == 11);

    /* Set Pad 5 to Green (Chain index 14) */
    smc_rgb_led_set_pad(5, SMC_COLOR_GREEN);
    assert(smc_rgb_led_get_pad(5) == SMC_COLOR_GREEN);
    assert(smc_pad_to_led_map[5] == 14);

    /* Other pads should still be black */
    assert(smc_rgb_led_get_pad(1) == SMC_COLOR_BLACK);
    assert(smc_rgb_led_get_pad(4) == SMC_COLOR_BLACK);

    /* Out of bounds index check */
    smc_rgb_led_set_pad(16, SMC_COLOR_WHITE);
    assert(smc_rgb_led_get_pad(16) == 0);

    /* Set all */
    smc_rgb_led_set_all(SMC_COLOR_CYAN);
    for (uint8_t i = 0; i < SMC_RGB_NUM_PADS; i++) {
        assert(smc_rgb_led_get_pad(i) == SMC_COLOR_CYAN);
    }

    printf("PASS: test_pad_mapping_and_colors\n");
}

static void test_ws2812_grb_serialization(void)
{
    printf("Running test_ws2812_grb_serialization...\n");
    smc_rgb_led_init();

    /* Set Pad 0 to Green (0x00FF00) */
    smc_rgb_led_set_pad(0, SMC_COLOR_GREEN);

    uint8_t dma_buf[SMC_RGB_DMA_BUFFER_SIZE];
    size_t written = smc_rgb_led_serialize(dma_buf, sizeof(dma_buf));
    assert(written == SMC_RGB_DMA_BUFFER_SIZE);

    /* Lead bytes must be 0x00 */
    assert(dma_buf[0] == 0x00);
    assert(dma_buf[1] == 0x00);

    /* Pad 0 maps to Chain Index 11.
     * Offset in DMA buffer = 2 + 24 * 11 = 266.
     * WS2812 GRB bit order:
     *   Green channel (bits 23..16): 0xFF -> 8 bytes of 0xFC
     *   Red channel (bits 15..8):    0x00 -> 8 bytes of 0xC0
     *   Blue channel (bits 7..0):    0x00 -> 8 bytes of 0xC0
     */
    uint8_t *pad0_encoded = &dma_buf[2 + (24 * 11)];

    /* Green bits */
    for (int i = 0; i < 8; i++) {
        assert(pad0_encoded[i] == 0xFC);
    }
    /* Red bits */
    for (int i = 8; i < 16; i++) {
        assert(pad0_encoded[i] == 0xC0);
    }
    /* Blue bits */
    for (int i = 16; i < 24; i++) {
        assert(pad0_encoded[i] == 0xC0);
    }

    /* Chain Index 0 (Pad 8) was not set -> should be all 0xC0 (black) */
    uint8_t *pad8_encoded = &dma_buf[2 + (24 * 0)];
    for (int i = 0; i < 24; i++) {
        assert(pad8_encoded[i] == 0xC0);
    }

    /* Trailing reset gap (bytes 386 to 547) must all be 0x00 */
    for (size_t i = 386; i < SMC_RGB_DMA_BUFFER_SIZE; i++) {
        assert(dma_buf[i] == 0x00);
    }

    printf("PASS: test_ws2812_grb_serialization\n");
}

static void test_buffer_bounds(void)
{
    printf("Running test_buffer_bounds...\n");
    uint8_t small_buf[100];
    assert(smc_rgb_led_serialize(small_buf, sizeof(small_buf)) == 0);
    assert(smc_rgb_led_serialize(NULL, SMC_RGB_DMA_BUFFER_SIZE) == 0);
    printf("PASS: test_buffer_bounds\n");
}

static void test_bank_palettes(void)
{
    printf("Running test_bank_palettes...\n");
    smc_rgb_led_init();

    /* Bank 0 theme */
    smc_rgb_led_set_bank_theme(0);
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_DIM_BLUE);
    assert(smc_rgb_led_get_bank_palette(0)->hit_color == SMC_COLOR_GREEN);

    /* Bank 1 theme */
    smc_rgb_led_set_bank_theme(1);
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_DIM_AMBER);
    assert(smc_rgb_led_get_bank_palette(1)->hit_color == SMC_COLOR_YELLOW);

    /* Bank 2 theme */
    smc_rgb_led_set_bank_theme(2);
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_DIM_MAGENTA);

    /* Bank 3 theme */
    smc_rgb_led_set_bank_theme(3);
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_DIM_CYAN);

    printf("PASS: test_bank_palettes\n");
}

static void test_shift_status_and_boot_animation(void)
{
    printf("Running test_shift_status_and_boot_animation...\n");
    smc_rgb_led_init();

    /* Render shift status: Preset Pad 5 (Bank B Preset 2), Curve 2 (Hard), Octave -12, Semitone +1 */
    smc_rgb_led_render_shift_status(5, 2, -12, 1);

    /* Row 1 & 2: Presets (Pads 0..7) */
    assert(smc_rgb_led_get_pad(5) == SMC_COLOR_WHITE);      /* Active preset pad 5 is white */
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_DIM_AMBER);  /* Inactive pad 0 is dim amber */
    assert(smc_rgb_led_get_pad(1) == SMC_COLOR_DIM_AMBER);  /* Inactive pad 1 is dim amber */
    assert(smc_rgb_led_get_pad(4) == SMC_COLOR_DIM_AMBER);  /* Inactive pad 4 is dim amber */

    /* Row 3: Velocity Curves (Pads 8..11) */
    assert(smc_rgb_led_get_pad(10) == SMC_COLOR_MAGENTA);     /* Active curve 2 (Pad 10) is bright magenta */
    assert(smc_rgb_led_get_pad(8)  == SMC_COLOR_DIM_MAGENTA); /* Inactive curve 0 (Pad 8) is dim magenta */

    /* Row 4: Transpose (Pads 12..15) */
    assert(smc_rgb_led_get_pad(12) == SMC_COLOR_DIM_CYAN);  /* Semitone -1 not active */
    assert(smc_rgb_led_get_pad(13) == SMC_COLOR_CYAN);      /* Semitone +1 is ACTIVE (semi > 0) */
    assert(smc_rgb_led_get_pad(14) == SMC_COLOR_BLUE);      /* Octave -12 is ACTIVE (octave < 0) */
    assert(smc_rgb_led_get_pad(15) == SMC_COLOR_DIM_BLUE);  /* Octave +12 not active */

    /* Run boot animation in test mode (no delay) */
    smc_rgb_led_boot_animation(smc_rgb_led_set_pad, smc_rgb_led_show, NULL);
    assert(smc_rgb_led_get_pad(0) == SMC_COLOR_BLACK);

    printf("PASS: test_shift_status_and_boot_animation\n");
}

int main(void)
{
    printf("\n=== Running SMC-Pad RGB LED Tests ===\n");
    test_initialization_and_defaults();
    test_pad_mapping_and_colors();
    test_bank_palettes();
    test_shift_status_and_boot_animation();
    test_ws2812_grb_serialization();
    test_buffer_bounds();
    printf("=== All SMC-Pad RGB LED Tests Passed Successfully! ===\n\n");
    return 0;
}
