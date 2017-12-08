#include "sfs_api.h"
#include "disk_emu.h"
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_NUM_BLOCKS 1024
#define BLOCK_SIZE 1024
#define MAX_INODES 100
#define MAX_OPEN_FILES 90
#define DIRECT_POINTERS 12
#define INDIRECT_POINTERS 1
#define MAX_FILE_NAME 20

#define ROOT_DIRECTORY_INODE 0
#define ROOT_FD 0

#define MAX_FILE_SIZE BLOCK_SIZE * (DIRECT_POINTERS + BLOCK_SIZE/sizeof(int))

#define IS_FREE 1
#define IS_USED 0

#define AHMADOV_ORUJ "sfs_disk.dsk"

typedef struct super_block {
	int magic;
	int block_size;
	int fs_size;
	int inode_table_len;
	int root_dir_inode;
} super_block_t;

typedef struct block_pointer {
	int direct_pointers[DIRECT_POINTERS];
	int indirect_pointer;
} block_pointer_t;

typedef struct inode {
	int mode;
	int link_cnt;
	int uid;
	int gid;
	int size;
	block_pointer_t block_pointers;
} inode_t;

typedef struct directory_entry {
    int num; // represents the inode number of the entery.
    char name[MAX_FILE_NAME]; // represents the name of the entry.
} directory_entry_t;


typedef struct fd_entry {
	int inode_number;
	inode_t *inode;
	int rd_write_ptr;
	char status;
} fd_entry_t;

void init_super_block();
void init_open_file_desc_table();
int get_free_data_block();
int get_free_fd();
void write_inode_to_disk(int inode_number, inode_t *inode);

inode_t *get_inode(int inode_number);

int get_inode_number(const char *name);
int get_free_inode();
void remove_inode_from_root(char *filename);
void reset_directory();
int get_block_pointer(block_pointer_t *pointers, int disk_block_num);
void free_direct_pointers(block_pointer_t *pointers);
void free_indirect_pointer(block_pointer_t *pointers);
void free_block_pointer(block_pointer_t *pointers);

directory_entry_t create_new_file(char *name);

super_block_t super_block;

fd_entry_t fd_table[MAX_OPEN_FILES]; // this stays in memory

// Formats the virtual disk implemented by the disk emulator and creates an instance
// of the simple file system on top of it.
void mksfs(int fresh) {

	// Bitmap
	char free_bitmap[BLOCK_SIZE];

	// Initialize File Descriptor table
	for (int index = 0; index < MAX_OPEN_FILES; index++)
	{
		fd_table[index].status = IS_FREE;
	}

	if (fresh == 1) {
		init_fresh_disk(AHMADOV_ORUJ, BLOCK_SIZE, MAX_NUM_BLOCKS);

		init_super_block();

		// Super Block on top: Write to disk
		write_blocks(0, 1, (void *) &super_block);

		// Bitmap to hold inodes
		memset(free_bitmap, IS_FREE, BLOCK_SIZE);
		free_bitmap[ROOT_DIRECTORY_INODE] = IS_USED;

		// Bitmap: Write to disk

		write_blocks(1, 1, free_bitmap);
		memset(free_bitmap, IS_FREE, BLOCK_SIZE);
		write_blocks(2, 1, free_bitmap);

		// Initialize open file descriptor table
		init_open_file_desc_table();

		// inode of root directory
		write_inode_to_disk(fd_table[ROOT_FD].inode_number, fd_table[ROOT_FD].inode);

	}
	else {
		init_disk(AHMADOV_ORUJ, BLOCK_SIZE, MAX_NUM_BLOCKS);
		fd_table[ROOT_FD].status = IS_USED;
		fd_table[ROOT_FD].inode_number = ROOT_DIRECTORY_INODE;
		fd_table[ROOT_FD].inode = get_inode(fd_table[ROOT_FD].inode_number);
		fd_table[ROOT_FD].rd_write_ptr = fd_table[ROOT_FD].inode->size;
	}

	return;
}

int sfs_getnextfilename(char *fname) {

	static directory_entry_t *root_directory;
	int num_of_files;

	static int index = 0;
	if (index == 0) {
		root_directory = malloc(fd_table[ROOT_FD].inode->size);
		fd_table[ROOT_FD].rd_write_ptr = 0;
		sfs_fread(ROOT_FD, (char *) root_directory, fd_table[ROOT_FD].inode->size);
		num_of_files = (fd_table[ROOT_FD].inode->size) / sizeof(directory_entry_t);
	}

	while (index < num_of_files) {
		strcpy(fname, root_directory[index].name);
		index++;
		return 1;
	}

	index = 0;
	return 0;
}

