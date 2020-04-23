///////////////////////////////////////////////////////////////////////////////
//
/// \file       xzdec.c
/// \brief      Simple single-threaded tool to uncompress .xz or .lzma files
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

//#include "sysdefs.h"
#include "lzma.h"
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static void my_errorf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	va_end(ap);
}

int  xzdec(const char *infile, const char *outfile)
{
	lzma_ret ret;
	FILE *file = NULL, *out_file=NULL;
	int err = -1;
    lzma_action action = LZMA_RUN;
	// The same lzma_stream is used for all files that we decode. This way
	// we don't need to reallocate memory for every file if they use same
	// compression settings.
	lzma_stream strm = LZMA_STREAM_INIT;
	file = fopen(infile, "rb");
	if (!file) return err;

	out_file = fopen(outfile, "wb");
	if (!out_file) goto error;

	// Initialize the decoder
	ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
	// The only reasonable error here is LZMA_MEM_ERROR.
	if (ret != LZMA_OK) {
		my_errorf("%s", ret == LZMA_MEM_ERROR ? strerror(ENOMEM)
				: "Internal error (bug)");
		goto error;
	}

	// Input and output buffers
	uint8_t in_buf[BUFSIZ];
	uint8_t out_buf[BUFSIZ];

	strm.avail_in = 0;
	strm.next_out = out_buf;
	strm.avail_out = BUFSIZ;
	while (1) {
		if (strm.avail_in == 0) {
			strm.next_in = in_buf;
			strm.avail_in = fread(in_buf, 1, BUFSIZ, file);

			if (ferror(file)) {
				// POSIX says that fread() sets errno if
				// an error occurred. ferror() doesn't
				// touch errno.
				my_errorf("%s: Error reading input file",
						 strerror(errno));
				goto error;
			}

			// When using LZMA_CONCATENATED, we need to tell
			// liblzma when it has got all the input.
			if (feof(file))
				action = LZMA_FINISH;
		}

		ret = lzma_code(&strm, action);

		// Write and check write error before checking decoder error.
		// This way as much data as possible gets written to output
		// even if decoder detected an error.
		if (strm.avail_out == 0 || ret != LZMA_OK) {
			const size_t write_size = BUFSIZ - strm.avail_out;

			if (fwrite(out_buf, 1, write_size, out_file)
					!= write_size) {
				// Wouldn't be a surprise if writing to stderr
				// would fail too but at least try to show an
				// error message.
				my_errorf("Cannot write to standard output: "
						"%s", strerror(errno));
				goto error;
			}

			strm.next_out = out_buf;
			strm.avail_out = BUFSIZ;
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END) {
				// lzma_stream_decoder() already guarantees
				// that there's no trailing garbage.
				assert(strm.avail_in == 0);
				assert(action == LZMA_FINISH);
				assert(feof(file));
				err = 0;
				break;
			}

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = strerror(ENOMEM);
				break;

			case LZMA_FORMAT_ERROR:
				msg = "File format not recognized";
				break;

			case LZMA_OPTIONS_ERROR:
				// FIXME: Better message?
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "File is corrupt";
				break;

			case LZMA_BUF_ERROR:
				msg = "Unexpected end of input";
				break;

			default:
				msg = "Internal error (bug)";
				break;
			}

			my_errorf(" %s", msg);
			goto error;
		}
	}

error:
	if (file) fclose(file);
	if (out_file) fclose(out_file);

	return err;
}
