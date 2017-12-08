// Written by William Nguyen 260638465

#include "sfs_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "disk_emu.h"

#include <sys/types.h>
#include <fcntl.h>

superblock_t sb;
block_t fbm;

fileDescriptor_t fdt[numberOfInodes];
inodeBlock_t inodeBlocks[13];
rootDirectory_t rootDirectory[4];
block_t writeBlock; 

/*
initialize the inode file to have all free inode size set to -1 and direct and indirect to -1
Set first inode to be the root Directory with data block 2,3,4,5.
*/
void initializeInodeFiles(){
	int i,k;
	
	inode_t tempInode;
	tempInode.size = -1;
	for (i =0 ; i < 14 ; i++){
		tempInode.direct[i] = -1;
	}
	tempInode.indirect = -1;
	
	// initialize all inode to be unused 
	for (i = 0; i < 13 ; i++){
	for (k = 0 ; k < 16; k++){
		inodeBlocks[i].inodeSlot[k]=tempInode;
		}
	}
	
	// first inode contains all 4 root directory block number 
	tempInode.size = 0;
	tempInode.direct[0] = sb.rootDirectoryBlockNumber[0];
	tempInode.direct[1] = sb.rootDirectoryBlockNumber[1];
	tempInode.direct[2] = sb.rootDirectoryBlockNumber[2];
	tempInode.direct[3] = sb.rootDirectoryBlockNumber[3];
	inodeBlocks[0].inodeSlot[0] = tempInode;
	
}

/*
Initialize all 4 data blocks in root directory by creating an empty entry and setting each slot
with this entry. 
*/

void initializeRootDirectory(){
	int i,k;	
	
	directoryEntry_t entry;
	entry.inodeIndex = -1;
	strcpy(entry.name,"root/");

	// initialize all entries of root directories with the empty entry
	for (k = 0 ; k < 4 ; k++){
		for (i = 0 ; i < numberOfEntries ; i++){
			rootDirectory[k].entries[i] = entry;
		}
	}
}

/*
Initialize all superblock members 
*/
void initializeSuperBlock(){
	
	int i;
	inode_t root;
	root.size = 13312;
	
	// setting the block numbers to check for the root inode 
	for( i = 0 ; i < 13 ; i++){
		root.direct[i] = 6 + i;
	}
	
	sb.magic[0] = 0xAC;
	sb.magic[1] = 0xBD;
	sb.magic[2] = 0x00;
	sb.magic[3] = 0x05;
	sb.block_size = blockSize;
	sb.fs_size = blockSize * numberOfBlocks; 
	sb.Inodes = numberOfInodes;
	sb.root = root;
	sb.rootDirectoryBlockNumber[0] = 2;
	sb.rootDirectoryBlockNumber[1] = 3;
	sb.rootDirectoryBlockNumber[2] = 4;
	sb.rootDirectoryBlockNumber[3] = 5;
}


/* 
initialize file directory and set all values to free, rwptr to 0 and  no inode values. 
*/
void initializeFileDescriptorTable() {
	int i;
	fileDescriptor_t fd;
	fd.free = -1;
	fd.rwptr = 0;
	fd.inode = -1;
	fd.readptr = 0;
	for (i = 0; i < numberOfInodes; i++){
		fdt[i] = fd; 
	}

}

/*
Find a data block that is free using the FBM 
*/
int FBMGetFreeBit(){
	int i,k;
	
	for (i = 0; i < blockSize; i++){
		for (k = 0; k < 8 ; k++){
			if ((fbm.bytes[i] & (1 << k)) == (1 << k)){			
				// set the bit to 0 
				fbm.bytes[i] = fbm.bytes[i] ^ (1 << k);
				return (i * 8 + k); 
			}
				
		}
	}
	
	return -1; 
}

/*
Initialize Free bit map by putting all data blocks to 1.
Set the first 18 bits to be used for the super block,root directory, inode files and FBM
*/
void initializeFBM(){
	int i;
	for (i = 0 ; i < blockSize; i++){
		fbm.bytes[i] = 0xFF;
	}
	
	// first 18 blocks are used when initializing 
	for( i = 0 ; i < 18; i++){
	FBMGetFreeBit();
	}
	
}

/* 
set the FBM bit to be the opposite of the current bit for the specific block number to indicate if block number is free.
*/
int setFBMbit(int blockNumber){
	int byte = blockNumber / 8;
	int bit = blockNumber % 8;
	fbm.bytes[byte] = fbm.bytes[byte] ^ (1 << bit); 
	
	return 0;
}

