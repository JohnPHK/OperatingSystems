#include "util.h"
#include <string.h>
#include <stdio.h>

int find_inode_path(const char *path, char *sb, a1fs_inode **inode){
    a1fs_inode *tempInode = find_inode_num(sb, 1); //root Inode num which is 1
    char newPath[A1FS_NAME_MAX];
    strcpy(newPath, path);
    char *token = strtok(newPath, "/");
    int newInodeNum;
    while(token != NULL){
        newInodeNum = find_inode_name(token, sb, tempInode);
        if(newInodeNum == -1){
            return -1;
        }
        tempInode = find_inode_num(sb, newInodeNum);
        token = strtok(NULL, "/");
        if((!(S_ISDIR(tempInode->mode))&& token != NULL)){
            return -2;
        }
    }
    *inode = tempInode;
    return 0;
}

// return the ino Num
int find_inode_name(char *name, char *image, a1fs_inode *inode){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    uint32_t numExtend = inode->i_blocks;
    for(uint32_t i=0;i < numExtend;i++){
        a1fs_extent extend = inode->i_block[i];
        a1fs_blk_t start = extend.start;
        a1fs_blk_t count = extend.count;
        int numOfEntry = A1FS_BLOCK_SIZE/sizeof(a1fs_dentry);
        a1fs_dentry *entry = (a1fs_dentry *) (image + (sb->first_data_block + start) * A1FS_BLOCK_SIZE);
        a1fs_blk_t j = 0;
        while(j < numOfEntry*count){
            if(strcmp(entry[j].name, name) == 0 && entry[j].ino != 0){
                return entry[j].ino;
            }
            j++;
        }
    }
    return -1; // -1 means not found
}

a1fs_inode *find_inode_num(char *image, a1fs_ino_t num){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    return (a1fs_inode *)(image + (sb->first_inode_block)*A1FS_BLOCK_SIZE + sizeof(a1fs_inode)*(num-1));
}

a1fs_blk_t total_datablock_for_inode(a1fs_inode *inode){
  a1fs_blk_t total = 0;
  for(uint32_t i = 0; i < inode->i_blocks; i++){
    total += inode->i_block[i].count;
  }
  return total;
}

int read_entries(fuse_fill_dir_t filler, char *image, a1fs_inode *inode, void *buf){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    uint32_t numExtend = inode->i_blocks;
    for(uint32_t i=0;i < numExtend;i++){
        a1fs_extent extend = inode->i_block[i];
        a1fs_blk_t start = extend.start;
        a1fs_blk_t count = extend.count;
        int numOfEntry = A1FS_BLOCK_SIZE/sizeof(a1fs_dentry);
        a1fs_dentry *entry = (a1fs_dentry *) (image + (sb->first_data_block + start) * A1FS_BLOCK_SIZE);
        a1fs_blk_t j = 0;
        while(j < numOfEntry*count){
            if(entry[j].ino != 0){
                if(filler(buf, entry[j].name , NULL, 0) != 0) {
                    return -1;}
            }
            j++;
        }
    }
    return 0;
}

//finding the first inode bit availabe from inode bitmap
a1fs_ino_t empty_inode_bitmap(char *image){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    char *bitmap = image + sb->inode_bitmap*A1FS_BLOCK_SIZE;
    a1fs_ino_t total_size = sb-> inodes_count;
    a1fs_ino_t byte = 0;
    a1fs_ino_t bit = 1;
    while(total_size){
        if(bit == 8){
            byte += 1;
            bit = 0;
        }
        if((bitmap[byte]&(1<<bit)) == 0){
            return byte*8 + bit;
        }
        bit ++;
        total_size --;
    }
    return 0;
}

int toggle_inode_bit(char *image, a1fs_ino_t num){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    unsigned char *inode_bitmap = (unsigned char *)(image + sb->inode_bitmap*A1FS_BLOCK_SIZE);
    a1fs_blk_t byte = 0;
    int bit = 0;
    byte = num / 8;
    bit = num % 8;
    inode_bitmap[byte] = inode_bitmap[byte]^(1<<bit);
    if ((inode_bitmap[byte] & (1<<bit)) == 0) {
        sb->free_inodes_count++;
    } else {
        sb->free_inodes_count--;
    }
    return 0;
}


int toggle_block_bit(char *image, a1fs_blk_t num){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    unsigned char *block_bitmap = (unsigned char *)
        (image + sb->datablock_bitmap*A1FS_BLOCK_SIZE);
    a1fs_blk_t byte = 0;
    int bit = 0;
    byte = num / 8;
    bit = num % 8;
    block_bitmap[byte] = block_bitmap[byte]^(1<<bit);
    if ((block_bitmap[byte] & (1<<bit)) == 0) {
        sb->free_blocks_count++;
    } else {
        sb->free_blocks_count--;
    }
    return 0;
}


a1fs_blk_t empty_block_bitmap(char *image){
  a1fs_superblock *sb = (a1fs_superblock *) image;
  char *bitmap = image + sb->datablock_bitmap *A1FS_BLOCK_SIZE;
  a1fs_blk_t bit_map_size = sb-> blocks_count;
  a1fs_blk_t byte = 0;
  a1fs_blk_t bit = 1;
  while(bit_map_size){
    if(bit == 8){
      byte += 1;
      bit = 0;
    }
    if((bitmap[byte]&(1<<bit)) == 0){
        return byte*8 + bit;
        }
    bit ++;
    bit_map_size --;
  }
  return 0;
}

