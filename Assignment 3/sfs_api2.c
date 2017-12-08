#include "sfs_api.h"
#include "disk_emu.h"
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_BLOCKS 18 + 512 // 18 is for super block to start of datablock. 512 for 512 data blocks available
#define BLOCK_SIZE 512
#define MAX_INODES 100
#define MAX_OPEN_FILES 98 // With my design, 98 is the max of open files
#define DIRECT_POINTERS 12
#define INDIRECT_POINTERS 1
#define MAX_FILENAME 20

#define ROOT_DIRECTORY_INODE 0
#define ROOT_FILE_DESCRIPTOR 0

#define MAX_FILE_SIZE BLOCK_SIZE * (DIRECT_POINTERS + BLOCK_SIZE/sizeof(int)) // 512 * (12 + 512/4) = Around 70,000

#define FREE 1
#define USED 0

#define DISK_FILE "sfs_disk.dsk"

typedef struct super_block {
	int magic;
	int block_size;
	int fs_size;
	int inode_table_len;
	int root_dir_inode;
} super_block_t;

typedef struct block_pointer {
	int directPointers[DIRECT_POINTERS];
	int indirectPointer;
} block_pointer_t;

typedef struct inode {
	int mode;
	int link_cnt;
	int uid;
	int gid;
	int size;
	block_pointer_t block_pointers; // FUCKING LOOK AT THIS
} inode_t;

typedef struct dir_entry {
	char filename[MAXFILENAME];
	int inode_idx;
} dir_entry_t;


typedef struct fd_entry {
	int inode_number;
	inode_t *inode;
	int rd_write_ptr;
	char status;
} fd_entry_t;

void initializeSuperBlock();
void initializeOpenFileDescriptorTable();
int getFreeDataBlock();
int getFreeFileDescriptor();
void writeInodeToDisk(int inodeNumber, inode_t *inode);
inode_t *getInode(int inodeNumber);
int getInodeNumber(const char *name);
int getFreeInode();
void removeInodeFromRootDirectory(char *filename);
void resetDirectory();
int getBlockPointer(block_pointer_t *pointers, int diskBlockNumber);
void freeDirectPointers(block_pointer_t *pointers);
void freeIndirectPointer(block_pointer_t *pointers);
void freeBlockPointer(block_pointer_t *pointers);
dir_entry_t createNewFile(char *name);

super_block_t superBlock;

fd_entry_t openFileDescriptorTable[MAX_OPEN_FILES]; // this stays in memory

// Overview of what is on disk...The only thing in memory is the open file descriptor table. everything else is read from disk
// block 0: super block
// block 1: inode bitmap
// block 2: free data block bitmap
// block 3 - 17: blocks for inodes
// block 18 - end: data blocks

void mksfs(int fresh) {

	//printf("91: Starting mksfs\n");

	// use a bitmap to keep track of inodes and data blocks
	char bitmap[BLOCK_SIZE];

	// initialize file descriptor statuses so each one is free
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++)
	{
		openFileDescriptorTable[i].status = FREE;
	}

	if (fresh == 1)
	{
		init_fresh_disk(DISK_FILE, BLOCK_SIZE, 18 + 512);

		initializeSuperBlock();
		// write super block to disk
		write_blocks(0, 1, (void *) &superBlock); // block 0 is reserved for super block

		// create bitmap for inodes
		memset(bitmap, FREE, BLOCK_SIZE);
		bitmap[ROOT_DIRECTORY_INODE] = USED; // inode at index 0 is reserved for root directory

		// write bitmap to keep track of data blocks to disk
		write_blocks(1, 1, bitmap);

		memset(bitmap, FREE, BLOCK_SIZE);
		// write bitmap to keep track of inodes to disk
		write_blocks(2, 1, bitmap);

		initializeOpenFileDescriptorTable();

		// write inode for root directory to disk
		writeInodeToDisk(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode_number, openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode);

		// inode blocks range from disk block 3 to disk block 17
		// data blocks start at disk block 18 and onwards
	}
	else
	{
		// init_disk: get disk image and set root directory file descriptor
		init_disk(DISK_FILE, BLOCK_SIZE, 18 + 512);
		openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].status = USED;
		openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode_number = ROOT_DIRECTORY_INODE;
		openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode = getInode(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode_number);
		openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size;
	}
	// printf("139: Ending mksfs\n");
	return;
}