/*
Goes through each direct value of j-node then check the Inode files to look for a free i-node by checking if i-node size is -1.
return : the inode index between 0 - 199
*/
int findFreeInodeIndex(){
	int i,k;
	int blockNumber;
	int inodeIndex = -5;
	
	//check for all the i-nodes files
	for (i = 0; i < 13; i++){
		blockNumber = sb.root.direct[i];
			
		for(k = 0; k < 16; k++){
			if (inodeBlocks[i].inodeSlot[k].size == -1){
				inodeIndex = i * 16 + k ;
				return inodeIndex;
			}	
		}
	}
		
	return inodeIndex;
	
}

/*
Add new inode into the free slot of the i-node File
inodeIndex : inode index to place the new inode in the i-node File
*/
int rootAddInode(int inodeIndex){
	int i = inodeIndex / 13;
	int k;
	inode_t newInode;
	
	for (k = 0 ; k < 14; k++){
		newInode.direct[k] = -1;
	}
	
	newInode.size = 0;
	
	for(k = 0 ; i<16; k++){
		if (inodeBlocks[i].inodeSlot[k].size == -1){
			inodeBlocks[i].inodeSlot[k] = newInode;
			write_blocks(sb.root.direct[i], 1, &inodeBlocks[i]);
			return 1;
		} 
	}
return -1;	
}

/*create a new file by creating a new entry and adding new entry to an available location in the root directory 
fname : name of the file to be created 
*/
int createFile(char* fname){
	int i,k;
	int freeIndex = findFreeInodeIndex();
	
	directoryEntry_t entry;
	stpcpy(entry.name,fname);
	entry.inodeIndex = freeIndex;
	
	// check if file exist in the root directories 
	for (k = 0 ; k < 4; k++){
		for(i = 0 ; i < numberOfEntries ; i++){
			
			if ( strcmp(rootDirectory[k].entries[i].name, fname) == 0){
				return -1;
			}
		}
	}
	// if doesn't exist, set new entry with name and inode associated to the file
	for (k = 0 ; k < 4 ; k++){
		for (i = 0; i < numberOfEntries; i++){
			if (rootDirectory[k].entries[i].inodeIndex == -1){
				rootDirectory[k].entries[i] = entry;
				write_blocks(sb.rootDirectoryBlockNumber[k],1, &rootDirectory[k]);
				break;
			}
		}
	}
	
	rootAddInode(freeIndex);
	
	return 0;
}
/*
Allocate a new data block when writing in a file if not enough available bytes to write.
inodeIndex : allocate new block to the specific i-node and set that block in the i-node direct
*/
int allocateDataBlock(int inodeIndex){
	int i = inodeIndex / 13;
	int slot = inodeIndex % 13;
	int newBlockNumber = FBMGetFreeBit();
	int k,m;
	
	for (k = 0; k < 14 ; k++){
		if(inodeBlocks[i].inodeSlot[slot].direct[k] == -1){
			inodeBlocks[i].inodeSlot[slot].direct[k] = newBlockNumber;
			write_blocks(sb.root.direct[i],1, &inodeBlocks[i]);
			return newBlockNumber;
		}
	}
	
	// maximum of 74 extra inodes for a single file 
	for (m = 0; m < 74; m++){
		// if all the direct data blocks are already allocated then need to check the indirect blocks
		if (inodeBlocks[i].inodeSlot[slot].indirect == -1){
			// allocate new i-node index to the indirect
			// get new free data block by checking the FBM 
			inodeBlocks[i].inodeSlot[slot].indirect = FBMGetFreeBit();
			// create new i-node for the file 
			rootAddInode(inodeBlocks[i].inodeSlot[slot].indirect);
			// need to create a new inode block that stores this data	
		}
		// goes to next indirect i-node 
		else {
			i = inodeBlocks[i].inodeSlot[slot].indirect / 13;
			slot = inodeBlocks[i].inodeSlot[slot].indirect % 13;
		}
	}
	
	return -1;
	
}
/*
make a shadow file system
fresh : if fresh > 0 then initialize the disk else recover persistance values in the disk 
*/
void mkssfs(int fresh){
	int i;
	initializeFileDescriptorTable();
		
	if (fresh){
	
		initializeFBM();
		initializeSuperBlock();
		initializeInodeFiles();
		initializeRootDirectory();

		//create a new file system
		char* filename = "WDDNGUYEN";
		
		init_fresh_disk(filename, blockSize, numberOfBlocks);
		
		initializeFileDescriptorTable();
		
		write_blocks(0,1, &sb);
		write_blocks(1,1, &fbm);		
		write_blocks(sb.rootDirectoryBlockNumber[0],1, &rootDirectory[0]);
		write_blocks(sb.rootDirectoryBlockNumber[1],1, &rootDirectory[1]);
		write_blocks(sb.rootDirectoryBlockNumber[2],1, &rootDirectory[2]);
		write_blocks(sb.rootDirectoryBlockNumber[3],1, &rootDirectory[3]);
		for (i = 0; i < 13; i++){
			write_blocks(6 + i, 1, &inodeBlocks[i]);
		}
		
	}
	// Shadow file system already exist 
	else {
	
	char* filename = "WDDNGUYEN";
	initializeFileDescriptorTable();
	init_disk(filename, blockSize, numberOfBlocks);
	
	// open super block 
	read_blocks(0,1,&sb);
	// open FBM 
	read_blocks(1,1,&fbm);
	// open root directory
	read_blocks(sb.rootDirectoryBlockNumber[0],1,&rootDirectory[0]);
	read_blocks(sb.rootDirectoryBlockNumber[1],1,&rootDirectory[1]);
	read_blocks(sb.rootDirectoryBlockNumber[2],1,&rootDirectory[2]);
	read_blocks(sb.rootDirectoryBlockNumber[3],1,&rootDirectory[3]);
	//open all inode file to cache 
	for(i = 0; i < 13; i++){
		read_blocks(sb.root.direct[i], 1 , &inodeBlocks[i]);
	}
	}
 
}