// Returns the size of a given file.
int sfs_getfilesize(const char* path) {
	inode_t *inode;
	int inode_number = get_inode_number(path);
	if (inode_number != -1)
	{
		inode = get_inode(inode_number);
		return inode->size;
	}
	return -1;
}

//Opens a file and returns the index that corresponds to the newly opened file in the file descriptor table.
int sfs_fopen(char *name) {

	// If name exceeds max file name size, return -1
	if (strlen(name) > MAX_FILE_NAME)
	{
		return -1;
	}

	int inode_number = get_inode_number(name);
	// begin from index 1 since index 0 is reserved for root directory
	for (int i = 1; i < MAX_OPEN_FILES; i++) // check what's already open
	{
		// if file is already open then return file descriptor index.
		if ((fd_table[i].status == IS_USED) && (fd_table[i].inode_number == inode_number)) {
						return i;
		}
	}

	// check for free fd
	int free_fd = get_free_fd();
	if (free_fd == -1)
	{
		return -1;
	}

	inode_t *inode;
	directory_entry_t new_file;

	// check if file exists already
	if (inode_number != -1)
	{
		inode = get_inode(inode_number);
		// update open file descriptor table entry for new file
		fd_table[free_fd].inode_number = inode_number;
		fd_table[free_fd].rd_write_ptr = inode->size;
		fd_table[free_fd].inode = inode;
	}
	else	{
		// allocate inode for new file
		inode = calloc(1, sizeof(inode_t));

		// create new file
		new_file = create_new_file(name);

		// go to end to store new file
		fd_table[ROOT_FD].rd_write_ptr = fd_table[ROOT_FD].inode->size;
		sfs_fwrite(ROOT_FD, (char *) &new_file, sizeof(directory_entry_t));
		write_inode_to_disk(fd_table[ROOT_FD].inode_number, fd_table[ROOT_FD].inode);

		// Update FD table
		fd_table[free_fd].inode_number = new_file.num;
		fd_table[free_fd].rd_write_ptr = inode->size;
		fd_table[free_fd].inode = inode;
	}
	return free_fd;
}

int sfs_fclose(int fileID){

	// check if fd at fileID is free
	if (fd_table[fileID].status == IS_FREE)
	{
		return -1;
	}
	fd_table[fileID].status = IS_FREE;
	write_inode_to_disk(fd_table[fileID].inode_number, fd_table[fileID].inode);

	free(fd_table[fileID].inode);
	return 0;
}

int sfs_fread(int fileID, char *buf, int length) {
	if (fd_table[fileID].status == IS_FREE) {
		return -1;
	}

	inode_t *inode = fd_table[fileID].inode;

	int rw_pointer = fd_table[fileID].rd_write_ptr;
	int disk_block_num = rw_pointer / BLOCK_SIZE;
	int written_size = rw_pointer % BLOCK_SIZE;

	int block_pointer;

	char block[BLOCK_SIZE];

	if (fd_table[fileID].rd_write_ptr + length > inode->size)
	{
		length = inode->size - rw_pointer;
	}

	if (rw_pointer + length <= BLOCK_SIZE) {
		block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
		read_blocks(block_pointer, 1, block);
		memcpy(buf, &block[written_size], length);
		fd_table[fileID].rd_write_ptr += length;
		return length;
	}

	int final_block = ((written_size + length) / BLOCK_SIZE) + disk_block_num;

	block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);

	read_blocks(block_pointer, 1, block);
	memcpy(buf, &block[written_size], BLOCK_SIZE - written_size);
	buf = buf + (BLOCK_SIZE - written_size);
	disk_block_num++;

	int extraBytes = (written_size + length) % BLOCK_SIZE;

	while (disk_block_num < final_block) {
		block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
		read_blocks(block_pointer, 1, block);
		memcpy(buf, block, BLOCK_SIZE);
		buf += BLOCK_SIZE;
		disk_block_num++;
	}

	block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
	read_blocks(block_pointer, 1, block);
	memcpy(buf, block, extraBytes);

	fd_table[fileID].rd_write_ptr += length;
	return length;
}

