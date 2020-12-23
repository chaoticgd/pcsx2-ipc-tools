// Handles communication with PCSX2. This is made possible by GovanifY's IPC
// implementation.
//
// For reference see:
//  https://github.com/PCSX2/pcsx2/pull/3591

#include "ipc.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// Uncomment this to print out a hexdump of each command buffer that gets sent.
//#define IPC_DEBUG

#define MAX_IPC_SIZE 650000
#define MAX_IPC_RETURN_SIZE 450000

#define IPC_OK   0
#define IPC_FAIL 0xff

#define MSG_READ8   0
#define MSG_READ16  1
#define MSG_READ32  2
#define MSG_READ64  3
#define MSG_WRITE8  4
#define MSG_WRITE16 5
#define MSG_WRITE32 6
#define MSG_WRITE64 7

static uint8_t *ipc_buffer = NULL;
static uint8_t *ipc_buffer_top = NULL;
static uint8_t *ipc_return_buffer = NULL;
static uint8_t **ipc_dest_buffer = NULL; // Store destination pointers for the read commands.
static uint8_t **ipc_dest_buffer_top = NULL;

static void _ipc_check_overflow();
static uint32_t _ipc_payload_size(uint8_t flag); // e.g. _ipc_payload_size(MSG_READ32) == 4

void ipc_begin()
{
	if(!ipc_buffer) {
		ipc_buffer = malloc(MAX_IPC_SIZE);
	}
	if(!ipc_return_buffer) {
		ipc_return_buffer = malloc(MAX_IPC_RETURN_SIZE);
	}
	if(!ipc_dest_buffer) {
		ipc_dest_buffer = malloc(2 * MAX_IPC_SIZE);
	}
	ipc_buffer_top = ipc_buffer + 4; // Reserve space for the total size.
	ipc_dest_buffer_top = ipc_dest_buffer;
}

void ipc_send()
{
	*(uint32_t *) ipc_buffer = ipc_buffer_top - ipc_buffer; // Fill in total size.
	
	char* runtime_dir = getenv("XDG_RUNTIME_DIR");
	if(runtime_dir == NULL) {
		runtime_dir = "/tmp";
	}
	
	int sock;
	struct sockaddr_un server;
	
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	server.sun_family = AF_UNIX;
	snprintf(server.sun_path, sizeof(server.sun_path), "%s/pcsx2.sock", runtime_dir);
	
	if(connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
		close(sock);
		fprintf(stderr, "[err] ipc_send: Failed to connect to socket (%s).\n", server.sun_path);
		exit(1);
	}
	
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	
	if(write(sock, ipc_buffer, ipc_buffer_top - ipc_buffer) < 0) {
		fprintf(stderr, "[err] ipc_send: Failed to write command buffer.\n");
		exit(1);
	}
	
#ifdef IPC_DEBUG
	for(uint32_t i = 0; i < ipc_buffer_top - ipc_buffer; i++) {
		if(i % 0x10 == 0) {
			printf("%08x ", i);
		}
		printf("%02x ", ipc_buffer[i]);
		if(i % 0x10 == 0xf) {
			printf("\n");
		}
	}
	printf("\n");
#endif
	
	// Read in the size of the response, when we've done that update end_size to
	// the true size of the response buffer and read the rest in as well if we
	// haven't already.
	ssize_t receive_size = 0;
	ssize_t end_size = 4;
	
	while(receive_size < end_size) {
		ssize_t size = read(sock, &ipc_return_buffer[receive_size], end_size - receive_size);
		if(size <= 0) {
			fprintf(stderr, "[err] ipc_send: Failed to read response.\n");
			receive_size = 0;
			break;
		}
		
		receive_size += size;
		
		if(end_size == 4 && receive_size >= 4) {
			end_size = *(uint32_t *) ipc_return_buffer;
			if(end_size > MAX_IPC_RETURN_SIZE) {
				fprintf(stderr, "[err] ipc_send: Response too big.\n");
				receive_size = 0;
				break;
			}
		}
	}
	
	close(sock);
	
	if(ipc_return_buffer[4] == IPC_FAIL) {
		fprintf(stderr, "[err] ipc_send: PCSX2 responded with IPC_FAIL!\n");
		exit(1);
	}
	
	// Parse the command list and write the read responses received.
	uint8_t **dest_cur = ipc_dest_buffer;
	uint8_t *ret_cur = ipc_return_buffer + 4 /* buffer size */ + 1 /* status bit */;
	for(uint8_t *cur = ipc_buffer + 4; cur < ipc_buffer_top;) {
		uint8_t flag = *cur++;
		switch(flag) {
			case MSG_READ8:
				cur += 4; // Desintation address.
				*(uint8_t *) *dest_cur = *(uint8_t *) ret_cur;
				dest_cur++;
				ret_cur += 1;
				break;
			case MSG_READ16:
				cur += 4; // Desintation address.
				*(uint16_t *) *dest_cur = *(uint16_t *) ret_cur;
				dest_cur++;
				ret_cur += 2;
				break;
			case MSG_READ32:
				cur += 4; // Desintation address.
				*(uint32_t *) *dest_cur = *(uint32_t *) ret_cur;
				dest_cur++;
				ret_cur += 4;
				break;
			case MSG_READ64:
				cur += 4; // Desintation address.
				*(uint64_t *) *dest_cur = *(uint64_t *) ret_cur;
				dest_cur++;
				ret_cur += 8;
				break;
			case MSG_WRITE8:
			case MSG_WRITE16:
			case MSG_WRITE32:
			case MSG_WRITE64:
				cur += 4; // Desination address.
				cur += _ipc_payload_size(flag);
				break;
			default:
				fprintf(stderr, "[err] ipc_send: Corrupted command buffer!\n");
				exit(1);
		}
	}
}

