## ADDED Requirements

### Requirement: Per-Bank RGB Color Palettes
The RGB LED subsystem SHALL support distinct resting (idle) and active (strike) color palettes for each of the 4 virtual banks, refreshing the entire 4x4 pad matrix immediately when the active bank changes.

#### Scenario: Switching pad bank refreshes palette
- **GIVEN** Bank 0 uses Dim Blue idle color and Bank 1 uses Dim Amber idle color
- **WHEN** the user cycles active Pad Bank from Bank 0 to Bank 1
- **THEN** all 16 pads on the matrix immediately transition to the Dim Amber idle color.
