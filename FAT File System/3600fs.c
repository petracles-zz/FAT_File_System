/*
* CS3600, Spring 2014
* Project 2 Starter Code
* (c) 2013 Alan Mislove
*
* This file contains all of the basic functions that you will need
* to implement for this project.  Please see the project handout
* for more details on any particular function, and ask on Piazza if
* you get stuck.
*/

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

/*
// Our happy little magic number. How cute!!
#define MAGIC 1337;
*/

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

// Instances:
vcb volcb;
dirent de[100];
fatent * fats;

// OH THOSE LOVELY HELPERS!!!

int readvcb(vcb v) {
	char tmpVCB[BLOCKSIZE];
	memset(tmpVCB, 0, BLOCKSIZE);
	dread(0, tmpVCB);
	memcpy(&v, tmpVCB, sizeof(vcb));
	/*
	if (v.mounted != 0) {
	return -1;
	}
	else if (v.magic != 1337) {
	return -2;
	}
	else {
	return 0;
	}
	*/
	return 0;
}

int writevcb(vcb v) {
	char tempVCB[BLOCKSIZE];
	memset(tempVCB, 0, BLOCKSIZE);
	memcpy(tempVCB, &v, sizeof(vcb));
	dwrite(0, tempVCB);
	return 0;
}

int readdirent(dirent * des, int index) {
	char tempDE[BLOCKSIZE];
	memset(tempDE, 0, BLOCKSIZE);
	dread(index, tempDE);
	memcpy(&des[index - 1], tempDE, sizeof(dirent));
	return 0;
}

int writedirent(dirent * des, int index) {
	char tempDE[BLOCKSIZE];
	memset(tempDE, 0, BLOCKSIZE);
	memcpy(tempDE, &des[index], sizeof(dirent));
	dwrite(index + 1, tempDE);
	return 0;
}

/* Determine if the given path has only one directory level, as in
it only has one slash */
int validatepath(const char * path) {
	const char * ptemp = path;
	int count = 0; // Counter for slashes

	// Parse thru and count the slashes
	while (*ptemp) {
		if (*ptemp == '/') {
			count++;
		}
		ptemp++;
	}
	if (count != 1) {
		return 0;
	}
	else {
		return 1;
	}
}

/*
* Initialize filesystem. Read in file system metadata and initialize
* memory structures. If there are inconsistencies, now would also be
* a good time to deal with that.
*
* HINT: You don't need to deal with the 'conn' parameter AND you may
* just return NULL.
*
*/
static void* vfs_mount(struct fuse_conn_info *conn) {
	fprintf(stderr, "vfs_mount called\n");

	// Do not touch or move this code; connects the disk
	dconnect();

	/* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
	AND LOAD ANY DATA STRUCTURES INTO MEMORY */

	int vcbread = readvcb(volcb);

	// Check the validity of our VCB & then read into memory:
	if (vcbread == -1) {
		fprintf(stderr, "Invalid: Error in previous unmounting.\n");
	}
	if (vcbread == -2) {
		fprintf(stderr, "Invalid: Non-matching magic number.\n");
	}
	else {
		volcb.mounted = 1;
		int vcbwrite = writevcb(volcb);
		if (vcbwrite != 0) {
			fprintf(stderr, "Invalid: Unable to mount disk.\n");
		}
	}

	// Read the dirents into memory
	for (int i = 1; i < 101; i++) {
		readdirent(de, i);
	}

	return NULL;
}

/*
* Called when your file system is unmounted.
*
*/
static void vfs_unmount(void *private_data) {
	fprintf(stderr, "vfs_unmount called\n");

	/* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
	ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
	KEEP DATA CACHED THAT'S NOT ON DISK */

	// Format the dirent's:
	for (int i = 0; i < 100; i++) {
		char temp[BLOCKSIZE];
		memset(temp, 0, BLOCKSIZE);
		memcpy(temp, &de[i], sizeof(dirent));
		dwrite(i + 1, temp);
	}

	// Format the FAT's:
	for (int i = 0; i < volcb.fat_length; i++) {
		char temp[BLOCKSIZE];
		memset(temp, 0, BLOCKSIZE);
		memcpy(temp, fats + 128 * i, BLOCKSIZE);
		dwrite(volcb.fat_start + 1, temp);
	}

	// Do not touch or move this code; unconnects the disk
	dunconnect();
}

