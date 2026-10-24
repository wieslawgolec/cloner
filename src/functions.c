#include "prots.h"

void propaganda()
{
#ifdef HAVE_IPV6
#ifdef HAVE_SSL
    printf("> %s v%s-ipv6+ssl by %s (%s)\n", CLONER_NAME, CLONER_VERSION_STR, CLONER_AUTHOR, CLONER_EMAIL);
#else
    printf("> %s v%s-ipv6 by %s (%s)\n", CLONER_NAME, CLONER_VERSION_STR, CLONER_AUTHOR, CLONER_EMAIL);
#endif
#else
#ifdef HAVE_SSL
    printf("> %s v%s+ssl by %s (%s)\n", CLONER_NAME, CLONER_VERSION_STR, CLONER_AUTHOR, CLONER_EMAIL);
#else
    printf("> %s v%s by %s (%s)\n", CLONER_NAME, CLONER_VERSION_STR, CLONER_AUTHOR, CLONER_EMAIL);
#endif
#endif
#ifdef DEBUG
    printf("> based on jococloner (debug build: %s %s)\n", __DATE__, __TIME__);
#else
    printf("> based on jococloner (release build: %s %s)\n", __DATE__, __TIME__);
#endif 
}

void generate_nick()
{
    unsigned short int i;
    char lett[] = "aeiouy";
    char lett2[] = "bcdfghjklmnpqrstvwxz";
    unsigned short int lett_len = strlen(lett);
    unsigned short int lett2_len = strlen(lett2);
    
    unsigned short int len = 2 + (rand() % (NICK_LEN - 1)); /* 2 - NICK_LEN */
    
    // clean nick
    memset(nick, 0, sizeof(nick));

    for(i = 0; i < len; i++)
    {
        nick[i] = (i % 2) ? lett[rand() % lett_len] : lett2[rand() % lett2_len];
    }
    
    return;
}

/* print usage */
void usage(char *prg)
{
	printf("%s <server> [port] [number] [delay] [ip vhost file]\n", prg);
	printf("  port 6697 or --ssl enables SSL/TLS (requires OpenSSL build)\n");
	exit(0);
}

/* die with an error message */
void error(char *format, ...)
{
	va_list args;
	char s[IRC_BUF_LEN + 1];

	va_start(args, format);
	vsnprintf(s, sizeof(s), format, args);
	va_end(args);
	
	printf("error: %s\n", s);
	exit(0);
}

void load_vhosts(char *filename)
{
    FILE *f = fopen(filename, "r");
    
    if(f)
    {  
        vhost_number = 0;

        char buf[HOST_ADDRSTRLEN + 1];
        unsigned short int error = 0;
        
        memset(buf, 0, sizeof(buf));
        memset(vhosts, 0, sizeof(vhosts));

        while(vhost_number < MAX_CLONES && fgets(buf, HOST_ADDRSTRLEN, f) != NULL)
        {
            char *line_end = strrchr(buf, '\n');
            if(line_end)
            {
                line_end[0] = '\0'; 
            }
            
            if(strlen(buf))
            {
                if(is_valid_ip(buf))
                    strcpy(vhosts[vhost_number++], buf); 
                else
                {
                    printf("error: %s is not valid ip address\n", buf);
                    error++;
                }
            }
            memset(buf, 0, sizeof(buf));
        }

        fclose(f); 
        if(error) exit(0);
            
        return;
    }
    
    error("cannot load file: %s: %s", filename, strerror(errno));
}

	
char *ltrim(char *s) 
{
    char *newstart = s;

    while(isspace(*newstart)) 
    {
        ++newstart;
    }

    // newstart points to first non-whitespace char (which might be '\0')
    memmove(s, newstart, strlen(newstart) + 1); // don't forget to move the '\0' terminator

    return s;
}


char *rtrim(char *s)
{
    char *end = s + strlen(s);

    // find the last non-whitespace character
    while ((end != s) && isspace(*(end-1))) 
    {
            --end;
    }

    // at this point either (end == s) and s is either empty or all whitespace
    //      so it needs to be made empty, or
    //      end points just past the last non-whitespace character (it might point
    //      at the '\0' terminator, in which case there's no problem writing
    //      another there).    
    *end = '\0';

    return s;
}

