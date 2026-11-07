## ADDED Requirements

### Requirement: Lock-free event ring buffer
The system SHALL provide a statically allocated Single-Producer Single-Consumer (SPSC) ring buffer for transferring input events from interrupt routines to the main dispatcher without dynamic memory allocation or blocking mutexes.

#### Scenario: Enqueue event from ISR
- **WHEN** a hardware scan ISR produces an input event and space is available in the queue
- **THEN** the event is written to the buffer tail, the tail pointer advances atomically, and true is returned

#### Scenario: Buffer overflow protection
- **WHEN** a hardware scan ISR attempts to push an event into an already full ring buffer
- **THEN** the event is dropped, the buffer pointers remain unchanged, an overflow counter increments, and false is returned

### Requirement: Event dispatch to MIDI engine
The main application loop SHALL dequeue events from the ring buffer and translate them into formatted MIDI packets.

#### Scenario: Dequeue and process MIDI event
- **WHEN** there are pending events in the ring buffer
- **THEN** the main loop pops the event at the head pointer, advances the head atomically, and passes the event to the MIDI dispatch engine
