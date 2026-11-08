/**
 * @file app_main.c
 * @brief SMC-Pad Clean-Slate Open Firmware Entry Point (JieLi AC6966C / BR25)
 *
 * Implements a high-performance, low-latency, "dumb" MIDI controller pipeline:
 * - 1 kHz timer scan of 16-column matrix (buttons & encoders with Gray code decoding)
 * - Dual HCF4051 ADC scan with dynamic auto-zero baseline calibration
 * - Single-wire WS2812/SK6812 RGB LED visual feedback at 6 MHz via SPI1 DMA
 * - Lock-free ring buffer between ISRs and main event loop
 * - USB-MIDI Note-On, Note-Off, CC dispatch and hardware Panic protocol
 */

#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "matrix.h"
#include "pad_adc.h"
#include "midi_dispatcher.h"
#include "rgb_led.h"

/* Peripheral hardware access macros */
#define REG32(addr) (*(volatile uint32_t *)(addr))

/* Static system state structures (Strict zero dynamic allocation) */
static smc_event_queue_t     g_event_queue;
static smc_encoder_state_t   g_encoder_engine;
static smc_matrix_state_t    g_matrix_engine;
static smc_pad_matrix_t      g_pad_engine;
static smc_midi_dispatcher_t g_midi_engine;

static volatile uint32_t     g_system_ticks = 0;
static uint8_t               g_pad_bank = 0;
static uint8_t               g_encoder_bank = 0;

/* Visual theme colors */
#define PAD_COLOR_IDLE       SMC_COLOR_DIM_BLUE   /* 0x000020 */
#define PAD_COLOR_HIT        SMC_COLOR_GREEN      /* 0x00FF00 */

/* ========================================================================= */
/* Hardware Callbacks (HAL)                                                  */
/* ========================================================================= */

static void hal_sr_clk(bool level) {
    if (level) {
        REG32(SMC_PORTB_BASE + 0x00) |= (1u << 6);  /* PB6 HIGH */
    } else {
        REG32(SMC_PORTB_BASE + 0x00) &= ~(1u << 6); /* PB6 LOW */
    }
}

static void hal_sr_dat(bool level) {
    if (level) {
        REG32(SMC_PORTB_BASE + 0x00) |= (1u << 7);  /* PB7 HIGH */
    } else {
        REG32(SMC_PORTB_BASE + 0x00) &= ~(1u << 7); /* PB7 LOW */
    }
}

static void hal_sr_lat(bool level) {
    if (level) {
        REG32(SMC_PORTB_BASE + 0x00) |= (1u << 4);  /* PB4 HIGH */
    } else {
        REG32(SMC_PORTB_BASE + 0x00) &= ~(1u << 4); /* PB4 LOW */
    }
}

static void hal_sr_oe(bool level) {
    /* Active-low output enable on PB5 */
    if (level) {
        REG32(SMC_PORTB_BASE + 0x00) &= ~(1u << 5); /* PB5 LOW (Outputs Enabled) */
    } else {
        REG32(SMC_PORTB_BASE + 0x00) |= (1u << 5);  /* PB5 HIGH (Outputs Disabled) */
    }
}

static bool hal_read_sense0(void) {
    /* Read PB0: Sense 0 (Encoders) */
    return (REG32(SMC_PORTB_BASE + 0x04) & (1u << 0)) != 0;
}

static bool hal_read_sense1(void) {
    /* Read PC1: Sense 1 (Buttons) */
    return (REG32(SMC_PORTC_BASE + 0x04) & (1u << 1)) != 0;
}

static void hal_set_led_phase(uint8_t phase, bool active) {
    /*
     * Phase commons are active-low PNP switches:
     * Phase 0: PA3
     * Phase 1: PC2
     * Phase 2: PC3
     */
    if (phase == 0) {
        if (active) REG32(SMC_PORTA_BASE + 0x00) &= ~(1u << 3);
        else        REG32(SMC_PORTA_BASE + 0x00) |=  (1u << 3);
    } else if (phase == 1) {
        if (active) REG32(SMC_PORTC_BASE + 0x00) &= ~(1u << 2);
        else        REG32(SMC_PORTC_BASE + 0x00) |=  (1u << 2);
    } else if (phase == 2) {
        if (active) REG32(SMC_PORTC_BASE + 0x00) &= ~(1u << 3);
        else        REG32(SMC_PORTC_BASE + 0x00) |=  (1u << 3);
    }
}

static void hal_set_led_segment(uint8_t seg, bool active) {
    /*
     * Segments are active-low sinking drivers:
     * Seg 0: PA5
     * Seg 1: PA2
     * Seg 2: PA0
     * Seg 3: PA1
     */
    uint32_t pin_mask = 0;
    if (seg == 0) pin_mask = (1u << 5);
    else if (seg == 1) pin_mask = (1u << 2);
    else if (seg == 2) pin_mask = (1u << 0);
    else if (seg == 3) pin_mask = (1u << 1);

    if (active) {
        REG32(SMC_PORTA_BASE + 0x00) &= ~pin_mask;
    } else {
        REG32(SMC_PORTA_BASE + 0x00) |= pin_mask;
    }
}