int sfs_getnextfilename(char *fname) {

	static dir_entry_t *rootDirectory;
	int numberOfFiles;
	static int index = 0;

	if (index == 0)
	{
		rootDirectory = malloc(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size);
		openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = 0;
		sfs_fread(ROOT_FILE_DESCRIPTOR, (char *) rootDirectory, openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size);
		numberOfFiles = (openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size) / sizeof(dir_entry_t);
	}

	while (index < numberOfFiles)
	{
		strcpy(fname, rootDirectory[index].filename);
		index++;
		return 1;
	}

	index = 0;
	return 0;
}

int sfs_getfilesize(const char* path) {
	//printf("169: Starting sfs_getfilesize\n");
	int inodeNumber = getInodeNumber(path);
	inode_t *inode;
	if (inodeNumber != -1)
	{
		inode = getInode(inodeNumber);
		//printf("175: Ending sfs_getfilesize found filesize %d\n", inode->size);
		return inode->size;
	}
	//printf("178: Ending sfs_getfilesize\n");
	return -1;
}

int sfs_fopen(char *name) {

	//printf("184: Starting sfs_fopen\n");

	if (strlen(name) > MAX_FILENAME) // exceeds max characters for filename so reject
	{
		//printf("188: Length is longer than allowed filename\n");
		return -1;
	}

	int inodeNumber = getInodeNumber(name);
	//printf("193: Inode Number: %d for file %s\n", inodeNumber, name);
	int i;
	// start at index 1 because index 0 is reserved for root directory, which is always open
	for (i = 1; i < MAX_OPEN_FILES; i++) // check what's already open
	{
		// file is already open so don't do anything. just return file descriptor index
		if ((openFileDescriptorTable[i].status == USED) && (openFileDescriptorTable[i].inode_number == inodeNumber))
		{
			//printf("201: Ending sfs_fopen because found open fd already %d\n", i);
			return i;
		}
	}

	// check to see if there's a free file descriptor to use or have we exceeded max amount of open files at one time
	int freeFileDescriptor = getFreeFileDescriptor();
	if (freeFileDescriptor == -1)
	{
		//printf("210: Ending sfs_fopen because no fd available\n");
		return -1; // no free fd so get out and return
	}

	inode_t *inode;
	dir_entry_t newFile;

	if (inodeNumber != -1) // file already exists // FUCKING STARTS HERE
	{
		inode = getInode(inodeNumber);
		// update open file descriptor table entry for file specified
		openFileDescriptorTable[freeFileDescriptor].inode_number = inodeNumber;
		openFileDescriptorTable[freeFileDescriptor].rd_write_ptr = inode->size;
		openFileDescriptorTable[freeFileDescriptor].inode = inode;
		//printf("224: Ending sfs_fopen with file descriptor %d, inode number %d\n", freeFileDescriptor, inodeNumber);
	}
	else // must create the file since it doesn't exist
	{
		// allocate i-node
		inode = calloc(1, sizeof(inode_t));

		// create new file
		newFile = createNewFile(name);

		// go to end of root directory to write file
		openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size;
		sfs_fwrite(ROOT_FILE_DESCRIPTOR, (char *) &newFile, sizeof(dir_entry_t));
		writeInodeToDisk(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode_number, openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode);

		// update open file descriptor table entry for file specified
		openFileDescriptorTable[freeFileDescriptor].inode_number = newFile.inode_idx;
		openFileDescriptorTable[freeFileDescriptor].rd_write_ptr = inode->size;
		openFileDescriptorTable[freeFileDescriptor].inode = inode;
	}
	return freeFileDescriptor;
}

