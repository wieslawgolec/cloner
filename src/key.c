#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLONER_KEYFILE "key.h"

int r(int j)
{
    return rand() % j;
}

int main()
{
    int i;
    FILE *f;
    char buf[256];
    
    if((f = fopen(CLONER_KEYFILE, "w")))
    {
	    memset(buf, 0, sizeof(buf));
	    srand(time(NULL)); /* randomize */

	    sprintf(buf, "\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x", 
	    r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256), r(256));
	    fprintf(f, "/*\n * This file contains magical key, that will be used during patching process.\n");
	    fprintf(f, " */\n\n");
	    fprintf(f, "static unsigned char Key[] = \"%s\";\n", buf);
	    fprintf(f, "static unsigned char FileMD5[] = \"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXYYYY\";\n"); 
	    fclose(f);
	    return 0;
    }

    return 1;
}
