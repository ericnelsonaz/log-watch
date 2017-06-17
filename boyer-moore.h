#ifndef __BOYER_MOORE_H__
#define __BOYER_MOORE_H__

/*
 * double-buffered Boyer-Moore search algorithm suitable for
 * streaming inputs, where we don't know if the needle will
 * be split across multiple reads.
 *
 * Expected usage assumes that sequential calls to boyer_moore_search()
 * contain data from the previous two reads from a stream. That is,
 * the "head" contains the most recently read data, and the "tail"
 * contains data from the previous read.
 *
 * Note that the maximum size of the "needle" is defined by the amount
 * of data read in two reads (the sum of the lengths of head and tail).
 *
 */

struct boyer_moore_context {
	unsigned needlelen;
	unsigned char minval;
	unsigned char maxval;
	unsigned skip; /* leftover from previous search */
	char const *needle;
	int occurrences[256];	/* offset by minval to keep cache clean */
};

void boyer_moore_init(char const *needle,
		      struct boyer_moore_context *ctx);

inline void boyer_moore_reset(struct boyer_moore_context *ctx)
{
	ctx->skip = 0;
}

/*
 * returns 1 and ending position in *pos if a match is found.
 */
int boyer_moore_search
	(struct boyer_moore_context *ctx,
	 char const *head,
	 unsigned headlen,
	 char const *tail,
	 unsigned taillen,
	 unsigned *pos);

#endif
