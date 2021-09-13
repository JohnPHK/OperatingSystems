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

#ifndef __SIM_H__
#define __SIM_H__

#include "pagetable.h"

#define MAXLINE 256
#define SIMPAGESIZE 16  /* Simulated physical memory page frame size */

extern unsigned memsize;
extern int debug;

extern int hit_count;
extern int miss_count;
extern int ref_count;
extern int evict_clean_count;
extern int evict_dirty_count;

/* We simulate physical memory with a large array of bytes */
extern char *physmem;

/* The tracefile name is a global variable because the OPT
 * algorithm will need to read the file before you start
 * replaying the trace.
 */
extern char *tracefile;

/* Each eviction algorithm is represented by a structure with its name
 * and three functions.
 */
struct functions {
	char *name;                  // String name of eviction algorithm
	void (*init)(void);          // Initialize any data needed by alg
	void (*ref)(pgtbl_entry_t *);// Called on each reference
	int (*evict)(void);          // Called to choose victim for eviction
};

extern void (*init_fcn)(void);
extern void (*ref_fcn)(pgtbl_entry_t *);
extern int (*evict_fcn)(void);

#endif // __SIM_H__
