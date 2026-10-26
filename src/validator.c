/**************************************************
*  This file is a part of psotnic irc bot code
*  Copyright 2003 Grzegorz Rusin <pks@irc.pl>
*
*  PRIVATE CODE - DO NOT DISTRIBUTE
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LEN 4096

#include "key.h"
#include "validator.h"
#include "validate.h"
#include "sha256.h"

int main(int argc, char *argv[])
{
	struct stat file;
	char *data, *magic, *p;
	unsigned char digest[32];
	unsigned char d1[32], d2[32];
	int n, fd;

	if(argc != 2)
	{
		printf("-+- Syntax: %s <file>\n", argv[0]);
		exit(1);
	}
	if((fd = open(argv[1], O_RDONLY)) == -1)
	{
		printf("-+- Error while opening '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}
	if(stat(argv[1], &file) == -1)
	{
		printf("-+- Error while fetching file size: %s\n", strerror(errno));
		close(fd);
		exit(1);
	}

	data = (char *) malloc(file.st_size);
	if (!data) {
		printf("-+- Out of memory\n");
		close(fd);
		exit(1);
	}
	if (read(fd, data, file.st_size) != file.st_size) {
		printf("-+- Read failed: %s\n", strerror(errno));
		free(data);
		close(fd);
		exit(1);
	}
	close(fd);

	magic = (char *) _memmem(data, file.st_size, FileMD5, 36);

	if(!magic)
	{
		printf("-+- Cannot locate magic string\n");
		printf("-+- (File already patched or validate.o is not linked into binary file)\n");
		free(data);
		exit(1);
	}

	p = _memmem(magic+36, file.st_size - abs(magic - data), (char *) FileMD5, 36);

	if(p)
	{
		printf("-+- Magic string is not unique, change magic string\n");
		free(data);
		exit(1);
	}

	/* Two SHA-256s; keep first 16 bytes of each -> 32 bytes total (same layout) */
	SHA256Hash(d1, data, abs(data-magic), Key, sizeof(Key)-1);
	SHA256Hash(d2, magic+36, file.st_size-abs(data-magic)-36, Key, sizeof(Key)-1);
	memcpy(digest,      d1, 16);
	memcpy(digest + 16, d2, 16);

	memcpy(magic, digest, 32);
	n = abs(magic - data);
	memcpy(magic+32, (const void *) &n, 4);

	if((fd = open(argv[1], O_RDWR | O_CREAT, 0755)) == -1)
	{
		printf("-+- Error while opening '%s' for writing: %s\n", argv[1], strerror(errno));
		free(data);
		exit(1);
	}
	if(write(fd, data, file.st_size) !=  file.st_size)
	{
		printf("-+- Write failed: %s\n", strerror(errno));
		free(data);
		close(fd);
		exit(1);
	}
	printf("-+- File patched\n");
	close(fd);
	free(data);
	return 0;
}
