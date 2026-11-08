/**
 * @file smc_hw.h
 * @brief SMC-Pad Hardware Register & Pin Definitions for JieLi AC6966C (BR25)
 *
 * Board: SK12-V2.1 2024/04/29
 * SoC: JieLi AC6966C (pi32v2 CPU core, 24 MHz crystal, 4 Mbit internal SPI flash)
 */

#ifndef SMC_HW_H
#define SMC_HW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Memory & Flash Layout Constants                                           */
/* ========================================================================= */

#define SMC_SFC_FLASH_BASE          0x1000u
#define SMC_SFC_CPU_BASE            0x1E00000u
#define SMC_APP_DIR                 0x9000u
#define SMC_APP_AREA_END            0x71000u
#define SMC_APP_AREA_MAX            (SMC_APP_AREA_END - SMC_APP_DIR)
#define SMC_APP_ENTRY               (SMC_SFC_CPU_BASE + SMC_APP_DIR - SMC_SFC_FLASH_BASE + 0x120)

/* Bootloader crash guard and DFU handshake (RAM registers surviving soft reset) */
#define SMC_BOOT_MAGIC_ADDR         0x31AE0u
#define SMC_BOOT_MAGIC_DFU          0xB007DF00u
#define SMC_BOOT_COUNT_ADDR         0x31AD8u
#define SMC_BOOT_COUNT_CHECK        0x5A5AB007u

/* ========================================================================= */
/* GPIO Register Offsets (JieLi BR25 Port mapping)                           */
/* PORTA @ 0x1E5000, PORTB @ 0x1E5040 (+0x40), PORTC @ 0x1E5080 (+0x80)      */
/* ========================================================================= */

#define SMC_PORTA_BASE              0x1E5000u
#define SMC_PORTB_BASE              0x1E5040u
#define SMC_PORTC_BASE              0x1E5080u

/* ========================================================================= */
/* SPI Hardware Registers                                                    */
/* ========================================================================= */

/* SPI1 Controller Registers (Pad RGB WS2812 single-wire DMA on PB1) */
#define SMC_SPI1_CON                0x1E1D00u
#define SMC_SPI1_BAUD               0x1E1D04u
#define SMC_SPI1_BUF                0x1E1D08u
#define SMC_SPI1_ADR                0x1E1D0Cu
#define SMC_SPI1_CNT                0x1E1D10u

/* I/O Mapping Registers */
#define SMC_IOMAP_CON1              0x1E531Cu   /* JL_IOMAP->CON1 */
#define SMC_IOMAP_SPI1_PORT_A_MASK  (1u << 4)   /* Bit 4: 0 = Port A (DO=PB1), 1 = Port B (DO=PC5) */

/* ========================================================================= */
/* Physical Hardware Pin Definitions                                         */
/* ========================================================================= */

/* 74HC595 Shift Registers (Matrix Column Drive) */
#define PIN_SR_CLK                  "PB6"   /* SPI2 CLK, map A */
#define PIN_SR_DAT                  "PB7"   /* SPI2 DO, map A  */
#define PIN_SR_LAT                  "PB4"   /* Shift register latch (RCLK) */
#define PIN_SR_OE                   "PB5"   /* Shift register /OE or 4051 INH */

/* Matrix Sense Lines */
#define PIN_SENSE_ENCODERS          "PB0"   /* Sense line 0 (8 encoder quadrature pairs) */
#define PIN_SENSE_BUTTONS           "PC1"   /* Sense line 1 (10 function buttons) */

/* Dual HCF4051 8-Channel Analog Multiplexers */
#define PIN_MUX_S0                  "PB2"   /* Shared channel select S0 */
#define PIN_MUX_S1                  "PB3"   /* Shared channel select S1 */
#define PIN_MUX_S2                  "PC4"   /* Shared channel select S2 */
#define PIN_ADC_MUX1_COM            "PA4"   /* 4051 #1 COM -> ADC Ch 3 (Pads 0-7) */
#define PIN_ADC_MUX2_COM            "PA6"   /* 4051 #2 COM -> ADC Ch 4 (Pads 8-15) */

/* Button Status LEDs (3 phases x 4 segments) */
#define PIN_LED_PHASE_0             "PA3"   /* High-side active-low common */
#define PIN_LED_PHASE_1             "PC2"   /* High-side active-low common */
#define PIN_LED_PHASE_2             "PC3"   /* High-side active-low common (Note: drags PC1 low if driven!) */

#define PIN_LED_SEG_0               "PA5"   /* Low-side active-low sink */
#define PIN_LED_SEG_1               "PA2"   /* Low-side active-low sink */
#define PIN_LED_SEG_2               "PA0"   /* Low-side active-low sink */
#define PIN_LED_SEG_3               "PA1"   /* Low-side active-low sink */

/* Pad RGB LEDs */
#define PIN_PAD_RGB_DATA            "PB1"   /* WS2812B/SK6812 single wire data (SPI1 DO) */

/* MIDI Output */
#define PIN_MIDI_OUT_TX             "PC5"   /* UART1 TX via IO mux */

/* ========================================================================= */
/* Key Matrix Column Mapping (16 Columns)                                    */
/* ========================================================================= */

/* Sense Line 1: Function Button Indices */
typedef enum {
    BTN_SHIFT       = 0,
    BTN_NOTE_REPEAT = 1,
    BTN_BLUETOOTH   = 2,
    BTN_PAD_BANK    = 3,
    BTN_KNOB_BANK   = 4,
    BTN_LEFT_ARROW  = 5,
    BTN_RIGHT_ARROW = 6,
    BTN_PLAY        = 7,
    BTN_REC         = 8,
    BTN_PAUSE       = 9,
    BTN_COUNT       = 10
} smc_button_id_t;

/* Button LED IDs (Mapped to Phase & Segment) */
typedef enum {
    LED_LEFT_ARROW  = 0, /* Phase 1, Seg 0 */
    LED_RIGHT_ARROW = 1, /* Phase 0, Seg 3 */
    LED_PLAY        = 2, /* Phase 1, Seg 2 */
    LED_PAUSE       = 3, /* Phase 0, Seg 1 */
    LED_REC         = 4, /* Phase 1, Seg 1 */
    LED_BLUETOOTH   = 5, /* Phase 2, Seg 2 */
    LED_PAD_BANK    = 6, /* Phase 2, Seg 1 */
    LED_KNOB_BANK   = 7, /* Phase 2, Seg 0 */
    LED_SHIFT       = 8, /* Phase 0, Seg 2 */
    LED_NOTE_REPEAT = 9, /* Phase 0, Seg 0 */
    LED_COUNT       = 10
} smc_led_id_t;

#define SMC_NUM_PADS                16
#define SMC_NUM_ENCODERS            8
#define SMC_NUM_BUTTONS             10
#define SMC_NUM_BANKS               4

#ifdef __cplusplus
}
#endif

#endif /* SMC_HW_H */