void ipc_read8(uint8_t *dest, PS2Ptr src)
{
	_ipc_check_overflow();
	ipc_buffer_top[0] = MSG_READ8;
	*(uint32_t*) &ipc_buffer_top[1] = src;
	ipc_buffer_top += 1 + 4;
	*ipc_dest_buffer_top++ = dest;
}

void ipc_write8(PS2Ptr dest, uint8_t value)
{
	_ipc_check_overflow();
	ipc_buffer_top[0] = MSG_WRITE8;
	*(uint32_t*) &ipc_buffer_top[1] = dest;
	ipc_buffer_top[5] = value;
	ipc_buffer_top += 1 + 4 + 1;
}

void ipc_write32(PS2Ptr dest, uint32_t value)
{
	_ipc_check_overflow();
	ipc_buffer_top[0] = MSG_WRITE32;
	*(uint32_t*) &ipc_buffer_top[1] = dest;
	ipc_buffer_top[5] = value;
	ipc_buffer_top += 1 + 4 + 4;
}

void ipc_read(void *dest, PS2Ptr src, uint32_t size)
{
	for(uint32_t i = 0; i < size; i++) {
		ipc_read8(dest + i, src + i);
	}
}

void ipc_write(PS2Ptr dest, uint8_t *src, uint32_t size)
{
	for(uint32_t i = 0; i < size; i++) {
		ipc_write8(dest + i, src[i]);
	}
}

void ipc_memset(PS2Ptr dest, uint8_t value, uint32_t size)
{
	for(uint32_t i = 0; i < size; i++) {
		ipc_write8(dest + i, value);
	}
}

static void _ipc_push8(uint8_t value)
{
	*ipc_buffer_top = value;
	ipc_buffer_top += 1;
}

static void _ipc_push32(uint32_t value)
{
	*(uint32_t *) ipc_buffer_top = value;
	ipc_buffer_top += 4;
}

static void _ipc_check_overflow()
{
	if(ipc_buffer_top >= ipc_buffer + MAX_IPC_SIZE - 0x100) {
		ipc_send();
		ipc_begin();
	}
}

static uint32_t _ipc_payload_size(uint8_t flag)
{
	switch(flag) {
		case MSG_READ8: return 1;
		case MSG_READ16: return 2;
		case MSG_READ32: return 4;
		case MSG_READ64: return 8;
		case MSG_WRITE8: return 1;
		case MSG_WRITE16: return 2;
		case MSG_WRITE32: return 4;
		case MSG_WRITE64: return 8;
		default:
			fprintf(stderr, "[err] _ipc_payload_size: Invalid flag!\n");
			exit(1);
	}
}
