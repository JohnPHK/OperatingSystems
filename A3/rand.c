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

/* Page to evict is chosen using the RAND algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int rand_evict(void)
{
	// choose index in coremap to evict a page from
	return (int)(random() % memsize);
}

/* This function is called on each access to a page to update any information
 * needed by the RAND algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void rand_ref(pgtbl_entry_t *p)
{
	(void)p;
	return;
}

/* Initialize any data structures needed for this replacement algorithm. */
void rand_init(void)
{
}
