## General
- Updated makefile to handle compilation types
- added dedicated workflows to build different sources types
- Added workflow to update wiki with documentation
- Added base codecov configuration
- Updated main headers with extern "C" modifier


## Client / Connection
- Refactor client events & errors structures definitions
- Using new client event & errors definitions in main client header
- Split client_receive () to be able to use an already allocated buffer
- Removed extra check in client_receive_handle_buffer ()
- Refactored client_connection_get_next_packet ()
- Refactor connection_update () to allocate a packet buffer

## Utilities
- Added new implementation of base64 using avx
- Updated utilities with latest implementations

## Tests
- Added test header with custom macros & definitions
- Added base collections & utilities unit tests