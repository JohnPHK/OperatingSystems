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
 * CSC369 Assignment 1 - a1fs driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <libgen.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"
#include "util.h"
//NOTE: All path arguments are absolute paths within the a1fs file system and
// start with a '/' that corresponds to the a1fs root directory.
//
// For example, if a1fs is mounted at "~/my_csc369_repo/a1b/mnt/", the path to a
// file at "~/my_csc369_repo/a1b/mnt/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "~/my_csc369_repo/a1b/mnt/dir/" will be passed to
// FUSE callbacks as "/dir".


/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool a1fs_init(fs_ctx *fs, a1fs_opts *opts)
{
	// Nothing to initialize if only printing help or version
	if (opts->help || opts->version) return true;

	size_t size;
	void *image = map_file(opts->img_path, A1FS_BLOCK_SIZE, &size);
	if (!image) return false;

	return fs_ctx_init(fs, image, size, opts);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in a1fs_init().
 */
static void a1fs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx*)ctx;
	if (fs->image) {
		if (fs->opts->sync && (msync(fs->image, fs->size, MS_SYNC) < 0)) {
			perror("msync");
		}
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx*)fuse_get_context()->private_data;
}


/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All remaining fields are required.
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int a1fs_statfs(const char *path, struct statvfs *st)
{
	(void)path;// unused
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));
	st->f_bsize   = A1FS_BLOCK_SIZE;
	st->f_frsize  = A1FS_BLOCK_SIZE;
	//TODO: fill in the rest of required fields based on the information stored
	// in the superblock
	a1fs_superblock *sb = (a1fs_superblock *)(fs->image);
	st->f_blocks = sb->blocks_count;
	st->f_bfree = sb->free_blocks_count;
	st->f_bavail = sb->free_blocks_count;
	st->f_files = sb->inodes_count;
	st->f_ffree = sb->free_inodes_count;
	st->f_favail = sb->free_inodes_count;
	st->f_namemax = A1FS_NAME_MAX;
	return 0;
}

/**
 * Get file or directory attributes.
 *
 * Implements the stat() system call. See "man 2 stat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All remaining fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors).
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int a1fs_getattr(const char *path, struct stat *st)
{
	if (strlen(path) >= A1FS_PATH_MAX) return -ENAMETOOLONG;
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));

	//TODO
	char * sb = (char *) fs->image;
	a1fs_inode *inode;
	int result = find_inode_path(path, sb, &inode);
	if(result == -1){
		return -ENOENT ;
	}
	if(result == -2){
		return -ENOTDIR;
	}
	st->st_mode = inode->mode;
	st->st_nlink = inode->links;
	st->st_size = inode->size;
	st->st_blocks = total_datablock_for_inode(inode) * A1FS_BLOCK_SIZE / 512;// change here
	st->st_mtim = inode->mtime;
	return 0;
	// return -ENOSYS;
}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler() for each directory
 * entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */
static int a1fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
	(void)offset;// unused
	(void)fi;// unused
	fs_ctx *fs = get_fs();
	char * sb = (char *) fs->image;
	a1fs_inode *inode;
	find_inode_path(path, sb, &inode);
	int result = read_entries(filler, sb, inode, buf);
	if(result == -1 || filler(buf, "." , NULL, 0) != 0 || filler(buf, ".." , NULL, 0) != 0){
		return -ENOMEM;
	}
	return 0;
}


/**
 * Create a directory.
 *
 * Implements the mkdir() system call.
 *
 * NOTE: the mode argument may not have the type specification bits set, i.e.
 * S_ISDIR(mode) can be false. To obtain the correct directory type bits use
 * "mode | S_IFDIR".
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the directory to create.
 * @param mode  file mode bits.
 * @return      0 on success; -errno on error.
 */
static int a1fs_mkdir(const char *path, mode_t mode)
{
	fs_ctx *fs = get_fs();
    char *image = fs->image;
	char new_path[A1FS_PATH_MAX];
    strcpy(new_path, path);

    char parentPath[A1FS_PATH_MAX];
    char filename[A1FS_NAME_MAX];

    strcpy(filename, basename(new_path));
    strcpy(parentPath, dirname(new_path));

	// find available inode
    a1fs_ino_t inode_bit_available = empty_inode_bitmap(image);
    if (inode_bit_available == 0) {
		return -ENOSPC;
	}
    toggle_inode_bit(image, inode_bit_available);
	a1fs_ino_t inodeNum = inode_bit_available+1;
	
    /*Initiating an inode*/
    a1fs_inode *new_inode = find_inode_num(image, inodeNum);
    new_inode->mode = mode | S_IFDIR;
    new_inode->links = 2;
    new_inode->size = (2*sizeof(a1fs_dentry));
    new_inode->inode_num = inodeNum;
    new_inode->i_blocks = 0;
    clock_gettime(CLOCK_REALTIME, &new_inode->mtime);
    
    /*Update the parent diretory*/
	a1fs_inode *parent;
	find_inode_path(parentPath, image, &parent);
    parent->links += 1;
     int result = change_parent(image, parent, filename, inodeNum);
     if(result == -1){
         return -ENOSPC;
     }
    return 0;
}

