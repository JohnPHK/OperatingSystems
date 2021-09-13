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
 * CSC369 Assignment 1 - a1fs formatting tool.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#include "a1fs.h"
#include "map.h"


/** Command line options. */
typedef struct mkfs_opts {
	/** File system image file path. */
	const char *img_path;
	/** Number of inodes. */
	size_t n_inodes;

	/** Print help and exit. */
	bool help;
	/** Overwrite existing file system. */
	bool force;
	/** Sync memory-mapped image file contents to disk. */
	bool sync;
	/** Verbose output. If false, the program must only print errors. */
	bool verbose;
	/** Zero out image contents. */
	bool zero;

} mkfs_opts;

static const char *help_str = "\
Usage: %s options image\n\
\n\
Format the image file into a1fs file system. The file must exist and\n\
its size must be a multiple of a1fs block size - %zu bytes.\n\
\n\
Options:\n\
    -i num  number of inodes; required argument\n\
    -h      print help and exit\n\
    -f      force format - overwrite existing a1fs file system\n\
    -s      sync image file contents to disk\n\
    -v      verbose output\n\
    -z      zero out image contents\n\
";

static void print_help(FILE *f, const char *progname)
{
	fprintf(f, help_str, progname, A1FS_BLOCK_SIZE);
}


static bool parse_args(int argc, char *argv[], mkfs_opts *opts)
{
	char o;
	while ((o = getopt(argc, argv, "i:hfsvz")) != -1) {
		switch (o) {
			case 'i': opts->n_inodes = strtoul(optarg, NULL, 10); break;

			case 'h': opts->help    = true; return true;// skip other arguments
			case 'f': opts->force   = true; break;
			case 's': opts->sync    = true; break;
			case 'v': opts->verbose = true; break;
			case 'z': opts->zero    = true; break;

			case '?': return false;
			default : assert(false);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Missing image path\n");
		return false;
	}
	opts->img_path = argv[optind];

	if (opts->n_inodes == 0) {
		fprintf(stderr, "Missing or invalid number of inodes\n");
		return false;
	}
	return true;
}


/** Determine if the image has already been formatted into a1fs. */
static bool a1fs_is_present(void *image)
{
	//TODO: check if the image already contains a valid a1fs superblock
	a1fs_superblock *sb = (a1fs_superblock *)image;
	if(sb->magic == A1FS_MAGIC){
		return true;
	}
	return false;
}


/**
 * Format the image into a1fs.
 *
 * NOTE: Must update mtime of the root directory.
 *
 * @param image  pointer to the start of the image.
 * @param size   image size in bytes.
 * @param opts   command line options.
 * @return       true on success;
 *               false on error, e.g. options are invalid for given image size.
 */
static bool mkfs(void *image, size_t size, mkfs_opts *opts)
{
	memset(image, 0 ,size);
	a1fs_superblock *sb = (a1fs_superblock *)image;
	sb->magic = A1FS_MAGIC;
	sb->size = size;
	sb->inodes_count = opts->n_inodes;
	sb->blocks_count = size / A1FS_BLOCK_SIZE;
	sb->free_blocks_count = sb->blocks_count - 1;
	sb->free_inodes_count = sb->inodes_count - 1;

	uint64_t numOfInodeBm = sb->inodes_count / (A1FS_BLOCK_SIZE * 8);
	if(sb->inodes_count % (A1FS_BLOCK_SIZE * 8) != 0){
		numOfInodeBm += 1;
	}
	uint64_t numOfDataBm = sb->blocks_count / (A1FS_BLOCK_SIZE * 8);
	if(sb->blocks_count % (A1FS_BLOCK_SIZE * 8) != 0){
		numOfDataBm += 1;
	}
	uint64_t numOfInodeTable = sb->inodes_count * sizeof(a1fs_inode) / A1FS_BLOCK_SIZE;
	if((sb->inodes_count * sizeof(a1fs_inode) % A1FS_BLOCK_SIZE) != 0){
		numOfInodeTable += 1;
	}

	// no more space to allocate datablock
	uint64_t totalReserveBlock = 1 + numOfInodeBm + numOfDataBm + numOfInodeTable;
	if(totalReserveBlock >= sb->blocks_count){
		return false;
	}
	sb->inode_bitmap = 1;
	sb->datablock_bitmap = sb->inode_bitmap + numOfInodeBm;
	sb->first_inode_block = sb->datablock_bitmap + numOfDataBm;
	sb->first_data_block = sb->first_inode_block + numOfInodeTable;

	// set inode in the inode table
	unsigned char *fisrtInode = (unsigned char*)(image + (sb->first_inode_block) * A1FS_BLOCK_SIZE);
	memset(fisrtInode, 0, numOfInodeTable * A1FS_BLOCK_SIZE);

	a1fs_inode *rootInode = (a1fs_inode *)(image+(sb->first_inode_block)*A1FS_BLOCK_SIZE);
	rootInode->mode = S_IFDIR;
	rootInode->links = 2;
	rootInode->size = 2 * sizeof(a1fs_dentry);
	clock_gettime(CLOCK_REALTIME,&rootInode->mtime);
	rootInode->inode_num = 1;
	rootInode->i_blocks = 0;
	
	// set DataBlock bitmap first char to 1, rest to 0
	unsigned char *DataBlockBm = (unsigned char*)(image + (sb->datablock_bitmap) * A1FS_BLOCK_SIZE);
	memset(DataBlockBm, 0, numOfDataBm * A1FS_BLOCK_SIZE);
	DataBlockBm[0] = DataBlockBm[0] | (1<<0);

	// set InodeBlock bitmap first char to 1, rest to 0
	unsigned char *InodeBm = (unsigned char*)(image + (sb->inode_bitmap) * A1FS_BLOCK_SIZE);
	memset(InodeBm, 0, numOfInodeBm * A1FS_BLOCK_SIZE);
	InodeBm[0] = InodeBm[0] | (1<<0);


	return true;
}


int main(int argc, char *argv[])
{
	mkfs_opts opts = {0};// defaults are all 0
	if (!parse_args(argc, argv, &opts)) {
		// Invalid arguments, print help to stderr
		print_help(stderr, argv[0]);
		return 1;
	}
	if (opts.help) {
		// Help requested, print it to stdout
		print_help(stdout, argv[0]);
		return 0;
	}

	// Map image file into memory
	size_t size;
	void *image = map_file(opts.img_path, A1FS_BLOCK_SIZE, &size);
	if (image == NULL) return 1;

	// Check if overwriting existing file system
	int ret = 1;
	if (!opts.force && a1fs_is_present(image)) {
		fprintf(stderr, "Image already contains a1fs; use -f to overwrite\n");
		goto end;
	}

	if (opts.zero) memset(image, 0, size);
	if (!mkfs(image, size, &opts)) {
		fprintf(stderr, "Failed to format the image\n");
		goto end;
	}

	// Sync to disk if requested
	if (opts.sync && (msync(image, size, MS_SYNC) < 0)) {
		perror("msync");
		goto end;
	}

	ret = 0;
end:
	munmap(image, size); // unmap
	return ret;
}