int sfs_fclose(int fileID){

	//printf("249: Starting sfs_fclose\n");
	// check if file descriptor is closed to begin with
	if (openFileDescriptorTable[fileID].status == FREE)
	{
		//printf("253: Ending sfs_fclose because fd is closed\n");
		return -1;
	}
	// free up file descriptor to be used for something else
	openFileDescriptorTable[fileID].status = FREE;
	// write inode to disk to "save" the data
	writeInodeToDisk(openFileDescriptorTable[fileID].inode_number, openFileDescriptorTable[fileID].inode);

	free(openFileDescriptorTable[fileID].inode); // free up this memory
	//printf("262: Ending sfs_close after freeing inode at index %d\n", fileID);
	return 0;
}

int sfs_fread(int fileID, char *buf, int length){
	//printf("267: Starting sfs_fread with fd %d and length %d\n", fileID, length);
	if (openFileDescriptorTable[fileID].status == FREE) // check to see if file is open
	{
		//printf("270: Ending sfs_fread because file is already open with file descriptor %d\n", fileID);
		return -1;
	}

	inode_t *inode = openFileDescriptorTable[fileID].inode;

	int readWritePointer = openFileDescriptorTable[fileID].rd_write_ptr;
	int diskBlockNumber = readWritePointer / BLOCK_SIZE; // index within inode of disk block
	int amountWritten = readWritePointer % BLOCK_SIZE; // how many blocks you have written to already

	int blockPointer;

	char block[BLOCK_SIZE];

	// check if amount read will go more than one block
	if (openFileDescriptorTable[fileID].rd_write_ptr + length > inode->size)
	{
		length = inode->size - readWritePointer;
	}

	// if data read in does not span more than one block
	if (readWritePointer + length <= BLOCK_SIZE)
	{
		blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);////////
		read_blocks(blockPointer, 1, block);
		memcpy(buf, &block[amountWritten], length);
		openFileDescriptorTable[fileID].rd_write_ptr += length;
		//printf("297: Ending sfs_fread...read in one block of data\n");
		return length;
	}

	// amount read spans more than 1 block

	int finalBlock = ((amountWritten + length) / BLOCK_SIZE) + diskBlockNumber; // get upper bound block

	blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);

	read_blocks(blockPointer, 1, block);
	memcpy(buf, &block[amountWritten], BLOCK_SIZE - amountWritten);
	buf = buf + (BLOCK_SIZE - amountWritten); // fill the space that's left in the block that has already been written to
	diskBlockNumber++; // read from next data block for remaining data

	int extraBytes = (amountWritten + length) % BLOCK_SIZE; // remaining data to read in the last data block

	while (diskBlockNumber < finalBlock) // here the data blocks are full and read until you reach last partially filled block
	{
		blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);
		read_blocks(blockPointer, 1, block);
		memcpy(buf, block, BLOCK_SIZE);
		buf += BLOCK_SIZE;
		diskBlockNumber++;
	}
	// data here partially fills the last block in the block range
	blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);
	read_blocks(blockPointer, 1, block);
	memcpy(buf, block, extraBytes);

	openFileDescriptorTable[fileID].rd_write_ptr += length; // update pointer for file location
	//printf("328: Ending sfs_fread...read in multiple blocks\n");
	return length;
}