/**
 * Remove a directory.
 *
 * Implements the rmdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOTEMPTY  the directory is not empty.
 *
 * @param path  path to the directory to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rmdir(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO: remove the directory at given path (only if it's empty)
    char *image = fs->image;
    char pathA[A1FS_PATH_MAX];
    strcpy(pathA, path);

    char filename[A1FS_NAME_MAX];
    strcpy(filename, basename(pathA));
	a1fs_inode *inode_to_remove;
    find_inode_path(path, image, &inode_to_remove);

    if (inode_to_remove->links != 2) {
         return -ENOTEMPTY;
     }

	a1fs_ino_t inodeNum = inode_to_remove->inode_num;
	toggle_inode_bit(image, inodeNum-1);
    
    /*update parent*/
    char parentPath[A1FS_PATH_MAX];
    strcpy(parentPath, dirname(pathA));
	a1fs_inode *parent;
    find_inode_path(parentPath, image, &parent);
    parent->links -= 1;
	parent->size -= sizeof(a1fs_dentry);
	remove_entry(image, parent, filename);
	return 0;
}

/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */
static int a1fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)fi;// unused
	assert(S_ISREG(mode));
		fs_ctx *fs = get_fs();
    char *image = fs->image;
	char pathA[A1FS_PATH_MAX];
    strcpy(pathA, path);
    
    char filename[A1FS_NAME_MAX];
    strcpy(filename, basename(pathA));

	// find available inode
    a1fs_ino_t inode_bit_available = empty_inode_bitmap(image);
    if (inode_bit_available == 0) {
		return -ENOSPC;
	}
    toggle_inode_bit(image, inode_bit_available);
	a1fs_ino_t inodeNum = inode_bit_available+1;
	
    /*Initiating an inode*/
    a1fs_inode *new_inode = find_inode_num(image, inodeNum);
    new_inode->mode = mode;
    new_inode->links = 1;
    new_inode->size = 0;
    new_inode->inode_num = inodeNum;
    new_inode->i_blocks = 0;
    clock_gettime(CLOCK_REALTIME, &new_inode->mtime);
    
    /*Update the parent diretory*/
	a1fs_inode *parent;
    char parentPath[A1FS_PATH_MAX];
    strcpy(parentPath, dirname(pathA));
	find_inode_path(parentPath, image, &parent);
    int result = change_parent(image, parent, filename, inodeNum);
	if(result == -1){
		return -ENOSPC;
	}
    return 0;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO: remove the file at given path
	char *image = fs->image;
    char pathA[A1FS_PATH_MAX];
    char filename[A1FS_NAME_MAX];
	strcpy(pathA, path);
    strcpy(filename, basename(pathA));

	a1fs_inode *inode_to_remove;
    find_inode_path(path, image, &inode_to_remove);
	a1fs_ino_t inodeNum = inode_to_remove->inode_num;
	toggle_inode_bit(image, inodeNum-1);
    
	for(a1fs_blk_t i = 0; i< inode_to_remove->i_blocks; i++){
		a1fs_extent extent = inode_to_remove->i_block[i];
		for(a1fs_blk_t j = extent.start; j < extent.start + extent.count; j++){
			toggle_block_bit(image, j);
			}
		}
    /*update parent*/
	a1fs_inode *parent;
    char parentPath[A1FS_PATH_MAX];
    strcpy(parentPath, dirname(pathA));
    find_inode_path(parentPath, image, &parent);
	parent->size -= sizeof(a1fs_dentry);
	remove_entry(image, parent, filename);
	return 0;
}

