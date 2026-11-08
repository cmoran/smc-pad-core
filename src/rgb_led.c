/**
 * @file rgb_led.c
 * @brief Clean-slate WS2812/SK6812 RGB LED Driver Implementation
 */

#include "rgb_led.h"
#include <string.h>

#define REG32(a)            (*(volatile uint32_t *)(a))

#define HW_PORTB_OUT        REG32(SMC_PORTB_BASE + 0x00)
#define HW_PORTB_DIR        REG32(SMC_PORTB_BASE + 0x08)
#define HW_PORTB_DIE        REG32(SMC_PORTB_BASE + 0x0C)
#define HW_PORTB_PU         REG32(SMC_PORTB_BASE + 0x10)
#define HW_PORTB_PD         REG32(SMC_PORTB_BASE + 0x14)

#define HW_IOMAP_CON1       REG32(SMC_IOMAP_CON1)

#define HW_SPI1_CON         REG32(SMC_SPI1_CON)
#define HW_SPI1_BAUD        REG32(SMC_SPI1_BAUD)
#define HW_SPI1_ADR         REG32(SMC_SPI1_ADR)
#define HW_SPI1_CNT         REG32(SMC_SPI1_CNT)

/*
 * Hardware mapping: Logical Pad (0..15) -> Physical WS2812 Chain Index (0..15)
 * Pad layout is 4 rows of 4:
 *   [ 0  1  2  3 ]
 *   [ 4  5  6  7 ]
 *   [ 8  9 10 11 ]
 *   [12 13 14 15 ]
 */
const uint8_t smc_pad_to_led_map[SMC_RGB_NUM_PADS] = {
    11,  4,  2,  7,
    10, 14,  6,  9,
     0,  5, 15,  1,
    13, 12,  8,  3
};

/* Virtual Bank Color Palettes:
 * Bank 0: Dim Blue idle, Bright Green hit (GM Drums)
 * Bank 1: Dim Amber idle, Bright Yellow hit
 * Bank 2: Dim Magenta idle, Bright Cyan hit
 * Bank 3: Dim Cyan idle, Bright White hit
 */
const smc_bank_palette_t smc_bank_palettes[4] = {
    { SMC_COLOR_DIM_BLUE,    SMC_COLOR_GREEN },
    { SMC_COLOR_DIM_AMBER,   SMC_COLOR_YELLOW },
    { SMC_COLOR_DIM_MAGENTA, SMC_COLOR_CYAN },
    { SMC_COLOR_DIM_CYAN,    SMC_COLOR_WHITE }
};

const smc_bank_palette_t *smc_rgb_led_get_bank_palette(uint8_t bank)
{
    return &smc_bank_palettes[bank & 0x03u];
}

void smc_rgb_led_set_bank_theme(uint8_t bank)
{
    const smc_bank_palette_t *pal = smc_rgb_led_get_bank_palette(bank);
    smc_rgb_led_set_all(pal->idle_color);
}

/* State variables (zero dynamic allocation) */
static uint32_t s_chain_rgb[SMC_RGB_NUM_PADS];
static uint8_t s_dma_buf[SMC_RGB_DMA_BUFFER_SIZE] __attribute__((aligned(4)));
static bool s_initialized = false;
static bool s_sending = false;
static bool s_dirty = false;
static uint32_t s_lsb_clock_hz = 48000000u; /* Default 48 MHz system LSB */

void smc_rgb_led_set_clock(uint32_t lsb_clock_hz)
{
    if (lsb_clock_hz >= SMC_RGB_SPI_BAUD_HZ) {
        s_lsb_clock_hz = lsb_clock_hz;
#ifndef SMC_HOST_TEST
        HW_SPI1_BAUD = (s_lsb_clock_hz / SMC_RGB_SPI_BAUD_HZ) - 1;
#endif
    }
}