int sfs_fwrite(int fileID, const char *buf, int length){
	if ((fd_table[fileID].status == IS_FREE) || (fd_table[fileID].rd_write_ptr + length > MAX_FILE_SIZE))
	{
		return -1;
	}

	inode_t *inode = fd_table[fileID].inode;

	int rw_pointer = fd_table[fileID].rd_write_ptr;
	int disk_block_num = rw_pointer / BLOCK_SIZE; // index within inode of disk block
	// Amount of written blocks
	int written_size = rw_pointer % BLOCK_SIZE;

	int block_pointer;

	char block[BLOCK_SIZE];

	// check if the data to be written will fit in one block
	if (written_size + length <= BLOCK_SIZE)
	{
		// find a block pointer to use to store data
		block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
		if (block_pointer != -1) // free data block to write data
		{
			read_blocks(block_pointer, 1, block);
			memcpy(&block[written_size], buf, length); // start appending to data that has already been written there
			write_blocks(block_pointer, 1, block);
			fd_table[fileID].rd_write_ptr = rw_pointer + length;
			if (rw_pointer + length > inode->size)
			{
				inode->size = rw_pointer + length;
			}
			write_inode_to_disk(fd_table[fileID].inode_number, fd_table[fileID].inode);
			return length;
		}
		return -1;
	}

	int final_block = ((written_size + length) / BLOCK_SIZE) + disk_block_num;

	block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
	if (block_pointer == -1) {
		return -1;
	}
	// fill first block until the end then move onto the next block
	read_blocks(block_pointer, 1, block);
	memcpy(&block[written_size], buf, BLOCK_SIZE - written_size);
	write_blocks(block_pointer, 1, block);

	buf = buf + (BLOCK_SIZE - written_size); // move pointer the amount you have appended
	disk_block_num++; // increment to the next block you need to write to

	// all blocks in here you can fill them up fully
	while (disk_block_num < final_block) {
		block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
		if (block_pointer == -1)
		{
			return -1;
		}
		memcpy(block, buf, BLOCK_SIZE);
		write_blocks(block_pointer, 1, block);
		buf += BLOCK_SIZE;
		disk_block_num++;
	}
	// write the remaining data into a final block that will fill it partially
	int extraBytes = (written_size + length) % BLOCK_SIZE;

	block_pointer = get_block_pointer(&(inode->block_pointers), disk_block_num);
	if (block_pointer == -1) {
		return -1;
	}
	read_blocks(block_pointer, 1, block);
	memcpy(block, buf, extraBytes);
	write_blocks(block_pointer, 1, block);

	fd_table[fileID].rd_write_ptr = rw_pointer + length;
	if (rw_pointer + length > inode->size)
	{
		inode->size = rw_pointer + length;
	}
	write_inode_to_disk(fd_table[fileID].inode_number, fd_table[fileID].inode);
	return length;
}

int sfs_fseek(int fileID, int loc){
	// check if file is open.
	if (fd_table[fileID].status == IS_FREE)
	{
		return -1;
	}

	if (loc > fd_table[fileID].inode->size){
		loc = fd_table[fileID].inode->size;
	}
	fd_table[fileID].rd_write_ptr = loc;
	return 0;
}

int sfs_remove(char *file) {
	int inode_number = get_inode_number(file);
	if (inode_number == -1) // no such file
	{
		return -1;
	}
	char free_bitmap[BLOCK_SIZE];

	inode_t *inode = get_inode(inode_number);

	// free data blocks...also frees position in free data block bitmap
	free_block_pointer(&(inode->block_pointers));

	// free bitmaps for inode
	read_blocks(1, 1, free_bitmap);
	free_bitmap[inode_number] = IS_FREE;
	write_blocks(1, 1, free_bitmap);

	remove_inode_from_root(file);

	free(inode);

	return 0;
}

void init_super_block() {
	super_block.magic = 0xAABB0005;
	super_block.block_size = BLOCK_SIZE;
	super_block.fs_size = MAX_NUM_BLOCKS * BLOCK_SIZE;
	super_block.inode_table_len = MAX_INODES;
	super_block.root_dir_inode = ROOT_DIRECTORY_INODE;
}

void init_open_file_desc_table()
{
	// create entry in open file descriptor table for ROOT DIRECTORY
	// File descriptor 0 is reserver for ROOT DIRECTORY
	fd_table[ROOT_FD].inode = calloc(1, sizeof(inode_t)); // create root directory inode
	fd_table[ROOT_FD].inode->size = 0; // no contents in root directory yet
	fd_table[ROOT_FD].inode->link_cnt = 1;
	fd_table[ROOT_FD].inode->uid = 0;
	fd_table[ROOT_FD].inode->gid = 0;
	fd_table[ROOT_FD].inode->mode = 0x755;
	fd_table[ROOT_FD].inode_number = ROOT_DIRECTORY_INODE;

	fd_table[ROOT_FD].rd_write_ptr = 0;
	fd_table[ROOT_FD].status = IS_USED;
}