int check_block_bitmap(char *image, a1fs_blk_t num){
  a1fs_superblock *sb = (a1fs_superblock *)image;
  unsigned char *block_bitmap = (unsigned char *)(image + sb->datablock_bitmap *A1FS_BLOCK_SIZE);
  a1fs_blk_t byte = num / 8;
  int bit = num % 8;
  return block_bitmap[byte]&(1<<bit);
}


int change_parent(char * image, a1fs_inode *parent, char *name, a1fs_ino_t inodeNo){
    a1fs_superblock *sb = (a1fs_superblock *) image;
    a1fs_blk_t freeDataBit;
    a1fs_extent* lastExt;
    if(parent->i_blocks == 0){
        parent->i_blocks++;
        freeDataBit = empty_block_bitmap(image);
        if(freeDataBit == 0){
            return -1;
        }
        lastExt = &(parent->i_block[parent->i_blocks-1]);
        lastExt->start = freeDataBit;
        lastExt->count = 1;
        toggle_block_bit(image, freeDataBit);
        }
    else if((parent->size %A1FS_BLOCK_SIZE) == (2*sizeof(a1fs_dentry))){
        lastExt = &(parent->i_block[parent->i_blocks-1]);
        int result = check_block_bitmap(image, lastExt->count+lastExt->start);
        if(result == 0){
            toggle_block_bit(image,lastExt->count+lastExt->start);
            lastExt->count++;
        }
        else{
            if(parent->i_blocks == NUM_BLOCK){
                return -1;
            }
            parent->i_blocks++;
            freeDataBit = empty_block_bitmap(image);
            if(freeDataBit == 0){
                return -1;}
            toggle_block_bit(image, freeDataBit);
            a1fs_extent new = {freeDataBit,1};
            parent->i_block[parent->i_blocks-1] = new;
        }
    }
        parent->size += sizeof(a1fs_dentry);
        lastExt = &(parent->i_block[parent->i_blocks-1]);
        a1fs_dentry *entry = (a1fs_dentry *)(image + (sb->first_data_block + lastExt->start + lastExt->count-1) * A1FS_BLOCK_SIZE);
        for(a1fs_ino_t i =0; i < A1FS_BLOCK_SIZE/(sizeof(a1fs_dentry)); i++){
            if(entry[i].ino == 0){
                entry[i].ino = inodeNo;
                strcpy(entry[i].name, name);
                return 0;
                }
        }
  return 0;
}



int remove_entry(char *image, a1fs_inode *parent, char *name){
    a1fs_superblock *sb = (a1fs_superblock *)image;
    for(uint32_t i=0;i < parent->i_blocks;i++){
        a1fs_extent extend = parent->i_block[i];
        a1fs_blk_t start = extend.start;
        a1fs_blk_t count = extend.count;
        int numOfEntry = A1FS_BLOCK_SIZE/sizeof(a1fs_dentry);
        a1fs_dentry *entry = (a1fs_dentry *) (image + (sb->first_data_block + start) * A1FS_BLOCK_SIZE);
        a1fs_blk_t j = 0;
        while(j < numOfEntry*count){
            if(strcmp(entry[j].name, name) == 0 && entry[j].ino != 0){
                entry[j].ino = 0;
                break;
            }
            j++;
        }
    }
    return 0;
}

char *find_data_block(char *image, a1fs_ino_t number) {
    a1fs_superblock *sb = (a1fs_superblock *)image;
    return (image +(sb->first_data_block + number)*A1FS_BLOCK_SIZE);
}

char *get_block(char *image, a1fs_ino_t block_number) {
    return (image + block_number*A1FS_BLOCK_SIZE);
}

a1fs_extent *get_new_extent(a1fs_inode *inode) {
    if (inode->i_blocks >= NUM_BLOCK) {
        return NULL;
    }
    inode->i_blocks++;
    return &(inode->i_block[inode->i_blocks - 1]);

}

/*precondition is that the data blocks must be available consecutively within 
 * the size*/
void null_padding_helper(char *image, int start_offset, a1fs_blk_t to_toggle_index, 
        uint64_t current_size, uint64_t new_size, int free_indicator) {
    
    a1fs_blk_t i_toggle = to_toggle_index;
    char *i_start; 
    if (start_offset == 1) {
        i_start = find_data_block(image, to_toggle_index) + current_size%A1FS_BLOCK_SIZE;
    } else {
        i_start = find_data_block(image, to_toggle_index);
    }
    uint64_t i_size = new_size - current_size;
    int started = 0;
    
    while (i_size) {
        if (started == 0) {
            if (current_size % A1FS_BLOCK_SIZE > 0) {
                if ((new_size - current_size) + (current_size % A1FS_BLOCK_SIZE) < A1FS_BLOCK_SIZE) {
                    memset(i_start, 0, new_size - current_size);
                    i_size -= (new_size - current_size);
                } else {
                    /*filling up rest of the block*/
                    memset(i_start, 0, A1FS_BLOCK_SIZE - (current_size % A1FS_BLOCK_SIZE));
                    i_start += A1FS_BLOCK_SIZE - (current_size % A1FS_BLOCK_SIZE);
                    i_size -= A1FS_BLOCK_SIZE - (current_size % A1FS_BLOCK_SIZE);
                }
            }
            if (free_indicator == 0) {
                i_toggle++;
            }
            started = 1;
        } else {
            if (i_size < A1FS_BLOCK_SIZE) {
                memset(i_start, 0, i_size);
                i_size -= i_size;
                toggle_block_bit(image, i_toggle);
            } else {
                memset(i_start, 0, A1FS_BLOCK_SIZE);
                i_size -= A1FS_BLOCK_SIZE;
                toggle_block_bit(image, i_toggle);
            }
            i_toggle++;
        }
    }
}
