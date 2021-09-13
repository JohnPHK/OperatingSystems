/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Andrew Peterson, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "pagetable.h"
#include "sim.h"

//---------------------------------------------------------------------
// Bitmap definitions and functions to manage space in swapfile.
// We create a fixed-size swapfile, although it could be made larger
// on demand with a little effort.
//
// The bitmap code is modified from the OS/161 bitmap functions.

#define BITS_PER_WORD 32 // Assumes sizeof(unsigned) = 4 bytes, 32 bits
#define WORD_ALLBITS  (0xffffffff)

#define DIVROUNDUP(a, b) (((a) + (b) -1) / (b))

struct bitmap {
	unsigned nbits;
	unsigned *v;
};

static struct bitmap *bitmap_create(unsigned nbits)
{
	unsigned words = DIVROUNDUP(nbits, BITS_PER_WORD);
	struct bitmap *b = (struct bitmap *)malloc(sizeof(struct bitmap));
	if (b == NULL) {
		return NULL;
	}
	b->v = malloc(words*sizeof(unsigned));
	if (b->v == NULL) {
		free(b);
		return NULL;
	}

	memset(b->v, 0, words*sizeof(unsigned));
	b->nbits = nbits;

	/* Mark any leftover bits at the end in use */
	if (words > nbits / BITS_PER_WORD) {
		unsigned ix = words - 1;
		unsigned overbits = nbits - ix * BITS_PER_WORD;

		assert(nbits / BITS_PER_WORD == words - 1);
		assert(overbits > 0 && overbits < BITS_PER_WORD);

		for (unsigned j = overbits; j < BITS_PER_WORD; j++) {
			b->v[ix] |= ((unsigned)1 << j);
		}
	}

	return b;
}

static int bitmap_alloc(struct bitmap *b, unsigned *index)
{
	unsigned maxix = DIVROUNDUP(b->nbits, BITS_PER_WORD);

	for (unsigned ix = 0; ix < maxix; ix++) {
		if (b->v[ix] != WORD_ALLBITS) {
			for (unsigned offset = 0; offset < BITS_PER_WORD; offset++) {
				unsigned mask = ((unsigned)1) << offset;

				if ((b->v[ix] & mask) == 0) {
					b->v[ix] |= mask;
					*index = (ix * BITS_PER_WORD) + offset;
					assert(*index < b->nbits);
					return 0;
				}
			}
			assert(0);
		}
	}
	return 1;
}

static void bitmap_destroy(struct bitmap *b)
{
	free(b->v);
	free(b);
}

//---------------------------------------------------------------------
// Swap definitions and functions.

static int swapfd;
static struct bitmap *swapmap;
static char *fname;

int swap_init(unsigned swapsize)
{
	// Initialize the swap file
	fname = malloc(20);
	strncpy(fname, "swapfile.XXXXXX",20);
	if ((swapfd = mkstemp(fname)) == -1) {
		perror("Failed to create temporary file for swap");
		exit(1);
	}

	// Initialize the bitmap
	if ((swapmap = bitmap_create(swapsize)) == NULL) {
		fprintf(stderr,"Failed to create bitmap for swap\n");
		exit(1);
	}

	return 0;
}

void swap_destroy()
{
	// Close and remove swapfile
	close(swapfd);
	unlink(fname);

	// Destroy bitmap
	bitmap_destroy(swapmap);
}

// Read data into (simulated) physical memory 'frame' from 'swap_offset'
// in swap file.
// Input:  frame - the physical frame number (not byte offset) in physmem
//         swap_offset - the byte position in the swap file.
// Return: 0 on success, 
//	   -errno on error or number of bytes read on partial read
// 
int swap_pagein(unsigned frame, int swap_offset)
{
	assert(swap_offset != INVALID_SWAP);

	// Get pointer to page data in (simulated) physical memory
	char *frame_ptr = &physmem[frame * SIMPAGESIZE];

	// Seek to position in swap file where this page was stored
	off_t pos = lseek(swapfd, swap_offset, SEEK_SET);
	if (pos != swap_offset) {
		assert(pos == (off_t)-1);
		perror("swap_pagein: failed to set read position");
		return -errno;
	}

	// Read page data from swapfile into memory
	ssize_t bytes_read = read(swapfd, frame_ptr, SIMPAGESIZE);
	if (bytes_read != SIMPAGESIZE) {
		fprintf(stderr, "swap_pagein: did not read whole page\n");
		return bytes_read;
	}
	return 0;
}

// Write data from (simulated) physical memory 'frame' to 'swap_offset'
// in swap file. Allocates space in swap file for virtual page if needed.
// Input:  frame - the physical frame number (not byte offset in physmem)
//         swap_offset - the byte position in the swap file.
// Return: the swap_offset where the data was written on success,
//         or INVALID_SWAP on failure
// 
int swap_pageout(unsigned frame, int swap_offset)
{
	// Check if swap has already been allocated for this page 
	if (swap_offset == INVALID_SWAP) {
		unsigned idx;
		if (bitmap_alloc(swapmap, &idx) != 0) {
			fprintf(stderr, "swap_pageout: Could not allocate space in swapfile. Try running again with a larger swapsize.\n");
			return INVALID_SWAP;
		}
		swap_offset = idx * SIMPAGESIZE;
	}
	assert(swap_offset != INVALID_SWAP);

	// Get pointer to page data in (simulated) physical memory
	void *frame_ptr = &physmem[frame * SIMPAGESIZE];

	// Seek to position in swap file where this page will be stored
	off_t pos = lseek(swapfd, swap_offset, SEEK_SET);
	if (pos != swap_offset) {
		assert(pos == (off_t)-1);
		perror("swap_pageout: failed to set write position");
		return INVALID_SWAP;
	}

	// Read page data from swapfile into memory
	ssize_t bytes_written = write(swapfd, frame_ptr, SIMPAGESIZE);
	if (bytes_written != SIMPAGESIZE) {
		fprintf(stderr,"swap_pageout: did not write whole page\n");
		return INVALID_SWAP;
	}
	return swap_offset;
}
