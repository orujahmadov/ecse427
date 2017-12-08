#include "disk_emu.h"

// size of components
#define blockSize 1024
#define sizeOfPointer 4
// might modified numberOf blocks 
#define numberOfBlocks 1024
#define numberOfInodes 200
#define sizeOfInode 64
#define sizeOfSuperBlockField 4

#define numberOfEntries 64
 
#define myFileName "WDDNguyen"

// non standard inode 
// size field  total number of bytes 
// no need to know about indirect 
// these block contains data

//File has lots of inodes 
// for j node   size = size of the file 
// set size = -1 to be blank


typedef struct {
	int size; 
	int direct[14];
	int indirect;
} inode_t;


typedef struct {
	inode_t inodeSlot[16];
} inodeBlock_t;

// root is a jnode
// have to add shadow jnode later
// need to fill to get to 1024 or  copy memory to block_t then pass that to the disk * calloc

typedef struct {
	
unsigned char magic[4];

int block_size;
int fs_size;
int Inodes;
inode_t root;
inode_t shadow[4];
int lastShadow;
int rootDirectoryBlockNumber[4]; 
//filling up the super block with empty value
char fill[668];
} superblock_t;


// can use uint8_t 
typedef struct{
	unsigned char bytes[blockSize];
}block_t;

typedef struct {
    int free;
    int inode;
    int rwptr;
	int readptr;
} fileDescriptor_t;
 
typedef struct {
    char name[10];
    int inodeIndex;
} directoryEntry_t;

typedef struct{
	directoryEntry_t entries[64];
} rootDirectory_t;

void mkssfs(int fresh);
int ssfs_fopen(char *name);
int ssfs_fclose(int fileID);
int ssfs_frseek(int fileID, int loc);
int ssfs_fwseek(int fileID, int loc);
int ssfs_fwrite(int fileID, char *buf, int length);
int ssfs_fread(int fileID, char *buf, int length);
int ssfs_remove(char *file);
/*int ssfs_commit();
int ssfs_restore(int cnum);
*/



