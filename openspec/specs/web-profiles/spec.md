# web-profiles Specification

## Purpose
Persistent named profile management, JSON import/export, and musical setup presets in WebMIDI configurator.

## Requirements

### Requirement: Named Profile Management in WebMIDI Configurator
The WebMIDI configurator application SHALL provide a Profile Manager that enables users to create, name, switch, duplicate, delete, and persist complete device configurations in browser local storage.

#### Scenario: Saving a new profile
- **WHEN** a user enters a profile name (e.g. "Future Funk Setup") and clicks "Save Profile"
- **THEN** the entire active configuration (pads, encoders, velocity curves, RGB palettes) is stored in browser local storage and added to the profile selector.

### Requirement: JSON Profile Export and Import
The WebMIDI configurator application SHALL support exporting saved profiles to standalone JSON files and importing external profile JSON files.

#### Scenario: Exporting profiles to JSON file
- **WHEN** the user clicks "Export Profiles"
- **THEN** a `.json` file containing the user's named profiles is generated and downloaded by the browser.

#### Scenario: Importing profiles from JSON file
- **WHEN** the user selects a valid profile `.json` file via "Import Profiles"
- **THEN** the profiles are parsed, validated, and merged into the available profile list.

### Requirement: Built-in Genre and Workflow Presets
The WebMIDI configurator application SHALL include pre-configured factory profiles representing distinct musical genres and workflows.

#### Scenario: Loading a factory preset
- **WHEN** the user selects "Future Funk Setup" or "Cumbia Setup" from the profile list
- **THEN** the configurator populates all pad notes, channels, encoder mappings, and RGB palettes according to the preset definition.