/*
find the entry in the root directory
name : search for the entry with the file name 
return : the inode index of the entry 
*/
int findEntry(char *name){
	int i,k;
	
	// check if file is already in root directory then add to open file descriptor
	for (k = 0 ; k < 4; k ++){
		for (i = 0; i < numberOfEntries ; i++){
			if (strcmp(rootDirectory[k].entries[i].name, name) == 0){
				return rootDirectory[k].entries[i].inodeIndex;
			}		
		}
	}
	return -1;
}
/*
Open a file by checking if file exist in the root directory and place the file in a file descriptor table when writing/reading
if file doesn't exist, create a new file, add into the root directory then place the file in the file descriptor table. 
name : name of the file to open .
return : file descriptor index
*/
int ssfs_fopen(char *name){
	int i;
	int inodeIndex = -1;
	
	//file name too big 
	if (strlen(name) > 10){
		return -1;
	}
	
	// search root directory for file name
	inodeIndex = findEntry(name);
	// if doesnt exist then create a new file 
	if(inodeIndex == -1){
		createFile(name);
		inodeIndex = findEntry(name);
		if (inodeIndex == -1){
			printf("too many files in root directory");
			return -1;
		}
	}
	
	//check if fdt already has a file open
	for(i = 0; i < numberOfInodes;i++){
		if(fdt[i].inode == inodeIndex){
			// need to adjust write pointer to last written file  when open    
			
			// get size of inode 
			int block = fdt[i].inode/ 13; 
			int slotIndex = fdt[i].inode % 13;
			int size = inodeBlocks[block].inodeSlot[slotIndex].size;
			
			fdt[i].rwptr = size;
			fdt[i].readptr = 0;
			return i;
		}
	}
	
	//check for free space in fdt and add inode index to the table
	for(i = 0; i < numberOfInodes; i++){
		if(fdt[i].free == -1){
			fdt[i].inode = inodeIndex;
			fdt[i].rwptr = 0;
			fdt[i].free = 0;
			fdt[i].readptr = 0;
			return i;
		}
	}
	
	return -1;
	
}
/*
Close the file descriptor table
fileID : the index of the file descriptor table
*/

int ssfs_fclose(int fileID){

	//invalid fileID
	if(fileID < 0 || fileID >= numberOfInodes){
		//printf("invalid fileID\n");
		return -1;
	}
	
	//if file ID is not open 
	if (fdt[fileID].free == -1){
		//printf("fdt location is free\n");
		return -1;
	}
	// remove fdt open file
	fdt[fileID].inode = -1;
	fdt[fileID].rwptr = 0;
	fdt[fileID].readptr = 0;
	fdt[fileID].free = -1;
	return 0;	
	
}

