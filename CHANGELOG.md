## General
- Removed game sources as they were outdated
- Added dedicated header with auth related definitions
- Updated files related methods with latest implementations
- Refactored cerver information handler methods
- Removed previous json utilities methods
- Added latest custom json sources from cerver

## Connection
- Updated connection methods with latest available methods
- Added the ability to send packets using a connection queue

## Handler
- Removed SockReceive structure & related methods
- Added receive related definitions in dedicated sources
- Added latest handler methods implementations

## Packets
- Refactored packet header field to be static instead of a pointer
- Updated packets sources with latest methods implementations
- Added base packet_send_actual () to send a tcp packet

## Threads
- Added latest thread pool implementation in threads sources
- Added latest bsem & job queue implementations
- Updated thread_set_name () implementation
- Added latest jobs & queue definitions & methods

## Tests
- Added latest dedicated json methods unit tests
- Added latest threads units tests methods