/*
*
* Given an absolute path to a file/directory (i.e., /foo ---all
* paths will start with the root directory of the CS3600 file
* system, "/"), you need to return the file attributes that is
* similar stat system call.
*
* HINT: You must implement stbuf->stmode, stbuf->st_size, and
* stbuf->st_blocks correctly.
*
*/
static int vfs_getattr(const char *path, struct stat *stbuf) {
	fprintf(stderr, "vfs_getattr called\n");

	// Do not mess with this code 
	stbuf->st_nlink = 1; // hard links
	stbuf->st_rdev = 0;
	stbuf->st_blksize = BLOCKSIZE;

	/* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */

	// Check if the root directory
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = 0777 | S_IFDIR;
		return 0;
	}

	// int to hold the available block for the file:
	int index = -1;

	// Find the location of the path:
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, path) == 0 && de[i].valid) {
			index = i;
		}
	}

	if (index == -1) {
		return -ENOENT;
	}

	stbuf->st_mode = de[index].mode | S_IFREG;
	stbuf->st_uid = de[index].user;
	stbuf->st_gid = de[index].group;
	stbuf->st_atime = de[index].access_time.tv_sec;
	stbuf->st_mtime = de[index].modify_time.tv_sec;
	stbuf->st_ctime = de[index].create_time.tv_sec;
	stbuf->st_size = de[index].size;
	stbuf->st_blocks = de[index].size / BLOCKSIZE;

	return 0;
}

/*
* Given an absolute path to a directory (which may or may not end in
* '/'), vfs_mkdir will create a new directory named dirname in that
* directory, and will create it with the specified initial mode.
*
* HINT: Don't forget to create . and .. while creating a
* directory.
*/
/*
* NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE
*       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
*       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

return -1;
} */

/** Read directory
*
* Given an absolute path to a directory, vfs_readdir will return
* all the files and directories in that directory.
*
* HINT:
* Use the filler parameter to fill in, look at fusexmp.c to see an example
* Prototype below
*
* Function to add an entry in a readdir() operation
*
* @param buf the buffer passed to the readdir() operation
* @param name the file name of the directory entry
* @param stat file attributes, can be NULL
* @param off offset of the next entry or zero
* @return 1 if buffer is full, zero otherwise
* typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
*                                 const struct stat *stbuf, off_t off);
*
* Your solution should not need to touch fi
*
*/
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	// Check to see if path has "/"
	if (strcmp(path, "/") != 0) {
		return -1;
	}
	int firstde = offset;
	for (firstde; firstde < 100; firstde++) {
		/* Call filler with args buf, filename, NULL, and next de
		and if no more files exist return 0, else -1 */
		if (de[firstde].valid == 1) {
			if (!filler(buf, de[firstde].name + 1, NULL, firstde + 1)) {
				return 0;
			}
		}
	}
	return 0;
}

/*
* Given an absolute path to a file (for example /a/b/myFile), vfs_create
* will create a new file named myFile in the /a/b directory.
*
*/
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	/*
	// Check path validity:
	if (validatepath(path) != 1) {
	return -1; // Error
	}
	*/

	// Check to see if file already exists:
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, path) == 0) {
			return -EEXIST;
		}
	}

	// Make sure there is an empty de for the file:
	int space = -1;
	for (int i = 0; i < 100; i++) {
		if (de[i].valid == 0) {
			space = i;
			break;
		}
	}
	if (space == -1) {
		return -1; // ERR there is no room for the file
	}

	// The file doesn't already exist, create the dirent:
	de[space].valid = 1;
	de[space].user = geteuid();
	de[space].group = getegid();
	de[space].mode = mode;
	clock_gettime(CLOCK_REALTIME, &de[space].access_time);
	clock_gettime(CLOCK_REALTIME, &de[space].modify_time);
	clock_gettime(CLOCK_REALTIME, &de[space].create_time);
	strcpy(de[space].name, path);
	return 0;
}