/*
seek the read pointer of the file descriptor table to the specific byte location
fileID : file descriptor table index
loc : byte location for read pointer to be placed.
*/
int ssfs_frseek(int fileID, int loc){
	// check if fileID is valid
	
	if (loc < 0){
		return -1;
	}
	
	if(fileID < 0 || fileID >= numberOfInodes){
		return -1;
	}
	
	
	if (fdt[fileID].free == -1){
		return -1;
	}
	
	int block = fdt[fileID].inode / 13;
	int slot = fdt[fileID].inode % 13;
	int size = inodeBlocks[block].inodeSlot[slot].size;
	
	// location can't be bigger than size 
	if(loc > size){
		return -1;
	}
	
	fdt[fileID].readptr = loc;
	return 0;
	
}

/*
seek the write pointer of the file descriptor table to the specific byte location
fileID : file descriptor table index
loc : byte location for write pointer to be placed.
*/

int ssfs_fwseek(int fileID, int loc){
	
	// check if fileID is valid
	
	if (loc < 0){
		return -1;
	}
	
	if(fileID < 0 || fileID >= numberOfInodes){
		return -1;
	}
	
	
	if (fdt[fileID].free == -1){
		return -1;
	}
	
	int block = fdt[fileID].inode / 13;
	int slot = fdt[fileID].inode % 13;
	int size = inodeBlocks[block].inodeSlot[slot].size;
	if(loc > size){
		return -1;
	}
	
	fdt[fileID].rwptr = loc;
	return 0;
}
/*
writing inside the data blocks of a file
if file is new, allocate a data block
fileID: file in the open descriptor table
buf : buffer to write from 
length : number of bytes to write 
return : length written
*/

int ssfs_fwrite(int fileID, char *buf, int length){
	
	if(fileID < 0 || fileID >= numberOfInodes){
		return -1;
	}
	
	// verify if file ID exist 
	if (fdt[fileID].inode == -1){
		return -1;
	}
	
	int inodeIndex = fdt[fileID].inode;
	int i = inodeIndex / 13;
	int slotIndex = inodeIndex % 13;
	int size = inodeBlocks[i].inodeSlot[slotIndex].size;
	int writeInDataBlock;
	
	// char to start writting from in a data block 
	int k = fdt[fileID].rwptr % 1024;
	int currentBlock = 0;
	int p;

	block_t write;	
		
	//check which block in the inode where the pointer is currently at 
	currentBlock = fdt[fileID].rwptr / blockSize;

	// if the block is > 14 than need to search through the indirect of the i-nodes	
	int m;
	int nextInodeOfFile = i;
	int nextSlot = slotIndex;
	int nextDirect = i;
	
	if (currentBlock >= 14){
		 nextInodeOfFile = currentBlock / 14; 
		 
		 // get to the last inode of the file 
		 for(m = 0; m < nextInodeOfFile; m++){
			nextInodeOfFile = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect / 13;
			nextSlot = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect % 13;
		 }
		 
		 // find the last direct block written in 
		 for(m = 0 ; m < 14; m++){
			 if (inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[m] == -1){
				 nextDirect = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[m - 1];
			 }
		 }
	}
	
	int totalWritingSize = fdt[fileID].rwptr + length;		
	int remaining = 0;
	int numberOfBlocksToAllocate = 0;
	int firstBlockDataLength = 0;
	int lastBlockDataLength = 0;
	
	// check if file is going to overflow to another data block
	if (totalWritingSize > (currentBlock + 1) * blockSize){
		// data to be written to fill up the current block 
		firstBlockDataLength = blockSize - k;
		// rest of data to be written in the other blocks 
		remaining = length - firstBlockDataLength;
		// need to allocate more blocks to satisfy write size
		if (remaining > 0){
			// need atleast 1 block to allocate 
			numberOfBlocksToAllocate = (remaining / blockSize) + 1;	
			lastBlockDataLength = remaining % blockSize;
		}
	}
	
	// If new file then allocate atleast 1 block 
	if(size == 0){
		allocateDataBlock(inodeIndex);
		inodeBlocks[i].inodeSlot[slotIndex].size = 0;
		fdt[fileID].rwptr = 0;
	}
	
	// don't need to allocate new data block if enough space
	if (numberOfBlocksToAllocate == 0){
		writeInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect];
		read_blocks(writeInDataBlock,1, &write);
		memcpy(write.bytes + k,buf,length);
		write_blocks(writeInDataBlock, 1, &write);
		inodeBlocks[i].inodeSlot[slotIndex].size = totalWritingSize;
		fdt[fileID].rwptr += length;
	
		return length;
	}
	
	else {
		
		// need to allocate more blocks if the length of the write is going to be a lot 	
		for(p = 0; p < numberOfBlocksToAllocate ; p++){
			allocateDataBlock(inodeIndex);
		}
				
		writeInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect];
		read_blocks(writeInDataBlock,1, &write);
		
		// start by writing to the current block;
		int dataLength = firstBlockDataLength; 
		k = size % 1024;
		memcpy(write.bytes + k, buf, dataLength);
		write_blocks(writeInDataBlock, 1, &write);
		// might have to write the inode block back
		int n = 0;
		
		// need to go to next inode  if next data block to write is after direct[14] 
		if ((nextDirect + n) > 14){
			nextInodeOfFile =  inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect / 13;
			nextSlot = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect % 13;
			nextDirect = 0;
		}
		
		// now need to write to the completely filled 
		if (numberOfBlocksToAllocate > 1){
			
			for (n = 1; n < numberOfBlocksToAllocate ; n++){
				nextDirect += 1;
				// need to go to next inode if next data block to write after direct[14]
				if (nextDirect > 14){
					nextInodeOfFile =  inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect / 13;
					nextSlot = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect % 13;
					nextDirect = 0;
				}
				
				writeInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect];
				read_blocks(writeInDataBlock, 1, &write);
				memcpy(write.bytes, buf +((currentBlock - 1 + n) * blockSize) + dataLength, blockSize);
				write_blocks(writeInDataBlock, 1, &write);
				
			}
			
		}
		
		// write to the last block  and update pointer 
		int lastDataLength = lastBlockDataLength;	
		writeInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect + 1];
		read_blocks(writeInDataBlock,1, &write);
		memcpy(&(write.bytes), buf + ((currentBlock - 1 + n) * blockSize) + dataLength, lastDataLength);
		write_blocks(writeInDataBlock, 1, &write);
		inodeBlocks[i].inodeSlot[slotIndex].size = totalWritingSize;
		fdt[fileID].rwptr += length;
		// might have to write the inode block back
		return length;
		}		
	return -1;
}

