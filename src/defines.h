#define CHAN_LEN                50
#define NICK_LEN                15
#define MAX_CLONES              100
#define MAX_JOBS                1000        /* it should be 10 times bigger than MAX_CLONES define */
#define MAX_CONNECT_DELAY       86400       /* 1 day in secs */
#define LAG_CHECK_DELAY         20          /* secs. delay between lag check messsages */
#define JOIN_CHECK_DELAY        20          /* secs. delay of next try to join chan by clone */
#define JOIN_CHECK_INTERVAL     5           /* secs. every 5 secs we will check if any of clones can try to join chan */ 
#define DEFAULT_VHOST_FILE      ".vhosts"   /* default vhost file name */
#define DEFAULT_CONNECT_DELAY   1           /* 1 sec */

#define DEFAULT_SERVER_PORT     6667        /* default server port used when not given */
#define DEFAULT_CLONE_NUMBER    0           /* number of clones which will be loaded by default if number not given */

#define HAVE_IPV6               1           /* compile with ipv6 support */
#define HAVE_AUTHOR_CHECK       1           /* built-in author online/onchan check */

#define _NO_LAME_ERRNO          1           /* no lame errno while on kill socket */

#define IRC_BUF_LEN             512         /* irc buf size */
#define IDENT_BUF_LEN           64          /* ident buf size */
#define IDENT_DEFAULT_PORT      113         /* default ident port */
#define MAX_IDENT_LEN           9           /* if more than 9, generated ident got dash (^) */
#define NO_IDENT_ADDR_CHECK     1           /* no checking destination addr on accepting connection */
#define NO_IDENT_FILE           ".no-ident" /* no ident check file */

#define CLONER_VERSION_INT      "2000300"
#define CLONER_VERSION_STR      "0.2.3"
#define CLONER_AUTHOR           "Esio"
#define CLONER_EMAIL            "esio(at)hoth.amu.edu.pl"
#define CLONER_NAME             "cloner"

#define CLONE_BUF_SIZE          1024        /* clone buf size */

#ifdef INET6_ADDRSTRLEN
#define HOST_ADDRSTRLEN         INET6_ADDRSTRLEN
#else
#define HOST_ADDRSTRLEN         INET_ADDRSTRLEN
#endif

#ifndef HOST_NAME_MAX           /* does not exists on bsd family system */
#define HOST_NAME_MAX           256
#endif

#ifdef DEBUG
#define DEBUG_FILE 	            "debug.log"
#endif

#ifdef SUNOS
#define gethostbyname2(h, p) getipnodebyname(h, p, 0, &errno)
#endif

#ifdef WINDOWS
#ifndef HAVE_IPV6
#define gethostbyname2(h, p) gethostbyname(h)
#endif
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 	        (-1)
#endif