int get_free_data_block()
{
	char free_bitmap[BLOCK_SIZE];
	read_blocks(2, 1, free_bitmap);

	int i;
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		if (free_bitmap[i] == IS_FREE) // there is a free data block to write to
		{
			free_bitmap[i] = IS_USED;
			write_blocks(2, 1, free_bitmap); // update free block bitmap
			return i + 18;
		}
	}
	return -1; // no free data blocks left

}

int get_free_fd()
{
	int i;
	// index 0 of open file descriptor table is reserved for ROOT DIRECTORY so start at inex 1
	for (i = 1; i < MAX_OPEN_FILES; i++)
	{
		if (fd_table[i].status == IS_FREE)
		{
			fd_table[i].status = IS_USED;
			return i;
		}
	}
	return -1;
}

void write_inode_to_disk(int inode_number, inode_t *inode)
{
	inode_t *inodeTable;
	char buffer[BLOCK_SIZE];

	// Figuring out where the inode should go and which block number inode will belong to
	int numberOfInodesPerBlock = BLOCK_SIZE/sizeof(inode_t);
	int blockNumber = inode_number/numberOfInodesPerBlock;
	int inodeIndex = inode_number % numberOfInodesPerBlock;

	// inodes range from block 3 to block 17
	read_blocks(3 + blockNumber, 1, buffer);

	inodeTable = (inode_t *) buffer;

	// add inode to inode table
	memcpy(&inodeTable[inodeIndex], inode, sizeof(inode_t));

	// write inode data block to disk
	write_blocks(3 + blockNumber, 1, buffer);
}

inode_t * get_inode(int inode_number)
{
	inode_t *inodeTable;
	inode_t *inode;
	char buffer[BLOCK_SIZE];

	// Figuring out where the inode should go and which block number inode will belong to
	int numberOfInodesPerBlock = BLOCK_SIZE/sizeof(inode_t);
	int blockNumber = inode_number/numberOfInodesPerBlock;
	int inodeIndex = inode_number % numberOfInodesPerBlock;

	// read data blocks from disk. inodes start at block index 3
	read_blocks(3 + blockNumber, 1, buffer);

	inodeTable = (inode_t *) buffer;

	inode = malloc(sizeof(inode_t));

	// copy inode at index into inode variable
	memcpy(inode, &inodeTable[inodeIndex], sizeof(inode_t));
	return inode;
}

int get_inode_number(const char *name)
{
	int directorySize = fd_table[ROOT_FD].inode->size; // get root directory size
	void *buffer = (void *) malloc(directorySize);
	directory_entry_t *files = (directory_entry_t *) buffer;

	fd_table[ROOT_FD].rd_write_ptr = 0;

	sfs_fread(ROOT_FD, buffer, directorySize); // read in the directory contents and loop through it

	int num_of_files = directorySize/sizeof(files);
	int i;
	int inode_number;
	for (i = 0; i < num_of_files; i++) // loop through all files found from reading disk
	{
		if (strcmp(files->name, name) == 0) // found the match
		{
			inode_number = files->num;
			return inode_number;
		}
		files++; // move onto next file
	}
	free(buffer);
	return -1;
}

int get_free_inode()
{
	char block[BLOCK_SIZE];
	read_blocks(1, 1, block); // read in inode bit map to find which one is free
	int i;
	for (i = 0; i < MAX_INODES; i++)
	{
		if (block[i] == IS_FREE)
		{
			block[i] = IS_USED;
			write_blocks(1, 1, block);
			return i; // inode blocks start at index 3 to index 17
		}
	}
	return -1;
}

void remove_inode_from_root(char *filename)
{
	int directorySize = fd_table[ROOT_FD].inode->size;
	void *buffer = (void *) malloc(directorySize);
	directory_entry_t *file = (directory_entry_t *) buffer;
	int num_of_files = directorySize/sizeof(directory_entry_t);

	fd_table[ROOT_FD].rd_write_ptr = 0;
	sfs_fread(ROOT_FD, buffer, directorySize); // read in bytes of root directory

	int i = 0;
	int remainingFiles;
	while (i < num_of_files) // loop through each file
	{
		remainingFiles = num_of_files-i-1;
		if (strcmp(file->name, filename) == 0) // found file to remove
		{
			// use memmove to overwrite file that will be deleted
			// moving memory one position right of file that will be deleted and overwrite file that will be deleted
			memmove(file, &file[1], sizeof(directory_entry_t) * remainingFiles);
			reset_directory();
			sfs_fwrite(ROOT_FD, buffer, directorySize - sizeof(directory_entry_t));
			write_inode_to_disk(fd_table[ROOT_FD].inode_number, fd_table[ROOT_FD].inode);
			free(buffer);
			return;
		}
		i++;
		file++; // move pointer to next file in the directory
	}
}