int sfs_fwrite(int fileID, const char *buf, int length){
	//printf("333: Starting sfs_fwrite\n");
	// check to see that the file is actually open and that the length we want to write will fit to not exceed max file size
	if ((openFileDescriptorTable[fileID].status == FREE) || (openFileDescriptorTable[fileID].rd_write_ptr + length > MAX_FILE_SIZE))
	{
		//printf("337: Ending sfs_fwrite because file is already open or length exceeds max filesize\n");
		return -1;
	}

	inode_t *inode = openFileDescriptorTable[fileID].inode;

	int readWritePointer = openFileDescriptorTable[fileID].rd_write_ptr;
	int diskBlockNumber = readWritePointer / BLOCK_SIZE; // index within inode of disk block
	int amountWritten = readWritePointer % BLOCK_SIZE; // how many blocks you have written to already

	int blockPointer;

	char block[BLOCK_SIZE];

	// check if the data to be written will fit in one block
	if (amountWritten + length <= BLOCK_SIZE)
	{
		// find a block pointer to use to store data
		blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);
		//printf("356: blockPointer %d\n", blockPointer);
		if (blockPointer != -1) // free data block to write data
		{
			read_blocks(blockPointer, 1, block);
			memcpy(&block[amountWritten], buf, length); // start appending to data that has already been written there
			write_blocks(blockPointer, 1, block);
			openFileDescriptorTable[fileID].rd_write_ptr = readWritePointer + length;
			if (readWritePointer + length > inode->size)
			{
				inode->size = readWritePointer + length;
			}
			//printf("367: Ending sfs_write by writing to disk where data fit in 1 block\n");
			writeInodeToDisk(openFileDescriptorTable[fileID].inode_number, openFileDescriptorTable[fileID].inode);
			return length;
		}
		//printf("371: Ending sfs_write...cannot write because no free data blocks to use\n");
		return -1; // cannot write because there are no free data blocks to use
	}

	// the data that needs to be written from here will take up more than 1 block

	int finalBlock = ((amountWritten + length) / BLOCK_SIZE) + diskBlockNumber; // get the upper bound block

	blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);
	if (blockPointer == -1) // no free datablocks to use
	{
		//printf("382: Ending sfs_write since no free datablocks to use\n");
		return -1;
	}
	// fill first block until the end then move onto the next block
	read_blocks(blockPointer, 1, block);
	memcpy(&block[amountWritten], buf, BLOCK_SIZE - amountWritten);
	write_blocks(blockPointer, 1, block);

	buf = buf + (BLOCK_SIZE - amountWritten); // move pointer the amount you have appended
	diskBlockNumber++; // increment to the next block you need to write to

	// all blocks in here you can fill them up fully
	while (diskBlockNumber < finalBlock) // loop until you reach the one before last full block
	{
		blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);
		if (blockPointer == -1)
		{
			//printf("399: Ending sfs_write since no free datablocks to use\n");
			return -1;
		}
		memcpy(block, buf, BLOCK_SIZE);
		write_blocks(blockPointer, 1, block);
		buf += BLOCK_SIZE;
		diskBlockNumber++;
	}
	// write the remaining data into a final block that will fill it partially
	int extraBytes = (amountWritten + length) % BLOCK_SIZE;

	blockPointer = getBlockPointer(&(inode->block_pointers), diskBlockNumber);
	if (blockPointer == -1)
	{
		//printf("413: Ending sfs_write since no free datablocks to use\n");
		return -1; // No more free blocks to use
	}
	read_blocks(blockPointer, 1, block);
	memcpy(block, buf, extraBytes);
	write_blocks(blockPointer, 1, block);

	openFileDescriptorTable[fileID].rd_write_ptr = readWritePointer + length;
	if (readWritePointer + length > inode->size)
	{
		inode->size = readWritePointer + length;
	}
	//printf("425: Ending sfs_fwrite by writing to disk\n");
	writeInodeToDisk(openFileDescriptorTable[fileID].inode_number, openFileDescriptorTable[fileID].inode);
	return length;
}

int sfs_fseek(int fileID, int loc){
	//printf("431: Starting sfs_fseek\n");
	// check if file is open. if it is not, return and get out
	if (openFileDescriptorTable[fileID].status == FREE)
	{
		//printf("435: Ending sfs_fseek because file is open already\n");
		return -1;
	}

	if (loc > openFileDescriptorTable[fileID].inode->size) // offset is larger than file size
	{
		loc = openFileDescriptorTable[fileID].inode->size;
	}
	openFileDescriptorTable[fileID].rd_write_ptr = loc;
	//printf("444: Ending sfs_fseek with new location %d\n", loc);
	return 0;
}

int sfs_remove(char *file) {
	//printf("449: Starting sfs_remove removing file %s\n", file);
	int inodeNumber = getInodeNumber(file);
	if (inodeNumber == -1) // no such file
	{
		return -1;
	}
	char bitmap[BLOCK_SIZE];

	inode_t *inode = getInode(inodeNumber);

	// free data blocks...also frees position in free data block bitmap
	freeBlockPointer(&(inode->block_pointers));

	// free bitmaps for inode
	read_blocks(1, 1, bitmap);
	bitmap[inodeNumber] = FREE;
	write_blocks(1, 1, bitmap);

	removeInodeFromRootDirectory(file);

	free(inode);

	//printf("471: Ending sfs_remove\n");
	return 0;
}

