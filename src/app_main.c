/**
 * @file app_main.c
 * @brief SMC-Pad Clean-Slate Open Firmware Entry Point (JieLi AC6966C / BR25)
 */

#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "matrix.h"
#include "pad_adc.h"
#include "midi_dispatcher.h"

/* Peripheral hardware access macros */
#define REG32(addr) (*(volatile uint32_t *)(addr))

/* Static system state structures (Strict zero dynamic allocation) */
static smc_event_queue_t    g_event_queue;
static smc_encoder_state_t  g_encoder_engine;
static smc_matrix_state_t   g_matrix_engine;
static smc_pad_matrix_t     g_pad_engine;
static smc_midi_dispatcher_t g_midi_engine;

static volatile uint32_t    g_system_ticks = 0;

/* ========================================================================= */
/* Hardware Callbacks (HAL)                                                  */
/* ========================================================================= */

static void hal_sr_clk(bool level) {
    (void)level;
    /* Driven via GPIO or SPI2 DMA on JieLi AC6966C */
}
static void hal_sr_dat(bool level) {
    (void)level;
}
static void hal_sr_lat(bool level) {
    (void)level;
}
static void hal_sr_oe(bool level) {
    (void)level;
}

static bool hal_read_sense0(void) {
    /* Read PB0 (encoders) */
    return false;
}
static bool hal_read_sense1(void) {
    /* Read PC1 (buttons) */
    return false;
}

static void hal_set_led_phase(uint8_t phase, bool active) {
    (void)phase;
    (void)active;
    /* Active-low high-side PNP driver */
}
static void hal_set_led_segment(uint8_t seg, bool active) {
    (void)seg;
    (void)active;
    /* Active-low low-side sinking driver */
}

static void hal_set_mux_channel(uint8_t ch) {
    (void)ch;
    /* Shared S0 (PB2), S1 (PB3), S2 (PC4) */
}
static void hal_settling_delay_us(uint32_t us) {
    /* Microsecond busy loop */
    volatile uint32_t count = us * 6; /* Approx at 24 MHz */
    while (count--) {
        __asm__ __volatile__("" ::: "memory");
    }
}
static uint16_t hal_read_adc_ch3(void) {
    /* PA4 / 4051 #1 COM (Pads 0..7) */
    return 0;
}
static uint16_t hal_read_adc_ch4(void) {
    /* PA6 / 4051 #2 COM (Pads 8..15) */
    return 0;
}

static void hal_send_midi_packet(const smc_midi_raw_msg_t *msg) {
    if (!msg) {
        return;
    }
    /* Send over USB-MIDI endpoint and UART1 (PC5) */
}

/* ========================================================================= */
/* Periodic 1 kHz Timer Tick ISR (Alternating Scan & LED Multiplexing)        */
/* ========================================================================= */

void smc_timer_1khz_isr(void) {
    g_system_ticks++;

    if ((g_system_ticks & 1) == 0) {
        /*
         * Even tick: Matrix and ADC Scan.
         * LED phases are strictly guaranteed OFF to eliminate PC3/PC1 crosstalk.
         */
        smc_matrix_scan_step(&g_matrix_engine, &g_encoder_engine, &g_event_queue, g_system_ticks);
        smc_pad_scan_step(&g_pad_engine, &g_event_queue, g_system_ticks);
    } else {
        /*
         * Odd tick: LED Multiplexing phase advancement.
         */
        smc_matrix_led_step(&g_matrix_engine);
    }
}

/* ========================================================================= */
/* Main Application Entry Point                                              */
/* ========================================================================= */

int main(void) {
    /* 1. Clear bootloader crash counter: indicates firmware booted successfully */
    REG32(SMC_BOOT_COUNT_ADDR) = 0;

    /* 2. Initialize SPSC Ring Buffer */
    smc_queue_init(&g_event_queue);

    /* 3. Initialize Rotary Encoder Engine (2 substeps per click) */
    smc_encoder_init(&g_encoder_engine, 2);

    /* 4. Initialize Key Matrix & LED Scanner */
    smc_matrix_hal_t m_hal = {
        .sr_set_clk = hal_sr_clk,
        .sr_set_dat = hal_sr_dat,
        .sr_set_lat = hal_sr_lat,
        .sr_set_oe  = hal_sr_oe,
        .read_sense0 = hal_read_sense0,
        .read_sense1 = hal_read_sense1,
        .set_led_phase = hal_set_led_phase,
        .set_led_segment = hal_set_led_segment
    };
    smc_matrix_init(&g_matrix_engine, &m_hal);

    /* 5. Initialize Dual-Mux Pad ADC Engine with Auto-Zero Baseline */
    smc_pad_hal_t p_hal = {
        .set_mux_channel = hal_set_mux_channel,
        .settling_delay_us = hal_settling_delay_us,
        .read_adc_ch3 = hal_read_adc_ch3,
        .read_adc_ch4 = hal_read_adc_ch4
    };
    smc_pad_init(&g_pad_engine, &p_hal, 50, 25, 1000, 3, VELOCITY_CURVE_LINEAR);
    smc_pad_calibrate_baseline(&g_pad_engine);

    /* 6. Initialize MIDI Dispatcher */
    smc_midi_init(&g_midi_engine, hal_send_midi_packet);

    /* 7. Main Event Loop (Lock-free consumer) */
    smc_event_t ev;
    while (1) {
        if (smc_queue_pop(&g_event_queue, &ev)) {
            smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
        }
    }

    return 0;
}