static void hal_set_mux_channel(uint8_t ch) {
    /*
     * Shared multiplexer address lines:
     * S0: PB2, S1: PB3, S2: PC4
     */
    if (ch & 1) REG32(SMC_PORTB_BASE + 0x00) |= (1u << 2);
    else        REG32(SMC_PORTB_BASE + 0x00) &= ~(1u << 2);

    if (ch & 2) REG32(SMC_PORTB_BASE + 0x00) |= (1u << 3);
    else        REG32(SMC_PORTB_BASE + 0x00) &= ~(1u << 3);

    if (ch & 4) REG32(SMC_PORTC_BASE + 0x00) |= (1u << 4);
    else        REG32(SMC_PORTC_BASE + 0x00) &= ~(1u << 4);
}

static void hal_settling_delay_us(uint32_t us) {
    volatile uint32_t count = us * 6; /* Approx at 24 MHz */
    while (count--) {
        __asm__ __volatile__("" ::: "memory");
    }
}

static uint16_t hal_read_adc_ch3(void) {
    /* PA4 / 4051 #1 COM (Pads 0..7) */
#ifndef SMC_HOST_TEST
    /* In target runtime, reading hardware SAR ADC register */
    return (uint16_t)(REG32(0x1E0E04u) & 0x03FFu); /* 10-bit SAR ADC */
#else
    return 0;
#endif
}

static uint16_t hal_read_adc_ch4(void) {
    /* PA6 / 4051 #2 COM (Pads 8..15) */
#ifndef SMC_HOST_TEST
    return (uint16_t)(REG32(0x1E0E08u) & 0x03FFu);
#else
    return 0;
#endif
}

static void hal_send_midi_packet(const smc_midi_raw_msg_t *msg) {
    if (!msg || msg->len == 0) {
        return;
    }

#ifndef SMC_HOST_TEST
    /* USB-MIDI Packet: [CIN / Cable, Status, Data1, Data2] */
    uint8_t cin = (msg->bytes[0] >> 4) & 0x0Fu;
    uint8_t packet[4] = {
        cin,
        msg->bytes[0],
        (msg->len > 1) ? msg->bytes[1] : 0,
        (msg->len > 2) ? msg->bytes[2] : 0
    };

    extern int smcpad_usbmidi_write(const uint8_t pkt[4]);
    smcpad_usbmidi_write(packet);
#endif
}

/* ========================================================================= */
/* Periodic 1 kHz Timer Tick ISR (Alternating Scan & LED Multiplexing)        */
/* ========================================================================= */

void smc_timer_1khz_isr(void) {
    g_system_ticks++;

    if ((g_system_ticks & 1) == 0) {
        /*
         * Even tick: Matrix and ADC Scan.
         * LED phases are strictly OFF to eliminate PC3/PC1 capacitance crosstalk.
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
    /* 1. Clear bootloader crash counter */
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

    /* 7. Initialize Pad RGB LEDs & Set Resting Theme */
    smc_rgb_led_init();
    smc_rgb_led_set_all(PAD_COLOR_IDLE);
    smc_rgb_led_show();

    /* 8. Main Event Processing Loop (Lock-Free Consumer) */
    smc_event_t ev;
    while (1) {
        if (smc_queue_pop(&g_event_queue, &ev)) {
            switch (ev.type) {
            case EVENT_PAD_STRIKE:
                /* Illuminate pad in active hit color */
                smc_rgb_led_set_pad(ev.index, PAD_COLOR_HIT);
                smc_rgb_led_show();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
                break;

            case EVENT_PAD_RELEASE:
                /* Restore resting idle color */
                smc_rgb_led_set_pad(ev.index, PAD_COLOR_IDLE);
                smc_rgb_led_show();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
                break;

            case EVENT_ENCODER_DELTA:
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
                break;

            case EVENT_BUTTON_DOWN:
                if (ev.index == BTN_SHIFT) {
                    g_midi_engine.shift_active = true;
                    smc_matrix_set_led(&g_matrix_engine, LED_SHIFT, true);
                } else if (ev.index == BTN_PAUSE && g_midi_engine.shift_active) {
                    /* Shift + Pause = Panic (All Notes Off) */
                    smc_midi_panic(&g_midi_engine);
                    smc_rgb_led_set_all(PAD_COLOR_IDLE);
                    smc_rgb_led_show();
                } else if (ev.index == BTN_PAD_BANK) {
                    /* Cycle pad bank 0..3 */
                    g_pad_bank = (g_pad_bank + 1) & 3;
                    smc_matrix_set_led(&g_matrix_engine, LED_PAD_BANK, (g_pad_bank > 0));
                } else if (ev.index == BTN_KNOB_BANK) {
                    /* Cycle encoder bank 0..3 */
                    g_encoder_bank = (g_encoder_bank + 1) & 3;
                    smc_encoder_set_bank(&g_encoder_engine, g_encoder_bank);
                    smc_matrix_set_led(&g_matrix_engine, LED_KNOB_BANK, (g_encoder_bank > 0));
                }
                break;

            case EVENT_BUTTON_UP:
                if (ev.index == BTN_SHIFT) {
                    g_midi_engine.shift_active = false;
                    smc_matrix_set_led(&g_matrix_engine, LED_SHIFT, false);
                }
                break;

            default:
                break;
            }
        }
    }

    return 0;
}