/**
 * Rename a file or directory.
 *
 * Implements the rename() system call. See "man 2 rename" for details.
 * If the destination file (directory) already exists, it must be replaced with
 * the source file (directory). Existing destination can be replaced if it's a
 * file or an empty directory.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "from" exists.
 *   The parent directory of "to" exists and is a directory.
 *   If "from" is a file and "to" exists, then "to" is also a file.
 *   If "from" is a directory and "to" exists, then "to" is also a directory.
 *
 * Errors:
 *   ENOMEM     not enough memory (e.g. a malloc() call failed).
 *   ENOTEMPTY  destination is a non-empty directory.
 *   ENOSPC     not enough free space in the file system.
 *
 * @param from  original file path.
 * @param to    new file path.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rename(const char *from, const char *to)
{
	fs_ctx *fs = get_fs();

	//TODO: move the inode (file or directory) at given source path to the
	// destination path, according to the description above
    char *image = fs->image;
    char fromC[A1FS_PATH_MAX];
    char toC[A1FS_PATH_MAX];
    strcpy(fromC, from);
    strcpy(toC, to);

    char filename[A1FS_NAME_MAX];
    char newFileName[A1FS_NAME_MAX];
    strcpy(filename, basename(fromC));
    strcpy(newFileName, basename(toC));

    char fromParentPath[A1FS_PATH_MAX];
    char toParentPath[A1FS_PATH_MAX];
    strcpy(fromParentPath, dirname(fromC));
    strcpy(toParentPath, dirname(toC));


    a1fs_inode *toParentInode;
    find_inode_path(toParentPath, image, &toParentInode);

    a1fs_inode *newInode;
    int find = find_inode_path(to, image, &newInode);
    if(find == 0){
        if(S_ISREG(newInode->mode) && newInode->size != 0){
            return -ENOTEMPTY;
        }
        else if(S_ISDIR(newInode->mode) && newInode->size != 2*sizeof(a1fs_dentry)){
            return -ENOTEMPTY;
        }
        toParentInode->links --;
        remove_entry(image, toParentInode, newFileName);
    }

    a1fs_inode *inode;
	find_inode_path(from, image, &inode);

    if(change_parent(image, toParentInode, newFileName, inode->inode_num) == -1){
        return -ENOSPC;
    }
    toParentInode->links ++;

    a1fs_inode *fromParInode;
    find_inode_path(fromParentPath, image, &fromParInode);
    fromParInode->size -= sizeof(a1fs_dentry);
    remove_entry(image, fromParInode, filename);


	return 0;
}


/**
 * Change the access and modification times of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only have to implement the setting of modification time (mtime).
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * @param path  path to the file or directory.
 * @param tv    timestamps array. See "man 2 utimensat" for details.
 * @return      0 on success; -errno on failure.
 */
static int a1fs_utimens(const char *path, const struct timespec tv[2])
{
	fs_ctx *fs = get_fs();

	//TODO: update the modification timestamp (mtime) in the inode for given
	// path with either the time passed as argument or the current time,
	// according to the utimensat man page
	char *sb = fs->image;
	a1fs_inode *inode;
	find_inode_path(path,sb,&inode);
	inode->mtime = tv[0];
	return 0;
}

/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, future reads from the new uninitialized range must
 * return ranges filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */
