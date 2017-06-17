/*
 * Copyright (C) 2017  Nelson Integration, LLC
 */

#include "boyer-moore.h"
#include <stdio.h>
#include <string.h>

void boyer_moore_init(char const *needle,
		      struct boyer_moore_context *ctx)
{
	unsigned len = ctx->needlelen = strlen(needle);
	unsigned i;

	ctx->skip = 0;
	ctx->needle = needle;

	/* find min and max */
	ctx->minval = 0xff;
	ctx->maxval = 0;
	for (i=0; i < len; i++) {
		unsigned char c = needle[i];
		if (c < ctx->minval)
			ctx->minval = c;
		if (c > ctx->maxval)
			ctx->maxval = c;
	}

	unsigned range = ctx->maxval - ctx->minval + 1;

	/* max values for missing occurrences */
	for (i=0; i < range; i++)
		ctx->occurrences[i] = ctx->needlelen;

	/* calculate offset-from-end for each value */
	for (i=0; i < len; i++) {
		unsigned char c = needle[i];
		ctx->occurrences[c-ctx->minval] = len-i-1;
	}
}

unsigned bm_offset(struct boyer_moore_context const *ctx,
		   unsigned char c)
{
	if ((c >= ctx->minval) && (c <= ctx->maxval))
		return ctx->occurrences[c-ctx->minval];
	return ctx->needlelen;
}

int bm_cmp(struct boyer_moore_context const *ctx,
	   unsigned lastchar,
	   char const *head,
	   unsigned headlen,
	   char const *tail,
	   unsigned taillen)
{
	unsigned avail=lastchar + taillen + 1;
	if (ctx->needlelen > avail)
		return 0; /* not enough data */

	/* common case: needle completely in head */
	if (lastchar >= ctx->needlelen)
		return (memcmp(head+lastchar-ctx->needlelen+1,
			       ctx->needle,
			       ctx->needlelen) == 0);

	/* try partial match in head, rest in tail */
	unsigned eos = lastchar + 1;
	if (memcmp(head, ctx->needle + ctx->needlelen - eos, eos) == 0) {
		unsigned left = ctx->needlelen - eos;
		return (memcmp(tail + taillen - left, ctx->needle, left) == 0);
	}

	return 0;
}

int boyer_moore_search
	(struct boyer_moore_context *ctx,
	 char const *head,
	 unsigned headlen,
	 char const *tail,
	 unsigned taillen,
	 unsigned *pos)
{
	if (headlen < ctx->skip) {
		ctx->skip -= headlen;
		return 0;
	}
	unsigned i=ctx->skip;
	while (i < headlen) {
		unsigned char c = head[i];
		unsigned offs = bm_offset(ctx, c);
		if (offs == 0) {
			if (bm_cmp(ctx, i, head, headlen, tail, taillen)) {
				*pos = i;
				return 1;
			}
			else
				i++;
		}
		else {
			i += offs;
		}
	}
	ctx->skip = i-headlen;
	return 0;
}

#ifdef BOYER_MOORE_TEST
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static void print_to_eol(char const *s) {
	fputc(':', stdout);
	while(!iscntrl(*s))
		fputc(*s++, stdout);
	fputc('\n', stdout);
}

int main(int argc, char const *const argv[])
{
	if (4 > argc) {
		fprintf(stderr, "Usage: %s searchfor filename buffersize\n", argv[0]);
		return -1;
	}
	FILE *fin = fopen(argv[2], "rb");
	if (!fin) {
		perror(argv[2]);
		return -1;
	}
	int bufsize=atoi(argv[3]);
	if (0 >= bufsize) {
		fprintf(stderr, "Invalid buffer size %s, enter in decimal\n",
			argv[3]);
		return -1;
	}
	char *const buf = malloc(2*bufsize);
	char *const bufs[2] = {
		buf,
		buf+bufsize
	};

	struct boyer_moore_context ctx;
	boyer_moore_init(argv[1], &ctx);

	unsigned cur = 0, prev = 1;
	unsigned prevlen = 0;
	size_t numread;

	while(0 != (numread=fread(bufs[cur], 1, bufsize, fin))) {
		unsigned pos = 0;
		while ((pos < bufsize) &&
		       boyer_moore_search(&ctx,
					  bufs[cur]+pos, numread-pos,
					  bufs[prev], prevlen,
					  &pos)) {
			if (pos >= ctx.needlelen) {
				print_to_eol(bufs[cur]+pos-ctx.needlelen+1);
			} else {
				print_to_eol(bufs[cur]);
			}
			pos++;
		}
		prevlen = numread;
		prev = cur;
		cur ^= 1;
	}

	return 0;
}

#endif
