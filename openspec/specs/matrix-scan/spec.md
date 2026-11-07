# matrix-scan Specification

## Purpose
TBD - created by archiving change foundation-core. Update Purpose after archive.
## Requirements
### Requirement: Shift register matrix scanning
The matrix scanner SHALL drive the 16 columns of the button and encoder matrix using the two cascaded 74HC595 shift registers (PB6 SRCLK, PB7 SER, PB4 RCLK, PB5 /OE) and sample the return lines PB0 (Sense 0, encoders) and PC1 (Sense 1, buttons).

#### Scenario: Full matrix column cycle
- **WHEN** the timer tick initiates a matrix scan pass
- **THEN** a walking active bit is clocked sequentially through all 16 shift register outputs and the corresponding state of PB0 and PC1 is captured for each column

### Requirement: Button debouncing
The system SHALL debounce all 10 function button inputs using an integrator or counter state machine to prevent mechanical contact bounce.

#### Scenario: Button press debounced
- **WHEN** a button column transitions to the pressed state and remains stable for the debounce qualification period
- **THEN** a button press event with the button ID is queued to the event pipeline

#### Scenario: Button release debounced
- **WHEN** a previously pressed button transitions to the released state and remains stable for the debounce qualification period
- **THEN** a button release event with the button ID is queued to the event pipeline

### Requirement: Non-interfering LED multiplexing
The button LED driver SHALL deactivate all LED drive phases (PA3, PC2, PC3) before and during the button matrix sense line (PC1) sampling window.

#### Scenario: Safe matrix read during LED multiplexing
- **WHEN** the matrix scanner prepares to sample column return lines on PC1
- **THEN** phase 2 (PC3) and all other LED phase rails are driven inactive high, ensuring zero electrical crosstalk onto PC1