/*
* The function vfs_read provides the ability to read data from
* an absolute path 'path,' which should specify an existing file.
* It will attempt to read 'size' bytes starting at the specified
* offset (offset) from the specified file (path)
* on your filesystem into the memory address 'buf'. The return
* value is the amount of bytes actually read; if the file is
* smaller than size, vfs_read will simply return the most amount
* of bytes it could read.
*
* HINT: You should be able to ignore 'fi'
*
*/
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
struct fuse_file_info *fi)
{
	// int to hold the available block for the file:
	int index = -1;

	// Find the location of the path:
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, path) == 0 && de[i].valid) {
			index = i;
		}
	}

	if (index == -1) {
		return -1;
	}

	// Read size bytes from path, starting at offset to buf:
	int bytes = 0;
	if (size + offset > de[index].size) {
		bytes = de[index].size - offset;
	}

	// Only read one block:
	if (offset % BLOCKSIZE + bytes <= BLOCKSIZE) {
		char temp[BLOCKSIZE];
		memset(temp, 0, BLOCKSIZE);
		dread(volcb.db_start + de[index].first_block, temp);
		memcpy(buf, temp + offset % BLOCKSIZE, bytes);
	}

	return bytes;
}

/*
* The function vfs_write will attempt to write 'size' bytes from
* memory address 'buf' into a file specified by an absolute 'path'.
* It should do so starting at the specified offset 'offset'.  If
* offset is beyond the current size of the file, you should pad the
* file with 0s until you reach the appropriate length.
*
* You should return the number of bytes written.
*
* HINT: Ignore 'fi'
*/
static int vfs_write(const char *path, const char *buf, size_t size,
	off_t offset, struct fuse_file_info *fi)
{

	/* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
	MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */

	// int to hold the available block for the file:
	int index = -1;

	// Find the location of the path:
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, path) == 0 && de[i].valid) {
			index = i;
		}
	}

	if (index == -1) {
		return -1;
	}

	// write "size" bytes from "buf" into "path" starting at "offset"
	// if "size" is 0 for whatever reason, do nothing
	if (size == 0)
		return 0;

	// For files smaller than BLOCKSIZE
	int filesize = de[index].size;
	if (filesize == 0 && size <= BLOCKSIZE) {
		char temp[BLOCKSIZE];
		memset(temp, 0, BLOCKSIZE);
		memcpy(temp + offset, buf, size);

		// Locate a free fat entry
		int startblock = -1;
		for (int i = 0; i < volcb.fat_length * 128; i++)
		{
			if (fats[i].used == 0)
			{
				fats[i].used = 1;
				fats[i].eof = 1;
				startblock = i;
			}
		}
		if (startblock == -1)
		{
			return -1; // -1 if not found
		}
		de[index].first_block = startblock;
		de[index].size = size + offset;
		dwrite(volcb.db_start + startblock, temp);
		return size + offset;
	}

	/*
	int currblocks = (filesize + (BLOCKSIZE - 1)) / BLOCKSIZE;
	int newsize; // The new size of the file
	if ((offset + size) > filesize)
	{
		newsize = offset + size;
	}
	else
	{
		newsize = filesize; // Overwrite existing data
	}
	int newblocks = (newsize + (BLOCKSIZE - 1)) / BLOCKSIZE; // The new blocks for the file
	int numnew = newblocks - currblocks; // The number of new blocks we need
	int fatnums[currblocks]; // Determine used blocks
	if (currblocks > 0)
	{
		int startblock = mydirents[block].first_block;
		for (int i = 0; i < currblocks; i++) {
			// write write write
		}
	}

	return 0;
	*/
}

