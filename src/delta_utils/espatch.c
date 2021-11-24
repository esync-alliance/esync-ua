/*
 * Copyright 2020 Excelfore
 * All rights reserved
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#ifdef __QNX__
#define ffs _ffs
#include <string.h>
#undef ffs
#else
#include <string.h>
#endif
#include <err.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef LIBUA_VER_2_0
	#include "esyncua.h"
#else
	#include "xl4ua.h"
#endif

#include <libxl4bus/build_config.h>

#define ESPERROR_STRINGS
#include "esdeltadec.h"

#include "common.h"

#define XSTR(s) str(s)
#define str(s) #s
#define SRCREF  __FILE__"("XSTR(__LINE__)")"
static int verbose=0;

#define dbprintf   if (verbose>0) printf
#define db2printf  if (verbose>1) printf
#define db3printf  if (verbose>2) printf
#define db4printf  if (verbose>3) printf

static size_t g_total_mem=0;
static size_t g_total_max=0;

XL4_PUB void* es_malloc(size_t size,const char *srcref)
{
	g_total_mem+=size;
	if (g_total_mem > g_total_max)
		g_total_max = g_total_mem;
	db4printf("%s: Alloc %zu  total=%zu\n",srcref,size,g_total_mem);
	size_t *ptr = malloc(size+sizeof(size_t));
	if (ptr == NULL) {
		dbprintf("%s: Unable to alloc %zu bytes\n",srcref,size);
		return NULL;
	}
	*ptr = size;
	size_t *inputbuffer = ptr + 1;
	return (void*)(inputbuffer);
}

XL4_PUB void es_free(void* ptr,const char *srcref)
{
	if (ptr) {
		size_t *p = ((size_t*)ptr)-1;
		g_total_mem-=*p;
		db4printf("%s: Free %zu  total=%zu\n",srcref,*p,g_total_mem);
		free(p);
	} else
		dbprintf("%s: Free NULL ?\n",srcref);
}

static bool __copyfile(int fdsrc, int fddst)
{
	uint8_t *buffer;
	const size_t buf_size = 16384;
	size_t n;
	buffer = (uint8_t *)malloc(buf_size);
	if (!buffer) return false;
	do {
		n = read(fdsrc, buffer, buf_size);
		if (n>0)
			n = write(fddst, buffer, n);
	} while (n>0);
	free(buffer);
	lseek(fddst, 0, SEEK_SET);
	return n>=0;
}

struct espatchctx {
	int  reffd;
	int  newfd;
	int  scratchfd;

	size_t refsize;
	bool checkreadref;
	offset_t pwr_interrupt_pos;
	size_t newsize;

	uint8_t *scratchbuf;
	size_t  scratchsize;
};

static size_t scratchmem_readblock(struct espatchctx *ctx, offset_t blkoffset, uint8_t* buffer, size_t length)
{
	if ((blkoffset + length) > ctx->scratchsize) {
		return (size_t)0;
	}
	db2printf("Read scrmem %zu, %zu\n",blkoffset,length);
	memcpy(buffer, ctx->scratchbuf+blkoffset, length);
	return length;
}

static size_t scratchmem_eraseblock(struct espatchctx *ctx, offset_t blkoffset, size_t length)
{
	if ((blkoffset + length) > ctx->scratchsize) {
		ctx->scratchbuf = realloc(ctx->scratchbuf, blkoffset + length);
		if (ctx->scratchbuf == NULL) {
			return (size_t)0;
		}
		ctx->scratchsize = blkoffset + length;
	}
	db2printf("Erase scrmem %zu, %zu\n",blkoffset,length);
	memset(ctx->scratchbuf+blkoffset, 0xff, length);
	return length;
}

static size_t scratchmem_writeblock(struct espatchctx *ctx,
                                    offset_t blkoffset,
                                    uint8_t* buffer,
                                    size_t length)
{
	if (blkoffset + length > ctx->scratchsize) {
		ctx->scratchbuf = realloc(ctx->scratchbuf, blkoffset + length);
		if (ctx->scratchbuf == NULL) {
			return (size_t)0;
		}
		ctx->scratchsize = blkoffset + length;
	}
	db2printf("Write scrmem %zu, %zu\n",blkoffset,length);
	memcpy(ctx->scratchbuf+blkoffset, buffer, length);
	return length;
}

size_t flash_readblock (void* user,
                        enum espflashbank bank,
                        offset_t blkoffset,
                        uint8_t* buffer,
                        size_t length)
{
	struct espatchctx *ctx = (struct espatchctx*)user;
	if (bank==efb_scratch && ctx->scratchbuf!=NULL) {
		return scratchmem_readblock(ctx, blkoffset, buffer, length);
	}
	int fd = (bank==efb_new) ?ctx->newfd : (bank==efb_ref) ? ctx->reffd : ctx->scratchfd;
	assert(fd > 0);
	lseek(fd,blkoffset,SEEK_SET);
	size_t n = read(fd,buffer,length);
	if (n < length && (bank==efb_new)) {
		// incase new file is smaller than expected (happens with resume testing)
		// then fill to end with crap.
		memset(buffer+n,0xdd,length-n);
	}
	db3printf("Read %s %zu, %zu\n",(bank==efb_new) ? "new" : (bank==efb_ref) ? "ref" : "scr",blkoffset,length);
	return length;
}

size_t flash_eraseblock(void* user, enum espflashbank bank, offset_t blkoffset, size_t length)
{
	struct espatchctx *ctx = (struct espatchctx*)user;
	if (bank==efb_scratch && ctx->scratchbuf!=NULL) {
		return scratchmem_eraseblock(ctx, blkoffset, length);
	}
	static uint8_t ffbuf[16384];
	if (ffbuf[0] == 0) {
		memset(ffbuf,0xff,sizeof(ffbuf));
	}
	int fd = (bank==efb_new) ? ctx->newfd : (bank==efb_ref) ? ctx->reffd : ctx->scratchfd;
	lseek(fd,blkoffset,SEEK_SET);
	size_t len = length;
	while (len) {
		size_t nb = len < sizeof(ffbuf) ? len : sizeof(ffbuf);
		if (write(fd,ffbuf,nb) != nb)
			break;
		len -= nb;
	}
	return length - len;
}

size_t flash_writeblock(void* user, enum espflashbank bank, offset_t blkoffset, uint8_t* buffer, size_t length)
{
	struct espatchctx *ctx = (struct espatchctx*)user;
	if ((bank==efb_scratch) && (ctx->scratchbuf!=NULL)) {
		return scratchmem_writeblock(ctx, blkoffset, buffer, length);
	}
	int fd = (bank==efb_new) ? ctx->newfd : (bank==efb_ref) ? ctx->reffd : ctx->scratchfd;
	//flash_eraseblock(user, bank, blkoffset, length);
	size_t offset = lseek(fd,blkoffset,SEEK_SET);
    assert(offset == blkoffset);
	size_t len = length;
	if ((bank==efb_new) && ctx->pwr_interrupt_pos && (blkoffset+len >= ctx->pwr_interrupt_pos) ) {
		if (blkoffset < ctx->pwr_interrupt_pos) {
			len = ctx->pwr_interrupt_pos - blkoffset;
		} else {
			len = 0;
		}
		db2printf("Write %s %zu, %zu interrupted at %zu\n",(bank==efb_new) ? "new" : (bank==efb_ref) ? "ref" : "scr",blkoffset,length,len);
	} else {
		db2printf("Write %s %zu, %zu\n",(bank==efb_new) ? "new" : (bank==efb_ref) ? "ref" : "scr",blkoffset,len);
	}
	len = write(fd,buffer,len);
	if ((len > 0) && (bank==efb_new)) {
		ctx->newsize = blkoffset + len;
	}
	return len;
}

char* sprintsize(size_t size) {
	static char buf[64];
	if (size >= 1024*1024*20) {
		snprintf(buf,sizeof(buf),"%d.%03dMiBytes",(uint32_t)(size/(1024*1024)),(uint32_t)(size%(1024*1024)/1024));
	} else {
		snprintf(buf,sizeof(buf),"%d.%03dKiBytes",(uint32_t)(size/1024),(uint32_t)(size%1024));
	}
	return buf;
}

const char *espatch_get_version(void)
{
	return ESDELTA_VERSION;
}

#if defined(__GHS__)
void read_file(int fd)
{
	const size_t buf_size = 4096;
	char *buf;
	buf = malloc(buf_size);
	if (!buf) return;
	lseek(fd,0,SEEK_SET);
	while(read(fd, buf, buf_size) > 0) {
	}
	free(buf);
}
#endif
int espatch(const char *reffile, const char *newfile, const char * patchfile)
{
	bool test_inplace = false;
	size_t inputbufsize=4096;
	size_t test_power_interrupt_at=0;
	bool   test_power_resume = false;
	bool   fuzz_input = false;
	bool   use_scratch_file = false;
	const char *scratchfile=NULL;

	uint8_t *inputbuffer = NULL;
	uint8_t *esbuffer = NULL;
	const size_t esbuf_size = 2*1024;
	uint8_t *wrbuf = NULL;
	const size_t wrbuf_size = 16*256*1024;
	uint8_t *rdbuf = NULL;
	const size_t rdbuf_size = 4096;
	struct espinfo info;
	char comment[128];
	size_t totalpatchlen=0;

	bzero(comment, sizeof(comment));

	if (use_scratch_file) {
		if (scratchfile == NULL) {
			scratchfile = "/tmp/esp_scratch.flash";
		}
	}

	enum esperr res=ESPERR_UNKOWN;
	struct esp *esp=es_malloc(SIZEOF_ESP_STRUCT,SRCREF);
	verbose = 1;
	if (verbose) {
		espDbgSetLevel(verbose);
	}

	int patchfd=open(patchfile,O_RDONLY,0);
	if (patchfd < 0) {
		printf("Error opening %s\n",patchfile);
		res=ESPERR_READ;
		goto cleanup;
	}

	struct espflashaccess flash;
	struct espatchctx espatchctx;

	memset(&espatchctx, 0, sizeof(espatchctx));
	espatchctx.newfd = open(newfile,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
	if (espatchctx.newfd < 0) {
		printf("Error opening %s for writing\n",newfile);
		res=ESPERR_READ;
		goto cleanup;
	}

	if (test_power_resume) {
		espatchctx.reffd = espatchctx.newfd;
	} else {
		espatchctx.reffd = open(reffile,O_RDONLY);
		if (espatchctx.reffd < 0) {
			printf("Error opening %s\n",reffile);
			res = ESPERR_READ;
			goto cleanup;
		} else {
			struct stat sb;
			fstat(espatchctx.reffd, &sb);
			espatchctx.refsize = sb.st_size;
		}
	}

	if (use_scratch_file) {
		espatchctx.scratchfd = open(scratchfile,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if (espatchctx.scratchfd < 0) {
			printf("Error opening %s for writing\n",scratchfile);
			res=ESPERR_READ;
			goto cleanup;
		}
	} else {
		espatchctx.scratchsize= 16384;
		espatchctx.scratchbuf = malloc(espatchctx.scratchsize);
	}

	inputbuffer=es_malloc(inputbufsize,SRCREF);
	if (!inputbuffer) goto cleanup;
	esbuffer = (uint8_t *)malloc(esbuf_size);
	if (!esbuffer) goto cleanup;

	wrbuf = (uint8_t *)malloc(wrbuf_size);
	if (!wrbuf) goto cleanup;

	rdbuf = (uint8_t *)malloc(rdbuf_size);
	if (!rdbuf) goto cleanup;

	test_inplace = true;
	if (test_inplace && !test_power_resume) {
		__copyfile(espatchctx.reffd, espatchctx.newfd);
		espatchctx.checkreadref = true;
	}

	espatchctx.pwr_interrupt_pos = test_power_interrupt_at;

	flash.user      = (void*)&espatchctx;
	flash.readblock = flash_readblock;
	flash.writeblock= flash_writeblock;
	flash.eraseblock= flash_eraseblock;

	flash.wrsec_size= wrbuf_size;
	flash.wrbuf_size= wrbuf_size;
	flash.wrbuf=wrbuf;

	flash.rdsec_size= rdbuf_size;
	flash.rdbuf=rdbuf;

	res = espInit(esp, &flash, esbuffer, esbuf_size);

	if (res == ESPOK) {
		size_t ix=0;
		size_t fuzz = 0;
		srand(0);

		do {
			if (fuzz_input) {
				fuzz = random() % (inputbufsize/2);
				if (fuzz > inputbufsize-ix)
					fuzz = 0;
			}
			size_t len = 0;
			if (res == ESPOK_MORE) {
				len = read(patchfd, &inputbuffer[ix], inputbufsize-ix-fuzz);
				if (len == 0) {
					res = ESPERR_EOF;
					break;
				}
				totalpatchlen += len;
			}
			res = espProcess(esp,inputbuffer, len+ix, &ix);
			if (res == ESPOK_INFO) {
				res = espGetInfo(esp, &info);
				if (info.comment)
					strcpy_s(comment,info.comment,sizeof(comment));
			}
		} while (res == ESPOK_MORE || res == ESPOK);
	}
cleanup:
	if (espatchctx.reffd >= 0  && espatchctx.reffd != espatchctx.newfd) {
		close(espatchctx.reffd);
	}
	if (espatchctx.scratchfd > 0) {
		close(espatchctx.scratchfd);
	}
	if (espatchctx.scratchbuf != NULL) {
		free(espatchctx.scratchbuf);
	}

	if (espatchctx.newfd > 0 && res >= ESPOK && espatchctx.newsize) {
		dbprintf("Truncate %zu\n",espatchctx.newsize);
#if defined(__GHS__)
		// In INTEGRITY we need to read whole file before calling ftruncate
		// to make the ftruncate works properlly
		// I have no idea why this happens. Perhaps read whole file make file
		// contents flushed to file properlly
		read_file(espatchctx.newfd);
#endif
		if (ftruncate(espatchctx.newfd, espatchctx.newsize) != 0) {
			printf("Error truncating new file: %d\n",errno);
		}
	}
	if (espatchctx.newfd > 0) close(espatchctx.newfd);
	if (patchfd > 0) close(patchfd);
	if (res >= ESPOK) {
		espatchctx.newfd=open(newfile,O_RDONLY);
		if (espCheckNew(esp)) {
			printf("Success\n");
		} else {
			printf("Error: New image SHA mismatch\n");
			res = ESPERR_IMGSHA;
		}
		close(espatchctx.newfd);
	} else {
		printf("Error: %d,%s [line:%d]\n",res,str_esperr[res],espDbgGetErrorLine(esp));
	}
	if (res >= ESPOK) {
		size_t sectorbufsize=info.sectorsize*info.numsectors;
		size_t working_mem_size=g_total_max-sectorbufsize-inputbufsize;
		float  compressed=100.0-100.0*totalpatchlen/info.newsize;
		printf("espatch stats info, numsectors:%ld,workingmemsize:%ld,compressed:%f%% \n",
		       sectorbufsize, working_mem_size, compressed);
	}
	if (esbuffer) free(esbuffer);
	if (rdbuf) free(rdbuf);
	if (wrbuf) free(wrbuf);

	if (inputbuffer) es_free(inputbuffer, SRCREF);

	return !(res >= ESPOK);
}
