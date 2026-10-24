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
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "md5.h"
#include "validate.h"
#include "key.h"

void validate(char *thisfile)
{
	int fd;

	if((fd = open(thisfile, O_RDONLY)) > 0)
	{
		struct stat file;

		if(stat(thisfile, &file) == 0)
		{
			int n, i;
			int ok = 1;
			char *data;
			unsigned char digest[32];

			data = (char *) malloc(file.st_size);
			if(read(fd, data, file.st_size) == file.st_size);
			{
				memcpy(&n, &FileMD5[32], 4);

				MD5Hash(&digest[0], data, n, Key, sizeof(Key)-1);
				MD5Hash(&digest[16], data+n+36, file.st_size-n-36, Key, sizeof(Key)-1);

				for(i=0; i<32; ++i)
				{
					if(FileMD5[i] != digest[i])
					{
						ok = 0;
						break;
					}
				}
				close(fd);
				free(data);
				if(ok) return;
			}
			free(data);
		}
	}
	printf("> This file is hacked\n");
	exit(1);
}
