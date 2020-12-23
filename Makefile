all: pcxmemset pcxmemsetr pcxmempeek
pcxmemset: pcxmemset.c ipc.h ipc.c
	cc pcxmemset.c ipc.c -o pcxmemset
pcxmemsetr: pcxmemsetr.c ipc.h ipc.c
	cc pcxmemsetr.c ipc.c -o pcxmemsetr
pcxmempeek: pcxmempeek.c ipc.h ipc.c
	cc pcxmempeek.c ipc.c -o pcxmempeek