void initializeSuperBlock()
{
	superBlock.magic = 0xAABB0005;
	superBlock.block_size = BLOCK_SIZE;
	superBlock.fs_size = MAX_BLOCKS * BLOCK_SIZE;
	superBlock.inode_table_len = MAX_INODES;
	superBlock.root_dir_inode = ROOT_DIRECTORY_INODE;
}

void initializeOpenFileDescriptorTable()
{
	// create entry in open file descriptor table for ROOT DIRECTORY
	// ***** file descriptor 0 is reserver for ROOT DIRECTORY ******
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode = calloc(1, sizeof(inode_t)); // create root directory inode
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size = 0; // no contents in root directory yet
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->link_cnt = 1;
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->uid = 0;
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->gid = 0;
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->mode = 0x755;
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode_number = ROOT_DIRECTORY_INODE;

	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = 0;
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].status = USED;
}

int getFreeDataBlock()
{
	//printf("502: Starting getFreeDataBlock\n");
	char bitmap[BLOCK_SIZE];
	read_blocks(2, 1, bitmap); // data block bitmap starts at disk block index 2 on disk

	int i;
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		if (bitmap[i] == FREE) // there is a free data block to write to
		{
			bitmap[i] = USED;
			write_blocks(2, 1, bitmap); // update free block bitmap
			//printf("513: Ending getFreeDataBlock...free data block is %d\n", i+18);
			return i + 18; // returns index of free data block. add 18 to it because 18 is start of data blocks
		}
	}
	//printf("517: Ending getFreeDataBlock...no free data block available\n");
	return -1; // no free data blocks left

}

int getFreeFileDescriptor()
{
	//printf("524: Starting getFreeFileDescriptor\n");
	int i;
	// index 0 of open file descriptor table is reserved for ROOT DIRECTORY so start at inex 1
	for (i = 1; i < MAX_OPEN_FILES; i++)
	{
		if (openFileDescriptorTable[i].status == FREE)
		{
			openFileDescriptorTable[i].status = USED;
			//printf("532: Ending getFreeFileDescriptor...free fd is %d\n", i);
			return i;
		}
	}
	//printf("536: Ending getFreeFileDescriptor...no free fd\n");
	return -1;
}

void writeInodeToDisk(int inodeNumber, inode_t *inode)
{
	//printf("542: Starting writeInodeToDisk\n");
	inode_t *inodeTable;
	char buffer[BLOCK_SIZE];

	// Figuring out where the inode should go and which block number inode will belong to
	int numberOfInodesPerBlock = 512/sizeof(inode_t); // comes out to be 7 inodes per block on disk
	int blockNumber = inodeNumber/numberOfInodesPerBlock;
	int inodeIndex = inodeNumber % numberOfInodesPerBlock;

	// inodes range from block 3 to block 17
	read_blocks(3 + blockNumber, 1, buffer);

	inodeTable = (inode_t *) buffer;

	// add inode to inode table
	//printf("557: inodeNumber %d\n", inodeNumber);
	memcpy(&inodeTable[inodeIndex], inode, sizeof(inode_t));

	// write inode data block to disk
	write_blocks(3 + blockNumber, 1, buffer);
	//printf("562: Ending writeInodeToDisk\n");
}

inode_t * getInode(int inodeNumber)
{
	//printf("567: Starting getInode\n");
	inode_t *inodeTable;
	inode_t *inode;
	char buffer[BLOCK_SIZE];

	// Figuring out where the inode should go and which block number inode will belong to
	int numberOfInodesPerBlock = 512/sizeof(inode_t); // comes out to be 7 inodes per block on disk
	int blockNumber = inodeNumber/numberOfInodesPerBlock;
	int inodeIndex = inodeNumber % numberOfInodesPerBlock;

	// read data blocks from disk. inodes start at block index 3
	read_blocks(3 + blockNumber, 1, buffer);

	inodeTable = (inode_t *) buffer;

	inode = malloc(sizeof(inode_t));

	// copy inode at index into inode variable
	memcpy(inode, &inodeTable[inodeIndex], sizeof(inode_t));
	//printf("586: Ending getInode\n");
	return inode;
}

