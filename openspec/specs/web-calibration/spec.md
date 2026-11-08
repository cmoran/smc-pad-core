# web-calibration Specification

## Purpose
Provides an interactive 60 FPS HTML5 canvas oscilloscope and automated 3-strike calibration assistant in the WebMIDI configurator.

## Requirements

### Requirement: Interactive Real-Time Oscilloscope
The WebMIDI configurator SHALL include an HTML5 canvas oscilloscope that renders live strike curves, peak pressure, and threshold boundary lines for pads as they are physically struck.

#### Scenario: Visualizing pad strike waveform
- **WHEN** incoming Note-On or pressure events arrive from a struck pad
- **THEN** the oscilloscope canvas renders the velocity pulse, peak indicator, and current baseline threshold in real time.

### Requirement: Automated 3-Strike Sensitivity Calibration Wizard
The WebMIDI configurator SHALL provide a step-by-step calibration assistant that prompts the user to strike physical pads to automatically compute optimal thresholds.

#### Scenario: Running calibration wizard
- **WHEN** the user initiates the 3-strike calibration wizard and strikes the requested pad
- **THEN** the configurator records the dynamic range, sets the custom noise floor and strike threshold, and prepares the calibration packet for upload.
