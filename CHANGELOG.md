## General
- Refactored build workflow to compile a common shared object
- Updated makefile to handle compilation types
- Added workflow to test production integration
- Added workflow to create release on success merge with master
- Added script to compile sources with production flags
- Added beta workflow to test integration with beta branches
- Added workflow to update wiki with documentation
- Added base codecov configuration
- Updated main headers with extern "C" modifier

## Client
- Refactored client_connection_get_next_packet ()

## Utilities
- Added new implementation of base64 using avx
- Updated sha256 sources with latest methods
- Updated base64 sources with new implementations
- Updated log sources with latest methods & values
- Updated json header & source with latest configurations

## Tests
- Added test header with custom macros & definitions
- Added base collections & utilities unit tests