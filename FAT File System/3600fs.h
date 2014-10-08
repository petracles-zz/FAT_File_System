/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__

// Struct to represent a Volume Control Block:
typedef struct vcb_s
{
	// Identification #:
	int magic;

	// Disk layout:
	int blocksize;
	int mounted;
	int de_start;
	int de_length;
	int fat_start;
	int fat_length;
	int db_start;

	// Metadata:
	uid_t user;
	gid_t group;
	mode_t mode;
	struct timespec access_time;
	struct timespec modify_time;
	struct timespec create_time;
} vcb;

// Struct to represent a Directory Entry:
typedef struct dirent_s
{
	unsigned int valid;
	unsigned int first_block;
	unsigned int size;
	uid_t user;
	gid_t group;
	mode_t mode;
	struct timespec access_time;
	struct timespec modify_time;
	struct timespec create_time;
	char name[512];
} dirent;

// Struct to represent a File Allocation Table:
typedef struct fatent_s
{
	unsigned int used : 1;
	unsigned int eof : 1;
	unsigned int next : 30;
} fatent;

#endif