/*
Read inside the data blocks of a file
fileID: file in the open descriptor table
buf : buffer to write from 
length : number of bytes to write 
return : length written
*/

int ssfs_fread(int fileID, char *buf, int length){
	
	// verify if file ID exist 
	if(fileID < 0 || fileID >= numberOfInodes){
		return -1;
	}
	
	if (fdt[fileID].inode == -1){
		return -1;
	}
	
	int inodeIndex = fdt[fileID].inode;
	int i = inodeIndex / 13;
	int slotIndex = inodeIndex % 13;
	int size = inodeBlocks[i].inodeSlot[slotIndex].size;
	
	int readInDataBlock;
	int directNumber;	
	
	// char to start writting from in a data block 
	int k = fdt[fileID].readptr % 1024;
	int currentBlock = 0;

	block_t read;	
	
	currentBlock = fdt[fileID].readptr / blockSize;
	
	// if the block is > 14 than need to search through the indirect of the inodes	
	
	int m;
	int nextInodeOfFile = i;
	int nextSlot = slotIndex;
	int nextDirect = i;
	
	if (currentBlock >= 14){
		 nextInodeOfFile = currentBlock / 14; 
		 
		 // get to the last inode of the file 
		 for(m = 0; m < nextInodeOfFile; m++){
			nextInodeOfFile = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect / 13;
			nextSlot = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect % 13;
		 }
		 
		 // find the last direct block written in 
		 for(m = 0 ; m < 14; m++){
			 if (inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[m] == -1){
				 nextDirect = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[m - 1];
			 }
		 }
	}
	
	int totalReadSize = fdt[fileID].readptr + length;		
	int remaining = 0;
	int numberOfBlocksToRead = 0;
	int firstBlockDataLength = 0;
	int lastBlockDataLength = 0;
	
	// check if file is going to overflow 
	if (totalReadSize > (currentBlock + 1) * blockSize){
		// data to be read in the current block 
		firstBlockDataLength = blockSize - k ;
		// rest of data to be written in the other blocks 
		remaining = length - firstBlockDataLength;
		// need to read blocks if remaining is bigger than 0 
		if (remaining > 0){
			// need atleast 1 extra block to read
			numberOfBlocksToRead = (remaining / blockSize) + 1;	
			lastBlockDataLength = remaining % blockSize;
		}
	}
	
	else {
	
		// don't need to allocate new data block if enough space in the block  
		if (numberOfBlocksToRead == 0){
			
			directNumber = currentBlock;  
			readInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect];
			
			read_blocks(readInDataBlock,1, &read);
			
			memcpy(buf, read.bytes + k, length);
			buf[length] = '\0';
			fdt[fileID].readptr += length;
			
			return length;
		}
		
		else {
			readInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect];
			read_blocks(readInDataBlock,1, &read);
			
			// start by writing to the current block;
			int dataLength = firstBlockDataLength;
			k = size % 1024;
			
			char block[length];
			
			memcpy(block, read.bytes, dataLength);
			
			int n = 1;
			
			if ((nextDirect + n) > 14){
				nextInodeOfFile =  inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect / 13;
				nextSlot = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect % 13;
				nextDirect = 0;
			}
					
			// now need to write to the completely filled 
			if (numberOfBlocksToRead > 1){
				
				for (n = 1; n < numberOfBlocksToRead ; n++){
					
				nextDirect += 1;
				
					// need to go to next inode if next data block to write after direct[14]
				if (nextDirect > 14){
					nextInodeOfFile =  inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect / 13;
					nextSlot = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].indirect % 13;
					nextDirect = 0;
				}
				
				readInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect];
				read_blocks(readInDataBlock, 1, &read);
				
				memcpy(block + ((currentBlock - 1 + n) * blockSize) + dataLength , read.bytes, blockSize);
				                         
				}
				
			}
			
			// read the last block  and update pointer 
			int lastDataLength = lastBlockDataLength;
				
			readInDataBlock = inodeBlocks[nextInodeOfFile].inodeSlot[nextSlot].direct[nextDirect + 1];
			read_blocks(readInDataBlock,1, &read);
			
			memcpy(block + ((currentBlock - 1 + n) * blockSize) + dataLength,read.bytes, lastDataLength);
			
			strcpy(buf,block);
			fdt[fileID].readptr += length;
			return length;
			}		
	}
	
	return -1; 
}

