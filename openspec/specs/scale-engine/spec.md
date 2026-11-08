# scale-engine Specification

## Purpose
Provides musical key and scale quantization with active RGB key illumination across the 16 physical pads.

## Requirements

### Requirement: Musical Scale and Key Quantization
The system SHALL provide musical scale quantization that maps the 16 pads monotonically across degrees of a selected Root Note and Scale Type (Major, Natural Minor, Dorian, Mixolydian, Minor Pentatonic, Major Pentatonic, Blues).

#### Scenario: Quantizing pads to C Major Pentatonic
- **GIVEN** Root Note is set to C (0) and Scale is set to Major Pentatonic (intervals 0, 2, 4, 7, 9)
- **WHEN** Pad 0 through Pad 4 are struck
- **THEN** notes C, D, E, G, and A are emitted consecutively across octaves without any out-of-scale chromatic notes.

### Requirement: Visual RGB Scale Key Illumination
The RGB LED matrix SHALL visually reflect scale membership, rendering Root notes in bright Gold/Amber, in-scale notes in bright Cyan, and out-of-scale notes in dim tones.

#### Scenario: Rendering scale colors on pad matrix
- **WHEN** Scale mode is active with Root Note C
- **THEN** all pads mapped to note C glow bright Gold/Amber, other scale tones glow Cyan, and non-scale pads remain dim.
