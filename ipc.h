#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef uint32_t PS2Ptr; // Use this for addresses in EE memory.

void ipc_begin(); // Reset the command buffer.
void ipc_send(); // Send the command list to PCSX2 and handle responses.

// Read/write PS2 memory.
void ipc_read8(uint8_t *dest, PS2Ptr src);
void ipc_read16(uint16_t *dest, PS2Ptr src);
void ipc_read32(uint32_t *dest, PS2Ptr src);
void ipc_read64(uint64_t *dest, PS2Ptr src);
void ipc_write8(PS2Ptr dest, uint8_t value);
void ipc_write16(PS2Ptr dest, uint16_t value);
void ipc_write32(PS2Ptr dest, uint32_t value);
void ipc_write64(PS2Ptr dest, uint64_t value);

void ipc_read(void *dest, PS2Ptr src, uint32_t size);
void ipc_write(PS2Ptr dest, uint8_t *src, uint32_t size);
void ipc_memset(PS2Ptr dest, uint8_t value, uint32_t size);