int getInodeNumber(const char *name)
{
	//printf("592: Starting getInodeNumber\n");
	int directorySize = openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size; // get root directory size
	void *buffer = (void *) malloc(directorySize);
	dir_entry_t *files = (dir_entry_t *) buffer;

	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = 0;

	sfs_fread(ROOT_FILE_DESCRIPTOR, buffer, directorySize); // read in the directory contents and loop through it

	int numberOfFiles = directorySize/sizeof(files);
	int i;
	int inodeNumber;
	for (i = 0; i < numberOfFiles; i++) // loop through all files found from reading disk
	{
		if (strcmp(files->filename, name) == 0) // found the match
		{
			inodeNumber = files->inode_idx;
			//printf("609: Ending getInodeNumber...inode number is %d for file %s\n", inodeNumber, name);
			return inodeNumber;
		}
		files++; // move onto next file
	}
	free(buffer);
	//printf("615: Ending getInodeNumber...no inode number can be found\n");
	return -1;
}

int getFreeInode()
{
	//printf("621: Starting getFreeInode\n");
	char block[BLOCK_SIZE];
	read_blocks(1, 1, block); // read in inode bit map to find which one is free
	int i;
	for (i = 0; i < MAX_INODES; i++)
	{
		if (block[i] == FREE)
		{
			block[i] = USED;
			write_blocks(1, 1, block);
			//printf("631: Ending getFreeInode...free inode is %d\n", i);
			return i; // inode blocks start at index 3 to index 17
		}
	}
	//printf("635: Ending getFreeInode...no free inode available\n");
	return -1;
}

void removeInodeFromRootDirectory(char *filename)
{
	//printf("641: Starting removeInodeFromRootDiretory\n");
	int directorySize = openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size;
	void *buffer = (void *) malloc(directorySize);
	dir_entry_t *file = (dir_entry_t *) buffer;
	int numberOfFiles = directorySize/sizeof(dir_entry_t);

	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = 0;
	sfs_fread(ROOT_FILE_DESCRIPTOR, buffer, directorySize); // read in bytes of root directory

	int i = 0;
	int remainingFiles;
	while (i < numberOfFiles) // loop through each file
	{
		remainingFiles = numberOfFiles-i-1;
		if (strcmp(file->filename, filename) == 0) // found file to remove
		{
			// use memmove to overwrite file that will be deleted
			// moving memory one position right of file that will be deleted and overwrite file that will be deleted
			memmove(file, &file[1], sizeof(dir_entry_t) * remainingFiles);
			resetDirectory();
			sfs_fwrite(ROOT_FILE_DESCRIPTOR, buffer, directorySize - sizeof(dir_entry_t));
			writeInodeToDisk(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode_number, openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode);
			free(buffer);
			//printf("664: Ending removeInodeFromRootDirectory...removed Inode\n");
			return;
		}
		i++;
		file++; // move pointer to next file in the directory
	}
	printf("659: Ending removeInodeFromRootDirectory...no filename found\n");
}

void resetDirectory()
{
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].rd_write_ptr = 0;
	openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->size = 0;
	freeBlockPointer(&(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode->block_pointers));
	memset(openFileDescriptorTable[ROOT_FILE_DESCRIPTOR].inode, 0, sizeof(inode_t));

}

