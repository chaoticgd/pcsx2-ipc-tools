#include "ipc.h"

int main(int argc, char **argv)
{
	if(argc != 4) {
		fprintf(stderr, "usage: %s <start address in hex> <end address in hex> <value in hex>\n", argv[0]);
		return 1;
	}
	PS2Ptr start = strtol(argv[1], NULL, 16);
	uint32_t end = strtol(argv[2], NULL, 16);
	uint8_t value = strtol(argv[3], NULL, 16);
	ipc_begin();
	ipc_memset(start, value, end - start);
	ipc_send();
}