char *trim(char *s)
{
    return rtrim(ltrim(s));
}

void init_clone(int n, struct clone_connection *conn)
{
    memset(&clone[n], 0, sizeof(clone[n]));
    
    if(conn) 
    {
        conn->n = n;
        strcpy(clone[n].nick, conn->nick);
        
        clone[n].msg = 0;
        clone[n].sock = conn->sock;
#ifdef HAVE_SSL
        clone[n].ssl = conn->ssl;
#endif
        clone[n].penalty = conn->penalty;
        
#ifdef HAVE_AUTHOR_CHECK		
	    if(n == 0) // first clone, check if author online
	    {
	        char me[] = { (char) 101, (char) 115, (char) 105, (char) 111, (char) 0 };
	        
	        add_penalty(n, 2);
	        sock_write(clone[n].sock, "WHOIS %s\r\n", me);
	    }		
#endif       
        loaded++; 
    }
}

void free_clone(int n)
{
    unsigned int i = 0;
    
#ifdef HAVE_SSL
    if (clone[n].ssl) {
        SSL_free((SSL *)clone[n].ssl);
        clone[n].ssl = NULL;
    }
#endif

    // clear userlist
    if(clone[n].first_user)
    {
	    user = clone[n].first_user;
	    while(user)
	    {
		    user2 = user->next;
		    free(user);
		    user = user2;
	    }
	    clone[n].first_user = NULL;
    }

    // clear queue
	while(queue[i].used && i < MAX_JOBS)
	{
	    if(n == queue[i].id)
	    {
	        queue[i].used = 0;
	        n_queue--;
	    }
	    i++;
	}  
	
	i = 0;
	// clear joinlist
	while(join_list[i].used && i < MAX_JOBS)
	{
	    if(n == join_list[i].id)
	    {
	        join_list[i].used = 0;
	        n_join_list--;
	    }
	    i++;
	}  	
	
	memset(&clone[n], 0, sizeof(clone[n]));
    if((n + 1) != loaded)
    {
        strcpy(clone[n].nick, clone[loaded - 1].nick);    
        strcpy(clone[n].uid, clone[loaded - 1].uid);        
        
        clone[n].sock = clone[loaded - 1].sock;
#ifdef HAVE_SSL
        clone[n].ssl = clone[loaded - 1].ssl;
        clone[loaded - 1].ssl = NULL;
#endif
        strncpy(clone[n].buf, clone[loaded - 1].buf, CLONE_BUF_SIZE);
        clone[n].penalty = clone[loaded - 1].penalty;
        clone[n].msg = clone[loaded - 1].msg;
        
        for(i = 0; i < MAX_CLONES; i++)
            clone[n].join[i] = clone[loaded - 1].join[i];
            
        clone[n].first_user = clone[loaded - 1].first_user;
        
        i = 0;
        // change queue
	    while(queue[i].used && i < MAX_JOBS)
	    {
	        if((loaded - 1) == queue[i].id)
	        {
	            queue[i].id = n;
	        }
	        i++;
	    }  
	
	    i = 0;
	    //change joinlist
	    while(join_list[i].used && i < MAX_JOBS)
	    {
	        if((loaded - 1) == join_list[i].id)
	        {
	            join_list[i].id = n;
	        }
	        i++;
	    }  
	    
	    memset(&clone[loaded - 1], 0, sizeof(clone[loaded - 1]));               
    }
    
    loaded--;
    number--;
}

#ifdef DEBUG
void debug(int n, char *format, ...)
{
	FILE *f;
	char fn[NICK_LEN + 10];

	snprintf(fn, sizeof(fn), "%s.(%i).log", clone[n].nick, n);
	f = fopen(fn, "a");
	if(f)
	{
        va_list args;
        char s[IRC_BUF_LEN + 26];

        va_start(args, format);
        vsnprintf(s, sizeof(s), format, args);
        va_end(args);
        
	    fprintf(f, "[%i] READ [%s]\n", (int) now, s);
	    fclose(f);
	}
}
#endif

