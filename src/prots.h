#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>

#ifdef SUNOS
#include <errno.h>
#include <sys/uio.h>
#else
#include <sys/errno.h>
#endif

#include <limits.h>             /* HOST_NAME_MAX */
#include <sys/socket.h>         /* AF_INET, AF_INET6 */
#include <sys/utsname.h>        /* utsname */

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "defines.h"
#include "structs.h"
#include "globals.h"
#include "functions.h"
#include "ident.h"
#include "cmds.h"
#include "net.h"
#include "irc.h"

/* inteligence */
#include "work.h"
#include "work_fast.h"