static int a1fs_truncate(const char *path, off_t size)
{
    fs_ctx *fs = get_fs();
    /*(void) path;*/
    /*(void) size;*/
    /*(void) fs;*/
    
	/*return -ENOSYS;*/
    

	//TODO: set new file size, possibly "zeroing out" the uninitialized range
    char *image = fs -> image;
    a1fs_inode *inode;
    
    find_inode_path(path, image, &inode);
    
    uint64_t oldsize = inode->size;
    inode->size = size;
    
    a1fs_blk_t extra = 0;
    a1fs_blk_t i_extent = 0;

    /*required_blocks is the total number of block occupied by th truncated*/
    a1fs_blk_t required_blocks;
    if (size % A1FS_BLOCK_SIZE != 0) {
        required_blocks = size/A1FS_BLOCK_SIZE + 1;
    }
    else{
        required_blocks = size/A1FS_BLOCK_SIZE;
    }
    a1fs_extent *current_extent;
    /*indicated size is 0 - making a size of 0 bytes*/
    if(size == 0){
        for(a1fs_blk_t i = 0; i< inode->i_blocks; i++){
            a1fs_extent *extent = &(inode->i_block[i]);
            for(a1fs_blk_t j = extent->start; j < extent->start + extent->count; j++){
                toggle_block_bit(image, j);
            }
        }
        inode->i_blocks = 0;
        return 0;
    }
    

    /*Size must be truncated indicated size is smaller*/
    else if (oldsize >= (uint64_t) size) {
        /*Size more than multiple of blocks*/
        
        while (required_blocks != 0) {
            current_extent = &(inode->i_block[i_extent]);
            if (required_blocks <= current_extent->count) {
                extra = required_blocks;
                required_blocks = 0;
            }
            else {
                required_blocks -= current_extent->count;
                i_extent += 1;
            }
        }
        char *last_block = find_data_block(image, current_extent->start + extra - 1);
        a1fs_blk_t extent_left = inode->i_blocks - i_extent - 1;
        a1fs_blk_t count_left = current_extent->count - extra;
        uint64_t offset = size % A1FS_BLOCK_SIZE;
        
        if (offset) {
            memset((char *) last_block + offset, 0, A1FS_BLOCK_SIZE - offset);
        }
        
        while(count_left) {
            toggle_block_bit(image, 
                    (a1fs_blk_t) current_extent->start + extra);
            extra ++;
            count_left--;
        }
        
        a1fs_blk_t block;
        while(extent_left) {
            extra = 0;
            i_extent ++;
            current_extent = &(inode->i_block[i_extent]);
            required_blocks = current_extent->count;
            while (required_blocks) {
                block = current_extent->start+extra;
                toggle_block_bit(image, block);
                required_blocks--;
                extra ++;
            }
            i_extent --;
        }
        return 0;
    }
    
    else {
        a1fs_blk_t current_blocks = total_datablock_for_inode(inode);
        a1fs_blk_t extents = inode->i_blocks;
        a1fs_blk_t remaining_blocks = required_blocks - current_blocks;
        uint64_t current_size = oldsize;
        a1fs_blk_t i = 0;
        /*char *last_block;*/
        
        /*Certain blocks allocated by file*/
        if (extents > 0) {
            current_extent = &(inode->i_block[extents - 1]); 
            a1fs_blk_t last_block_num = current_extent->start + current_extent->count -1;
            while (i < remaining_blocks) {
                if (check_block_bitmap(image, last_block_num + 1 + i)) {
                    break;
                }
                i++;
            }
            current_extent->count += i;
            uint64_t start_offset = oldsize%A1FS_BLOCK_SIZE;
            if (remaining_blocks == 0) {
                null_padding_helper(image, 1, last_block_num, oldsize, size, 0);
            } else {
                null_padding_helper(image, 1, last_block_num, oldsize, 
                        oldsize + ((i+1)*A1FS_BLOCK_SIZE - start_offset), 0);
                current_size +=  (i+1)*A1FS_BLOCK_SIZE - start_offset;
            }
        } 

        
        /*left overs*/
        remaining_blocks -= i;
        
        if (remaining_blocks) {
            a1fs_blk_t free_blocks_idx = empty_block_bitmap(image);
            if(free_blocks_idx == 0) {
                return -ENOSPC;
            }
            current_extent = get_new_extent(inode);
            if (current_extent == NULL) {
                return -ENOSPC;
            }
            current_extent->start = free_blocks_idx;
            current_extent->count = remaining_blocks;
            null_padding_helper(image, 0, free_blocks_idx, current_size, size, 1);
        }
        return 0;
    }
}


