# C++ Multiplayer Server
WIP - to be updated

## Part A: foundation (DONE)
- Player/client IDs and welcome msg packet handshake with join retries
- Input validation(is_valid_input, bad_packets counter)
- Disconnect handling and cleanup
- Sequence logic(sequence_newer, stale and dupe rejection)
- Ping/rtt
- Server silence handle on client side

## Part B: game
- Game rules and movement logic
- Client-side prediction and reconcilliation
- Interpolation? <- we're here
- ...