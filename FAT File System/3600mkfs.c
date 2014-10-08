/*
* CS3600, Spring 2014
* Project 2 Starter Code
* (c) 2013 Alan Mislove
*
* This program is intended to format your disk file, and should be executed
* BEFORE any attempt is made to mount your file system.  It will not, however
* be called before every mount (you will call it manually when you format
* your disk file).
*/

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "3600fs.h"
#include "disk.h"


/* No header, so declarations: */
vcb initvcb(int numofde);
dirent initde();
fatent initfat();

void myformat(int size) {
	// Do not touch or move this function
	dcreate_connect();

	/* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
	WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */

	vcb volcb;
	volcb = initvcb(size);
	dirent de;
	de = initde();
	fatent fat;
	fat = initfat();

	char * temp = (char *)malloc(BLOCKSIZE); // Zero'd out memory
	memset(temp, 0, BLOCKSIZE);
	memcpy(temp, &volcb, sizeof(vcb)); // Copy to temp
	dwrite(0, temp); // Write to disk

	memset(temp, 0, BLOCKSIZE);
	memcpy(temp, &de, sizeof(dirent));
	for (int i = 1; i < 101; i++) {
		dwrite(i, temp); // Write each dirent to disk
	}

	// first we want to create a block of just fatents
	memset(temp, 0, BLOCKSIZE);
	for (int i = 0; i < BLOCKSIZE; i = i + 4) {
		memcpy(temp + i, &fat, sizeof(fatent));
	}

	for (int i = volcb.fat_start; i < (volcb.fat_start + volcb.fat_length); i++) {
		dwrite(i, temp);
	}

	memset(temp, 0, BLOCKSIZE);
	for (int i = volcb.db_start; i < size; i++) {
		dwrite(i, temp);
	}

	dunconnect();
}

vcb initvcb(int numofde) {
	vcb volcb;
	volcb.magic = 1337;
	volcb.blocksize = BLOCKSIZE;
	volcb.mounted = 0;
	volcb.de_start = 1;
	volcb.de_length = 0;
	volcb.fat_start = volcb.de_start + volcb.de_length + 1;
	volcb.fat_length = 0;
	volcb.db_start = volcb.fat_start + volcb.fat_length + 1;

	volcb.user = getuid();
	volcb.group = getgid();
	volcb.mode = 0777;

	clock_gettime(CLOCK_REALTIME, &volcb.access_time);
	clock_gettime(CLOCK_REALTIME, &volcb.modify_time);
	clock_gettime(CLOCK_REALTIME, &volcb.create_time);

	return volcb;
}

dirent initde() {
	dirent de;
	de.valid = 0;
	de.size = 0;
	return de;
}

fatent initfat() {
	fatent fat;
	fat.used = 0;
	fat.eof = 0;
	return fat;
}

int main(int argc, char** argv) {
	// Do not touch this function
	if (argc != 2) {
		printf("Invalid number of arguments \n");
		printf("usage: %s diskSizeInBlockSize\n", argv[0]);
		return 1;
	}

	unsigned long size = atoi(argv[1]);
	printf("Formatting the disk with size %lu \n", size);
	myformat(size);
}
