#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ext2.h"


// Pointer to the beginning of the disk (byte 0)
unsigned char *disk;


void print_bitmap(unsigned char *bitmap, int num_bytes) {
	unsigned char in_use;
    for (int byte = 0; byte < num_bytes; byte++) {
	    for (int bit = 0; bit < 8; bit++) {
	        in_use = (bitmap[byte] & (1 << bit)) >> bit;
	        printf("%d", in_use);
		}
	    printf(" ");
    }
    printf("\n");
}


int in_use(unsigned char *bitmap, int index) {
    int byte = index / 8;
    int bit = (index % 8);
    return bitmap[byte] & (1 << bit);
}


char get_type(struct ext2_inode inode) {
	char type = '\0';
    if ((inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
	    type = 'd';
    } else if ((inode.i_mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
	    type = 'f';
    } else if ((inode.i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
	    type = 'l';
    }
	return type;
}

void print_inode(struct ext2_inode *inodes, int i_num) {
	struct ext2_inode inode = inodes[i_num-1];
    char type = get_type(inode);
    printf("[%d] type: %c size: %d links: %d blocks: %d\n", i_num, type, inode.i_size, inode.i_links_count, inode.i_blocks);
    
	printf("[%d] Blocks: ", i_num);
    for (int i = 0; i < inode.i_blocks/2; i++) {
	    printf(" %d", inode.i_block[i]);
    }
    printf("\n");
}


char get_dir_type(struct ext2_dir_entry *dir_entry) {
	char type = '\0';
    if ((dir_entry->file_type & EXT2_FT_DIR) == EXT2_FT_DIR) {
	    type = 'd';
    } else if ((dir_entry->file_type & EXT2_FT_REG_FILE) == EXT2_FT_REG_FILE) {
	    type = 'f';
    } else if ((dir_entry->file_type & EXT2_FT_SYMLINK) == EXT2_FT_SYMLINK) {
	    type = 'l';
    }
	return type;
}


void print_inodes(struct ext2_inode *inodes, unsigned char *inode_bitmap, int num_inodes) {
    print_inode(inodes, EXT2_ROOT_INO);
    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < num_inodes; i++) {
    	if (in_use(inode_bitmap, i)) {
	        print_inode(inodes, i+1);
		}
    }
}


void print_block(unsigned char *dir) {
	int index = 0;
    while (index < EXT2_BLOCK_SIZE) {
		struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (dir + index);
		char type = get_dir_type(dir_entry);
		printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n", dir_entry->inode, dir_entry->rec_len,
		    dir_entry->name_len, type, dir_entry->name_len, dir_entry->name);
	    index += dir_entry->rec_len;
	}
}


void print_blocks(struct ext2_inode *inodes, unsigned char *inode_bitmap, int num_inodes) {
	for (int i = 0; i < inodes[EXT2_ROOT_INO-1].i_blocks/2; i++) {
        printf("   DIR BLOCK NUM: %d (for inode %d)\n", inodes[EXT2_ROOT_INO-1].i_block[i], EXT2_ROOT_INO);
		unsigned char *dir = (unsigned char *)(disk + EXT2_BLOCK_SIZE*(inodes[EXT2_ROOT_INO-1].i_block[i]));
        print_block(dir);
    }
	for (int i = EXT2_GOOD_OLD_FIRST_INO; i < num_inodes; i++) {
		char type = get_type(inodes[i]);
        if (in_use(inode_bitmap, i) && (type == 'd')) {
            for (int j = 0; j < inodes[i].i_blocks/2; j++) {
                printf("   DIR BLOCK NUM: %d (for inode %d)\n",inodes[i].i_block[j], i+1);
				unsigned char *dir = (unsigned char *)(disk + EXT2_BLOCK_SIZE*(inodes[i].i_block[j]));
                print_block(dir);
            }
        }
    }
}


int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
		exit(1);
	}
	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	// Map the disk image into memory so that we don't have to do any reads and writes
	disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	struct ext2_super_block *sb = (struct ext2_super_block*)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE*2);
	unsigned char *block_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE*gd->bg_block_bitmap);
	unsigned char *inode_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE*gd->bg_inode_bitmap);
	struct ext2_inode *inodes = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE*gd->bg_inode_table);

	printf("Inodes: %d\n", sb->s_inodes_count);
	printf("Blocks: %d\n", sb->s_blocks_count);
	printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);

	printf("Block bitmap: ");
	print_bitmap(block_bitmap, sb->s_blocks_count/8);

	printf("Inode bitmap: ");
	print_bitmap(inode_bitmap, sb->s_inodes_count/8);

    printf("\n");
	printf("Inodes:\n");
	print_inodes(inodes, inode_bitmap, sb->s_inodes_count);

    printf("\n");
	printf("Directory Blocks:\n");
    print_blocks(inodes, inode_bitmap, sb->s_inodes_count);

    return 0;
}