// returns data block location on disk for a block pointer inside an inode
int getBlockPointer(block_pointer_t *pointers, int diskBlockNumber)
{
	//printf("685: Starting getBlockPointer, diskBlockNumber %d\n", diskBlockNumber);
	// indirect pointer will be pointing to this block
	int indirectPointers[BLOCK_SIZE/sizeof(int)];

	// check if block number given is for a direct pointer
	if (diskBlockNumber < DIRECT_POINTERS)
	{
		//printf("692: pointers->directPointers %d\n", pointers->directPointers[diskBlockNumber]);
		if (pointers->directPointers[diskBlockNumber] == 0)
		{
			pointers->directPointers[diskBlockNumber] = getFreeDataBlock();
			if (pointers->directPointers[diskBlockNumber] == -1)
			{
				return -1;
			}
		}
		//printf("701: Ending getBlockPointer...block pointer is %d\n", pointers->directPointers[diskBlockNumber]);
		return pointers->directPointers[diskBlockNumber];
	}

	// first time trying to access indirect pointer. file grew big enough and needs more than 12 direct data blocks
	if (pointers->indirectPointer == 0)
	{
		pointers->indirectPointer = getFreeDataBlock();
		if (pointers->indirectPointer != -1)
		{
			memset(indirectPointers, 0, BLOCK_SIZE); // set all pointer locations to 0
			write_blocks(pointers->indirectPointer, 1, (void *) indirectPointers); // write block of pointers here where there is a free block
		}
		else // no more free blocks
		{
			return -1;
		}
	}
	// go to location where indirect pointer points to and get all those other pointers to data blocks
	read_blocks(pointers->indirectPointer, 1, (void *) indirectPointers);
	if (indirectPointers[diskBlockNumber - DIRECT_POINTERS] == 0)
	{
		indirectPointers[diskBlockNumber - DIRECT_POINTERS] = getFreeDataBlock();
		if (indirectPointers[diskBlockNumber - DIRECT_POINTERS] != -1)
		{
			write_blocks(pointers->indirectPointer, 1, (void *) indirectPointers); // write back onto disk the list of indirect pointers
		}
		else // no more free blocks
		{
			return -1;
		}
	}
	//printf("733: Ending getBlockPointer...entered into indirect block %d\n", indirectPointers[diskBlockNumber-DIRECT_POINTERS]);
	return indirectPointers[diskBlockNumber - DIRECT_POINTERS];
}

void freeDirectPointers(block_pointer_t *pointers)
{
	//printf("739: Starting freeDirectPointers\n");
	char bitmap[BLOCK_SIZE];

	read_blocks(2, 1, bitmap); // get free data block bit map

	int blockIndex;

	int i;
	for (i = 0; i <= DIRECT_POINTERS; i++)
	{
		blockIndex = pointers->directPointers[i]; // returns location of data block on disk
		if (blockIndex == 0) // no more data can exist after this iteration
		{
			write_blocks(2, 1, bitmap); // write back to disk the updated bitmap of free data blocks
			return;
		}
		blockIndex = blockIndex - 18; // represents location of data block on disk
		bitmap[blockIndex] = FREE;
	}
	write_blocks(2, 1, bitmap); // write updated bitmap back to disk
	//printf("759: Ending freeBlockPointers\n");
}

void freeIndirectPointer(block_pointer_t *pointers)
{
	//printf("764: Starting freeIndirectPointer\n");
	int indirectPointers[BLOCK_SIZE/sizeof(int)];
	char bitmap[BLOCK_SIZE];
	int numberOfPointers = BLOCK_SIZE/sizeof(int);

	read_blocks(2, 1, bitmap); // get free data block bit map

	read_blocks(pointers->indirectPointer, 1, (void *) indirectPointers); // get array of pointers pointed to by indirect pointer

	int blockIndex;

	int i;
	for (i = 0; i < numberOfPointers; i++)
	{
		blockIndex = indirectPointers[i];
		if (blockIndex == 0)
		{
			write_blocks(2, 1, bitmap); // write back to disk the updated bitmap of free data blocks
			return;
		}
		blockIndex = blockIndex - 18; // represents location of data block on disk
		bitmap[blockIndex] = FREE;
	}
	//printf("787: Ending freeIndirectPointer\n");
}

void freeBlockPointer(block_pointer_t *pointers)
{
	freeDirectPointers(pointers);
	// only free indirect pointers if there is actually one that exists and can only start at block number 18 where the data blocks start
	if (pointers->indirectPointer >= 18)
	{
		freeIndirectPointer(pointers);
	}
}

dir_entry_t createNewFile(char *name)
{
	//printf("802: Starting createNewFile for file name: %s\n", name);
	int inodeNumber = getFreeInode();

	dir_entry_t newFile;
	newFile.inode_idx = inodeNumber;
	strcpy(newFile.filename, name);
	//printf("808: Ending createNewFile. Create new file with inode number %d and file name %s\n", newFile.inode_idx, newFile.filename);
	return newFile;
}