void smc_rgb_led_init(void)
{
    memset(s_chain_rgb, 0, sizeof(s_chain_rgb));
    memset(s_dma_buf, 0, sizeof(s_dma_buf));
    s_dirty = true;
    s_sending = false;

#ifndef SMC_HOST_TEST
    /* PB1 digital output drive: Pin 1 */
    HW_PORTB_DIR &= ~(1u << 1); /* 0 = output */
    HW_PORTB_DIE |=  (1u << 1); /* 1 = digital input/output enable */
    HW_PORTB_PU  &= ~(1u << 1); /* no pull-up */
    HW_PORTB_PD  &= ~(1u << 1); /* no pull-down */
    HW_PORTB_OUT &= ~(1u << 1); /* drive low initially */

    /* Map SPI1 to Port A (DO = PB1) */
    HW_IOMAP_CON1 &= ~SMC_IOMAP_SPI1_PORT_A_MASK;

    /* Master mode, MSB first, DMA enabled */
    HW_SPI1_CON = 0x4021;

    /* Baud divisor: SPICK = clk / (BAUD + 1) => BAUD = clk / 6000000 - 1 */
    HW_SPI1_BAUD = (s_lsb_clock_hz / SMC_RGB_SPI_BAUD_HZ) - 1;
#endif

    s_initialized = true;
}

void smc_rgb_led_set_pad(uint8_t pad_idx, uint32_t rgb)
{
    if (pad_idx < SMC_RGB_NUM_PADS) {
        uint8_t chain_idx = smc_pad_to_led_map[pad_idx];
        if (s_chain_rgb[chain_idx] != (rgb & 0xFFFFFFu)) {
            s_chain_rgb[chain_idx] = rgb & 0xFFFFFFu;
            s_dirty = true;
        }
    }
}

void smc_rgb_led_set_all(uint32_t rgb)
{
    uint32_t clean_rgb = rgb & 0xFFFFFFu;
    for (size_t i = 0; i < SMC_RGB_NUM_PADS; i++) {
        s_chain_rgb[i] = clean_rgb;
    }
    s_dirty = true;
}

uint32_t smc_rgb_led_get_pad(uint8_t pad_idx)
{
    if (pad_idx < SMC_RGB_NUM_PADS) {
        uint8_t chain_idx = smc_pad_to_led_map[pad_idx];
        return s_chain_rgb[chain_idx];
    }
    return 0;
}

size_t smc_rgb_led_serialize(uint8_t *dest_buf, size_t max_len)
{
    if (dest_buf == NULL || max_len < SMC_RGB_DMA_BUFFER_SIZE) {
        return 0;
    }

    /* Zero the entire frame: 2 lead bytes + 162 trailing reset gap bytes are zero */
    memset(dest_buf, 0, SMC_RGB_DMA_BUFFER_SIZE);

    /* Encode 16 LEDs: 24 bytes each in GRB order */
    for (size_t i = 0; i < SMC_RGB_NUM_PADS; i++) {
        uint32_t c = s_chain_rgb[i];
        uint32_t r = (c >> 16) & 0xFFu;
        uint32_t g = (c >> 8) & 0xFFu;
        uint32_t b = c & 0xFFu;

        /* GRB ordering */
        uint32_t grb = (g << 16) | (r << 8) | b;
        uint8_t *p = dest_buf + 2 + (24 * i);

        for (int bit = 23; bit >= 0; bit--) {
            *p++ = ((grb >> bit) & 1u) ? 0xFCu : 0xC0u;
        }
    }

    return SMC_RGB_DMA_BUFFER_SIZE;
}

bool smc_rgb_led_is_busy(void)
{
#ifndef SMC_HOST_TEST
    if (s_sending) {
        /* Check BIT(15) of SPI1_CON: pending flag set when DMA completes */
        if (HW_SPI1_CON & (1u << 15)) {
            HW_SPI1_CON |= (1u << 14); /* Clear pending */
            s_sending = false;
        }
    }
#endif
    return s_sending;
}

void smc_rgb_led_show(void)
{
    if (!s_initialized) {
        smc_rgb_led_init();
    }

    /* Wait for previous frame if still transmitting (~0.73 ms) */
#ifndef SMC_HOST_TEST
    while (s_sending && !(HW_SPI1_CON & (1u << 15))) {
    }
    HW_SPI1_CON |= (1u << 14); /* Clear pending */
    s_sending = false;
#endif

    smc_rgb_led_serialize(s_dma_buf, sizeof(s_dma_buf));

#ifndef SMC_HOST_TEST
    HW_SPI1_ADR = (uint32_t)s_dma_buf;
    HW_SPI1_CNT = SMC_RGB_DMA_BUFFER_SIZE;
    s_sending = true;
#endif

    s_dirty = false;
}

