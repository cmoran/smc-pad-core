## Context

The SMC-Pad hardware provides dedicated panel controls for the Note Repeat engine:
- Direct Pad selection via `Shift + Note Repeat` mode (Pads 1-16)
- Real-time adjustment via Knobs 1-4 while holding the `Note Repeat` button

## Goals / Non-Goals

**Goals:**
- Implement dynamic tempo scaling across 30..300 BPM (default 120 BPM).
- Implement swing shuffle (50% to 70%).
- Implement Latch mode allowing pads to sustain rhythmic repetition hands-free.
- Implement Tap Tempo on Pad 16 with visual tempo flash.
- Add rotary encoder step accumulation for Knob 1 to prevent abrupt rate jumping.
- Fully populate the 16-pad configuration matrix in `Shift + Note Repeat` mode.

**Non-Goals:**
- External MIDI Clock slave synchronization over 5-pin DIN (reserved for future MIDI RX sync change).

## Decisions

### 1. Dynamic Period Calculation
- Period for division $i$ computed from base tempo:
  - 1/4 = $60000 / BPM$
  - 1/4t = $(60000 * 2) / (BPM * 3)$
  - 1/8 = $30000 / BPM$
  - 1/8t = $20000 / BPM$
  - 1/16 = $15000 / BPM$
  - 1/16t = $10000 / BPM$
  - 1/32 = $7500 / BPM$
  - 1/32t = $5000 / BPM$
- Gate length maintains 60% duty cycle, ensuring host synth envelopes trigger cleanly at all BPMs.

### 2. Swing Shuffle State Machine
- Swing is applied on alternating pulses using a step flip-flop:
  - Even pulses: $period_{even} = (period * 2 * swing\_pct) / 100$
  - Odd pulses: $period_{odd} = (period * 2 * (100 - swing\_pct)) / 100$
  - Average period remains constant.

### 3. Latch Architecture
- Active repeat mask is $M = (held\_mask \mid latched\_mask)$.
- Releasing a pad clears $held\_mask$, but keeps the pad active in repeat if bit is present in $latched\_mask$.
- Re-striking a latched pad removes it from $latched\_mask$ (toggle behavior).

### 4. Knob Step Accumulation
- Knob 1 (Rate) and Knob 2 (Swing) use signed accumulators requiring 2 physical quadrature clicks per parameter increment, providing tactile and deliberate control.

## Risks / Trade-offs

- **[Risk]** Very high BPM (300) with 1/32t division produces 16 ms periods (10 ms gate).
  - **Mitigation:** Minimum gate duration clamped to 12 ms, well within JieLi 250 µs tick resolution.