/*
remove file from directory entry, release the i-node entry and releasr the data blocks by the file
*/ 
int ssfs_remove(char *file){
	int i,k,p;
	int inodeIndexFound;
	int inodeBlock; 
	int slot;
	
	if (strlen(file) > 10){
		return -1;
	}
	
	//Delete from root directory and remove inode and free data blocks of the inode
	for(k = 0; k < 4; k++){
		for(i = 0; i < 64; i++){
			
			//limited file to 200 
			if (k * 64 + i > 200){
				return -1;
			}
			
			if (strcmp(rootDirectory[k].entries[i].name, file) == 0){
				// found the entry
				inodeIndexFound = rootDirectory[k].entries[i].inodeIndex;
				
				//delete it from the directory 
				rootDirectory[k].entries[i].inodeIndex = -1;
				strcpy(rootDirectory[k].entries[i].name, "root/");
				
				inodeBlock = inodeIndexFound / 13;
				slot = inodeIndexFound % 13;
				
				// remove inode and inode files 
				
				inode_t removeInode;
				removeInode.size = -1;
				for(p = 0 ; p < 14; p++){
					if (inodeBlocks[inodeBlock].inodeSlot[slot].direct[p] != -1){
						setFBMbit(inodeBlocks[inodeBlock].inodeSlot[slot].direct[p]);
						}
						removeInode.direct[p] = -1;
				}
				
			// set the inode slot to be free 
			inodeBlocks[inodeBlock].inodeSlot[slot] = removeInode;
							
			//close file if open 
			
			for(i = 0; i < numberOfInodes; i++){
				if (fdt[i].inode == inodeIndexFound){
					fdt[i].inode = -1;
					fdt[i].free = -1;
					fdt[i].rwptr = 0;
					fdt[i].readptr = 0;
				}
			}
			
			return 0;
			}
		}
	}
	return -1;
}
