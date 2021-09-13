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

#include "pagetable.h"
#include "sim.h"

extern unsigned memsize;
int *clock;
int pointer;

/* Page to evict is chosen using the CLOCK algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int clock_evict(void)
{
	//TODO
	int size = memsize;
	// reach the end of the clock
	if(pointer == size){
		pointer = 0;
	}

	// when the clock points to bit 1
	if(clock[pointer] == 1){
		clock[pointer] = 0;
		coremap[pointer].pte->frame &= ~PG_REF;
		pointer += 1;
		return clock_evict();
	}
	// when the clock points to bit 0
	else{
		pointer += 1;
		return pointer - 1;
	}
}

/* This function is called on each access to a page to update any information
 * needed by the CLOCK algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p)
{
	//TODO
	int index = p -> frame >> PAGE_SHIFT;
	clock[index] = 1;
	// change the reference bit to 1
	p->frame |= PG_REF;
	return;
}

/* Initialize any data structures needed for this replacement algorithm. */
void clock_init(void)
{
	//TODO
	clock = malloc(memsize * sizeof(int));
	// make sure all page have bit 0
	for(unsigned i=0;i<memsize;i++){
		clock[i] = 0;
	}
	pointer = 0;
	return;
}