void reset_directory()
{
	fd_table[ROOT_FD].rd_write_ptr = 0;
	fd_table[ROOT_FD].inode->size = 0;
	free_block_pointer(&(fd_table[ROOT_FD].inode->block_pointers));
	memset(fd_table[ROOT_FD].inode, 0, sizeof(inode_t));

}

// returns data block location on disk for a block pointer inside an inode
int get_block_pointer(block_pointer_t *pointers, int disk_block_num) {
	// indirect pointer will be pointing to this block
	int indirect_pointers[BLOCK_SIZE/sizeof(int)];

	// check if block number given is for a direct pointer
	if (disk_block_num < DIRECT_POINTERS)
	{
		if (pointers->direct_pointers[disk_block_num] == 0)
		{
			pointers->direct_pointers[disk_block_num] = get_free_data_block();
			if (pointers->direct_pointers[disk_block_num] == -1)
			{
				return -1;
			}
		}
		return pointers->direct_pointers[disk_block_num];
	}

	// first time trying to access indirect pointer. file grew big enough and needs more than 12 direct data blocks
	if (pointers->indirect_pointer == 0)
	{
		pointers->indirect_pointer = get_free_data_block();
		if (pointers->indirect_pointer != -1)
		{
			memset(indirect_pointers, 0, BLOCK_SIZE); // set all pointer locations to 0
			write_blocks(pointers->indirect_pointer, 1, (void *) indirect_pointers); // write block of pointers here where there is a free block
		}
		else // no more free blocks
		{
			return -1;
		}
	}
	// go to location where indirect pointer points to and get all those other pointers to data blocks
	read_blocks(pointers->indirect_pointer, 1, (void *) indirect_pointers);
	if (indirect_pointers[disk_block_num - DIRECT_POINTERS] == 0)
	{
		indirect_pointers[disk_block_num - DIRECT_POINTERS] = get_free_data_block();
		if (indirect_pointers[disk_block_num - DIRECT_POINTERS] != -1)
		{
			write_blocks(pointers->indirect_pointer, 1, (void *) indirect_pointers); // write back onto disk the list of indirect pointers
		}
		else // no more free blocks
		{
			return -1;
		}
	}
	return indirect_pointers[disk_block_num - DIRECT_POINTERS];
}

void free_direct_pointers(block_pointer_t *pointers)
{
	char free_bitmap[BLOCK_SIZE];

	read_blocks(2, 1, free_bitmap); // get free data block bit map

	int block_index;

	int i;
	for (i = 0; i <= DIRECT_POINTERS; i++)
	{
		block_index = pointers->direct_pointers[i]; // returns location of data block on disk
		if (block_index == 0) // no more data can exist after this iteration
		{
			write_blocks(2, 1, free_bitmap); // write back to disk the updated bitmap of free data blocks
			return;
		}
		block_index = block_index - 18; // represents location of data block on disk
		free_bitmap[block_index] = IS_FREE;
	}
	write_blocks(2, 1, free_bitmap); // write updated bitmap back to disk
}

void free_indirect_pointer(block_pointer_t *pointers)
{
	int indirect_pointers[BLOCK_SIZE/sizeof(int)];
	char free_bitmap[BLOCK_SIZE];
	int num_of_pointers = BLOCK_SIZE/sizeof(int);

	// find free data block from bitmap
	read_blocks(2, 1, free_bitmap);

	// get array of indirect pointers
	read_blocks(pointers->indirect_pointer, 1, (void *) indirect_pointers);

	int block_index;

	for (int index = 0; index < num_of_pointers; index++)
	{
		block_index = indirect_pointers[index];
		if (block_index == 0) {
			// write block to bitmap
			write_blocks(2, 1, free_bitmap);
			return;
		}
		block_index = block_index - 18;
		free_bitmap[block_index] = IS_FREE;
	}
}

void free_block_pointer(block_pointer_t *pointers)
{
	// Free all direct pointers
	free_direct_pointers(pointers);
	// Check block number 18 to see if indirect pointer is present. Free if yes.
	if (pointers->indirect_pointer >= 18)
	{
		free_indirect_pointer(pointers);
	}
}

directory_entry_t create_new_file(char *name) {
	// Find free inode for new file
	int inode_number = get_free_inode();
	directory_entry_t new_file;
	new_file.num = inode_number;
	strcpy(new_file.name, name);
	return new_file;
}
