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

struct page_frame{
	unsigned int frame;
	struct page_frame* newer;
};

struct page_frame *oldest;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int lru_evict(void)
{
	//TODO
    unsigned int to_evict;

    if (oldest == NULL) {
        perror("lru error");
        exit(1);
    }

    if (oldest->newer == NULL) {
        to_evict = oldest->frame;
        free(oldest);
        oldest = NULL;
        return to_evict;
    }

    to_evict = oldest->frame;
	struct page_frame *temp = oldest;
	oldest = oldest->newer;
	free(temp);
	return to_evict;
    
}

/* This function is called on each access to a page to update any information
 * needed by the LRU algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p)
{
	//TODO

    unsigned int frame = p -> frame >> PAGE_SHIFT;

	if(oldest == NULL){
		oldest = malloc(sizeof(struct page_frame));
		oldest->frame = frame;
		oldest->newer = NULL;
	} else {
        struct page_frame *cur = oldest;
        struct page_frame *temp;
        if (oldest->frame == frame) {
            temp = oldest;
            oldest = oldest->newer;
            cur = oldest;
            free(temp);
            if(cur != NULL){
                while (cur->newer != NULL) {
                    cur = cur->newer;
                }
            }
        } else {
            while (cur->newer != NULL) {
                if (cur->newer->frame == frame) {
                    temp = cur->newer;
                    cur->newer = cur->newer->newer;
                    free(temp);
                    if (cur->newer == NULL) {
                        break;
                    } 
                }
                cur = cur->newer;
            }

        }

        cur->newer = malloc(sizeof(struct page_frame));
        cur->newer->frame = frame;
        cur->newer->newer = NULL;

    }
}

/* Initialize any data structures needed for this replacement algorithm. */
void lru_init(void)
{
	//TODO
    oldest = NULL;
}
