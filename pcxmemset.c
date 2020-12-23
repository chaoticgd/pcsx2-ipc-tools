#include "ipc.h"

int main(int argc, char **argv)
{
	if(argc != 4) {
		fprintf(stderr, "usage: %s <address in hex> <value in hex> <size in hex>\n", argv[0]);
		return 1;
	}
	PS2Ptr dest = strtol(argv[1], NULL, 16);
	uint8_t value = strtol(argv[2], NULL, 16);
	uint32_t size = strtol(argv[3], NULL, 16);
	ipc_begin();
	ipc_memset(dest, value, size);
	ipc_send();
}