/**
 * Read data from a file.
 *
 * Implements the pread() system call. Should return exactly the number of bytes
 * requested except on EOF (end of file) or error, otherwise the rest of the
 * data will be substituted with zeros. Reads from file ranges that have not
 * been written to must return ranges filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int a1fs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void)fi;// unused
    fs_ctx *fs = get_fs();

	//TODO: read data from the file at given offset into the buffer
    char *image = fs->image;
    a1fs_inode *inode;
    
    /*finding the inode of the given path*/
    find_inode_path(path, image, &inode);
    
    /*offset(where reading starts) larger than file size - unable to read*/
    if((uint64_t) offset >= inode->size) {
        return 0;
    }
    
    a1fs_blk_t offset_block = (a1fs_blk_t) (offset / A1FS_BLOCK_SIZE) + 1; 
    uint64_t offset_position = (uint64_t) offset % A1FS_BLOCK_SIZE;
    a1fs_blk_t offset_extent = 0;    /*extent at which offset starts*/
    a1fs_blk_t offset_index = offset_block;     /*The indicator for where in the extent the offset starts*/
    a1fs_extent *current_extent;
    
    /*this while block gives us the extent where offset starts from*/
    while (1) {
        current_extent = &(inode->i_block[offset_extent]); 
        if (offset_index <= current_extent->count) {
            offset_index -= 1;
            break;
        } else {
            offset_index -= current_extent->count; 
            offset_extent += 1;
        }
    }
    
    char *start = find_data_block(image, current_extent->start + offset_index) + offset_position;
    size_t bytes_left = inode->size - offset;
    size_t bytes_in_extent = (current_extent->count - offset_index)*A1FS_BLOCK_SIZE - offset_position; 
    size_t bytes_read = 0; 
    
    
    if (bytes_left >= size) {
        bytes_left = size;
    } 
    
    /*more bytes to be read after reading till the end of the extent*/
    while (bytes_in_extent < bytes_left) {
       memcpy(buf + bytes_read, start, bytes_in_extent);
       
       bytes_left -= bytes_in_extent;
       bytes_read += bytes_in_extent;
       
       offset_extent++;
       current_extent = &(inode->i_block[offset_extent]); 
       start = find_data_block(image, current_extent->start);
       bytes_in_extent = current_extent->count * A1FS_BLOCK_SIZE;
    }
    /*read the byte_left amount within the extent and it is over*/
    memcpy(buf + bytes_read, start, bytes_left);
    bytes_read += bytes_left;
    bytes_left = 0;
    
    return bytes_read;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Should return exactly the number of
 * bytes requested except on error. If the offset is beyond EOF (end of file),
 * the file must be extended. If the write creates a "hole" of uninitialized
 * data, future reads from the "hole" must return ranges filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int a1fs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: write data from the buffer into the file at given offset, possibly
	// "zeroing out" the uninitialized range
    char *sb = fs->image;
    a1fs_superblock *superblock = (a1fs_superblock *) sb;
    a1fs_inode *inode;
    find_inode_path(path, sb, &inode);
    if(inode->size < offset + size && a1fs_truncate(path, offset+size) != 0){
        return -ENOSPC;
    }
    else{
        a1fs_blk_t totalBlock = (a1fs_blk_t)offset / A1FS_BLOCK_SIZE + 1;
        a1fs_blk_t totalExtend = 0;
        a1fs_blk_t leftCount = 0;
        a1fs_extent *ext;
        while(1){
            ext = &(inode->i_block[totalExtend]);
            if(totalBlock <= ext->count){
                leftCount = totalBlock;
                break;
            }
            else{
                totalExtend += 1;
                totalBlock -= ext->count;
            }
        }
        uint64_t remain = (uint64_t)offset % A1FS_BLOCK_SIZE;
        char *start = sb + (superblock->first_data_block + ext->start + leftCount -1)*A1FS_BLOCK_SIZE + remain;
        size_t spaceLeft = (ext->count - leftCount + 1) * A1FS_BLOCK_SIZE - remain;
        if(spaceLeft < size){
            memcpy(start, buf, spaceLeft);
            size_t written = spaceLeft;
            size -= spaceLeft;
            totalExtend += 1;
            if(totalExtend == 27){
                return written;
            }
            ext = &(inode->i_block[totalExtend]);
            while(1){
                spaceLeft = (ext->count) * A1FS_BLOCK_SIZE;
                start = sb + (superblock->first_data_block + ext->start) * A1FS_BLOCK_SIZE;
                if(size > spaceLeft){
                    memcpy(start, buf+written, spaceLeft);
                    written += spaceLeft;
                    size -= spaceLeft;
                    totalExtend += 1;
                    if(totalExtend == 27){
                        return written;
                    }
                    ext = &(inode->i_block[totalExtend]);
                }
                else{
                    memcpy(start, buf+written, spaceLeft);
                    break;
                }

            }
        return written;
        }
        else{
            memcpy(start, buf, size);
            return size;
        }

    }
}


static struct fuse_operations a1fs_ops = {
	.destroy  = a1fs_destroy,
	.statfs   = a1fs_statfs,
	.getattr  = a1fs_getattr,
	.readdir  = a1fs_readdir,
	.mkdir    = a1fs_mkdir,
	.rmdir    = a1fs_rmdir,
	.create   = a1fs_create,
	.unlink   = a1fs_unlink,
	.rename   = a1fs_rename,
	.utimens  = a1fs_utimens,
	.truncate = a1fs_truncate,
	.read     = a1fs_read,
	.write    = a1fs_write,
};

int main(int argc, char *argv[])
{
	a1fs_opts opts = {0};// defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!a1fs_opt_parse(&args, &opts)) return 1;

	fs_ctx fs = {0};
	if (!a1fs_init(&fs, &opts)) {
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &a1fs_ops, &fs);
}