void smc_rgb_led_render_shift_status(uint8_t preset_pad,
                                     uint8_t velocity_curve,
                                     int8_t octave_offset,
                                     int8_t semitone_offset)
{
    /* Rows 1 & 2 (Pads 0..7): Presets / Bank selection
     * Row 1 (Pads 0..3): Presets 1..4 (Bank A)
     * Row 2 (Pads 4..7): Presets 1..4 (Bank B)
     */
    for (uint8_t p = 0; p < 8; p++) {
        uint32_t color = (p == (preset_pad & 0x07u)) ? SMC_COLOR_WHITE : SMC_COLOR_DIM_AMBER;
        smc_rgb_led_set_pad(p, color);
    }

    /* Row 3 (Pads 8..11): Velocity Curves (0: Linear, 1: Soft, 2: Hard, 3: Fixed) */
    for (uint8_t c = 0; c < 4; c++) {
        uint8_t pad = 8 + c;
        uint32_t color = (c == (velocity_curve & 0x03u)) ? SMC_COLOR_MAGENTA : SMC_COLOR_DIM_MAGENTA;
        smc_rgb_led_set_pad(pad, color);
    }

    /* Row 4: Transposition (Pads 12..15)
     * Pad 12: Semitone -1
     * Pad 13: Semitone +1
     * Pad 14: Octave -12
     * Pad 15: Octave +12
     */
    smc_rgb_led_set_pad(12, (semitone_offset < 0) ? SMC_COLOR_CYAN : SMC_COLOR_DIM_CYAN);
    smc_rgb_led_set_pad(13, (semitone_offset > 0) ? SMC_COLOR_CYAN : SMC_COLOR_DIM_CYAN);
    smc_rgb_led_set_pad(14, (octave_offset < 0)   ? SMC_COLOR_BLUE : SMC_COLOR_DIM_BLUE);
    smc_rgb_led_set_pad(15, (octave_offset > 0)   ? SMC_COLOR_BLUE : SMC_COLOR_DIM_BLUE);
}

void smc_rgb_led_boot_animation(void (*set_fn)(uint8_t pad, uint32_t color),
                                void (*show_fn)(void),
                                void (*delay_fn)(uint32_t ms))
{
    static const uint8_t center_spiral[4] = { 5, 6, 10, 9 };
    static const uint8_t perimeter[12] = { 4, 8, 12, 13, 14, 15, 11, 7, 3, 2, 1, 0 };
    static const uint32_t spiral_colors[12] = {
        0x00E0FF, 0x20A0FF, 0x5060FF, 0x8020FF,
        0xB000FF, 0xE000E0, 0xFF00B0, 0xFF0070,
        0xFF0040, 0xFF3010, 0xFF8000, 0x00FF80
    };

    /* 1. Clear matrix */
    for (uint8_t p = 0; p < SMC_RGB_NUM_PADS; p++) {
        smc_rgb_led_set_pad(p, SMC_COLOR_BLACK);
        if (set_fn) set_fn(p, SMC_COLOR_BLACK);
    }
    if (show_fn) show_fn();

    /* 2. Center 4 pads spiral ignite (60 ms each = 240 ms) */
    for (uint8_t i = 0; i < 4; i++) {
        smc_rgb_led_set_pad(center_spiral[i], 0x00FFFF);
        if (set_fn) set_fn(center_spiral[i], 0x00FFFF);
        if (show_fn) show_fn();
        if (delay_fn) delay_fn(60);
    }

    /* 3. Outer perimeter chase in clockwise spiral (50 ms each = 600 ms) */
    for (uint8_t i = 0; i < 12; i++) {
        smc_rgb_led_set_pad(perimeter[i], spiral_colors[i]);
        if (set_fn) set_fn(perimeter[i], spiral_colors[i]);
        if (show_fn) show_fn();
        if (delay_fn) delay_fn(50);
    }

    /* Hold full spiral for 150 ms */
    if (delay_fn) delay_fn(150);

    /* 4. Matrix flare pulse (120 ms) */
    for (uint8_t p = 0; p < SMC_RGB_NUM_PADS; p++) {
        smc_rgb_led_set_pad(p, SMC_COLOR_WHITE);
        if (set_fn) set_fn(p, SMC_COLOR_WHITE);
    }
    if (show_fn) show_fn();
    if (delay_fn) delay_fn(120);

    /* 5. Clear before bank transition (80 ms) */
    for (uint8_t p = 0; p < SMC_RGB_NUM_PADS; p++) {
        smc_rgb_led_set_pad(p, SMC_COLOR_BLACK);
        if (set_fn) set_fn(p, SMC_COLOR_BLACK);
    }
    if (show_fn) show_fn();
    if (delay_fn) delay_fn(80);
}