/**
* This function deletes the last component of the path (e.g., /a/b/c you
* need to remove the file 'c' from the directory /a/b).
*/
static int vfs_delete(const char *path)
{
	/* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
	AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */

	// Locate the file and mark its de location as free:
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, path) == 0 && de[i].valid) {
			de[i].valid = 0;
			de[i].name[0] = '\0';
			return 0;
		}
	}

	return -1; // If the file wasn't found return -1

}

/*
* The function rename will rename a file or directory named by the
* string 'oldpath' and rename it to the file name specified by 'newpath'.
*
* HINT: Renaming could also be moving in disguise
*
*/
static int vfs_rename(const char *from, const char *to)
{
	// Locate the file and change its name:
	for (int i = 0; i < 100; i++) { // Parse thru looking for file
		if (strcmp(de[i].name, from) == 0 && de[i].valid == 1) {
			strcpy(de[i].name, to); // Update file name
			return 0;
		}
	}

	return -1; // If the file wasn't found return -1
}


/*
* This function will change the permissions on the file
* to be mode.  This should only update the file's mode.
* Only the permission bits of mode should be examined
* (basically, the last 16 bits).  You should do something like
*
* fcb->mode = (mode & 0x0000ffff);
*
*/
static int vfs_chmod(const char *file, mode_t mode)
{
	// Locate the block
	int index = -1;
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, file) == 0 && de[i].valid) {
			index = i;
		}
	}
	if (index == -1) {
		return -1; // If block was not found
	}
	de[index].mode = (mode & 0x0000ffff); // Update mode

	return 0;
}

/*
* This function will change the user and group of the file
* to be uid and gid.  This should only update the file's owner
* and group.
*/
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
	// Locate the block
	int index = -1;
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, file) == 0 && de[i].valid) {
			index = i;
		}
	}
	if (index == -1) {
		return -1; // If block was not found
	}
	de[index].user = uid; // Update user ID
	de[index].group = gid; // Update group ID
	return 0;
}

/*
* This function will update the file's last accessed time to
* be ts[0] and will update the file's last modified time to be ts[1].
*/
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
	// Locate the block
	int index = -1;
	for (int i = 0; i < 100; i++) {
		if (strcmp(de[i].name, file) == 0 && de[i].valid) {
			index = i;
		}
	}
	if (index == -1) {
		return -1; // If block was not found
	}
	de[index].access_time = ts[0]; // Update access time
	de[index].modify_time = ts[1]; // Update modify time
	return 0;
}

/*
* This function will truncate the file at the given offset
* (essentially, it should shorten the file to only be offset
* bytes long).
*/
static int vfs_truncate(const char *file, off_t offset)
{

	/* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
	BE AVAILABLE FOR OTHER FILES TO USE. */
	truncate(file, offset);

	return 0;
}


/*
* You shouldn't mess with this; it sets up FUSE
*
* NOTE: If you're supporting multiple directories for extra credit,
* you should add
*
*     .mkdir	 = vfs_mkdir,
*/
static struct fuse_operations vfs_oper = {
	.init = vfs_mount,
	.destroy = vfs_unmount,
	.getattr = vfs_getattr,
	.readdir = vfs_readdir,
	.create = vfs_create,
	.read = vfs_read,
	.write = vfs_write,
	.unlink = vfs_delete,
	.rename = vfs_rename,
	.chmod = vfs_chmod,
	.chown = vfs_chown,
	.utimens = vfs_utimens,
	.truncate = vfs_truncate,
};

int main(int argc, char *argv[]) {
	/* Do not modify this function */
	umask(0);
	if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
		printf("Usage: ./3600fs -s -d <dir>\n");
		exit(-1);
	}
	return fuse_main(argc, argv, &vfs_oper, NULL);
}

