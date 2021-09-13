/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 1 - Miscellaneous utility functions.
 */

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include "a1fs.h"

#define FUSE_USE_VERSION 29
#include <fuse.h>

/** Check if x is a power of 2. */
static inline bool is_powerof2(size_t x)
{
	return (x & (x - 1)) == 0;
}

/** Check if x is a multiple of alignment (which must be a power of 2). */
static inline bool is_aligned(size_t x, size_t alignment)
{
	assert(is_powerof2(alignment));
	return (x & (alignment - 1)) == 0;
}

/** Align x up to a multiple of alignment (which must be a power of 2). */
static inline size_t align_up(size_t x, size_t alignment)
{
	assert(is_powerof2(alignment));
	return (x + alignment - 1) & (~alignment + 1);
}

int find_inode_path(const char *path, char *sb, a1fs_inode **inode);
int find_inode_name(char *name, char *sb, a1fs_inode *inode);
a1fs_inode *find_inode_num(char *image, a1fs_ino_t num);
a1fs_blk_t total_datablock_for_inode(a1fs_inode *inode);
int read_entries(fuse_fill_dir_t filler, char *image, a1fs_inode *inode, void *buf);
a1fs_ino_t empty_inode_bitmap(char *image);
a1fs_blk_t empty_block_bitmap(char *image);
int toggle_inode_bit(char *image, a1fs_ino_t num);
int toggle_block_bit(char *image, a1fs_blk_t num);
int change_parent(char * image, a1fs_inode *parent_inode, char *name, a1fs_ino_t inodeNo);
int remove_entry(char *image, a1fs_inode *parent, char *name);
char *find_data_block(char *image, a1fs_ino_t block_number);
char *get_block(char *image, a1fs_ino_t block_number);
int format_dir(char *image,  a1fs_blk_t start);
int check_block_bitmap(char *image, a1fs_blk_t num);
a1fs_extent *get_new_extent(a1fs_inode *inode);
void null_padding_helper(char *image, int start_offset, a1fs_blk_t to_toggle_index, 
uint64_t current_size, uint64_t new_size, int free_indicator);
