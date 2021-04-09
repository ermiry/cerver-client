## Client
- Split client_connection_start () into dedicated connection methods
- Checking for connection queue in client_connection_start ()

## Connection
- Added the ability to send packets using a connection queue

## Packets
- Refactored packets stats methods to use the correct types
- Added base packet_send_actual () to send a tcp packet