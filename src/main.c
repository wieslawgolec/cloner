#include "prots.h"

#ifdef HAVE_AUTHOR_CHECK
unsigned short int neutral = 0, author_online = 0;
#endif

unsigned short int current_number, number, join_delay = 0;
unsigned int delay = 1;

struct irc_server server;

char vhosts[MAX_CLONES][HOST_ADDRSTRLEN + 1];
unsigned short int vhost_number = 0, next_vhost = 0;

time_t now;

unsigned short int fast, mode;

char chan[CHAN_LEN + 1];
char key[CHAN_LEN + 1];
char nick[NICK_LEN + 1];
unsigned short int clones_on_chan = 0;
unsigned short int loaded;

struct irc_clone clone[MAX_CLONES];
struct user_list *user, *user2;

int order[MAX_CLONES], new_order[MAX_CLONES], next_check, check_num = 0, n_order,
		n_new_order, order_num = 0, o_start, o_end;

/* global join list: we count how many times we saw other clones joining
main idea: we synchronize number of messages depending on the number of joins
seen by a clone
message numbers: we rely on the clone who saw the most */
struct irc_join_list join_list[MAX_JOBS];
int n_join_list = 0;

/* queue sent commands so that we can check for errors and resend commands */
struct clone_queue queue[MAX_JOBS];
int n_queue = 0;

/* ident list */
short int ident_sock = 0;
struct ident_list *first_ident = NULL;
struct ident_list *ident, *ident2;

/* oplist: we let them be ops */
struct op_list *first_op = NULL;
struct op_list *op, *op2;

char outbuf[500], temp[500];

/* new clones loading system */
struct clone_connection *first_conn = NULL;
struct clone_connection *clone_conn, *clone_conn2;

/* main routine */
int main(int argc, char *argv[])
{
#ifdef HAVE_VALIDATOR
    validate(argv[0]); /* binary check */
#endif

	if(argc < 2) /* 3 args or more */
		usage(argv[0]);

    memset(&server, 0, sizeof(server));

    if(!resolve_host(argv[1]))
        error("cannot resolve server name");

    strncpy(server.name, argv[1], HOST_NAME_MAX);
    
#ifdef DEBUG
#ifdef HAVE_IPV6
    if(server.ip[0] && server.ipv6[0])
        printf("> Resolved server name: %s => ip: %s, ipv6: %s\n", server.name, server.ip, server.ipv6);
        
    if(!server.ip[0] && server.ipv6[0])
        printf("> Resolved server name: %s => ipv6: %s\n", server.name, server.ipv6);
        
    if(server.ip[0] && !server.ipv6[0])
        printf("> Resolved server name: %s => ip: %s\n", server.name, server.ip);
#else
    printf("> Resolved server name: %s => ip: %s\n", server.name, server.ip);
#endif
#endif    
  
    server.port = (argc > 2) ? htons(atoi(argv[2])) : htons(DEFAULT_SERVER_PORT);
    number = (argc > 3) ? abs(atoi(argv[3])) : DEFAULT_CLONE_NUMBER;
    delay = (argc > 4) ? abs(atoi(argv[4])) : DEFAULT_CONNECT_DELAY;

	if(!server.port)
		error("invalid port");
	if(number > MAX_CLONES)
		error("number not 0..%i", MAX_CLONES);
	if(delay > MAX_CONNECT_DELAY)
	    error("delay not 0..%i", MAX_CONNECT_DELAY);	

#ifdef HAVE_SSL
    /* Auto-enable SSL for the conventional secure port */
    if (ntohs(server.port) == DEFAULT_SSL_PORT) {
        server.use_ssl = 1;
        printf("> SSL/TLS enabled (port %d)\n", DEFAULT_SSL_PORT);
    }
    if (server.use_ssl) {
        if (!init_ssl())
            error("failed to initialise OpenSSL");
    }
#endif

    if(argc > 5) /* vhosts file on the end */
        load_vhosts(argv[5]);
    else // check for default vhosts file
    {
        // check if default file exists and can be readed
        if(access(DEFAULT_VHOST_FILE, (F_OK | R_OK)) != -1)
            load_vhosts(DEFAULT_VHOST_FILE);    
    }
    
	fast = 0;
	mode = 1;		

    propaganda();

    if(vhost_number)
        printf("> loaded %i ip address%s\n", vhost_number, (vhost_number == 1) ? "" : "es");

    if(number)
    {
#ifdef HAVE_IPV6
        if(!strcmp(server.name, (server.ipv6[0] ? server.ipv6 : server.ip)))
	        printf("> loading %i clone%s to %s:%i with %i sec delay\n", 
	                number, (number == 1 ? "" : "s"), server.name, ntohs(server.port), delay);
        else
	        printf("> loading %i clone%s to %s[%s]:%i with %i sec delay\n", 
	                number, (number == 1 ? "" : "s"), server.name, (server.ipv6[0] ? server.ipv6 : server.ip), ntohs(server.port), delay);
#else   
        if(!strcmp(server.name, server.ip))
	        printf("> loading %i clone%s to %s:%i with %i sec delay\n", 
	                number, (number == 1 ? "" : "s"), server.name, ntohs(server.port), delay);
        else
	        printf("> loading %i clone%s to %s[%s]:%i with %i sec delay\n", 
	                number, (number == 1 ? "" : "s"), server.name, server.ip, ntohs(server.port), delay);	        
#endif
    }
    
	memset(clone, 0, sizeof(clone));
	memset(join_list, 0, sizeof(join_list));
	memset(queue, 0, sizeof(queue));
	memset(chan, 0, sizeof(chan));
	memset(key, 0, sizeof(key));

    unsigned short int i;  
	time_t next_connect = time(NULL);
		
	srand(next_connect); /* randomize */

    // check if no ident lock file exists
    if(access(NO_IDENT_FILE, (F_OK | R_OK)) == -1)
    {	
	    start_listening();
    }
    
	for(i = 0; loaded < number; )
	{
		now = time(NULL);
		if(now >= next_connect && i < number)
		{
			connect_clone(i++);
			next_connect = now + delay;
		}
		check_input();
	}    
    
    current_number = loaded;
    time_t next_join = 0;

	for(;;)
	{
		now = time(NULL);

		if(loaded < number) // still something to load
		{
		    if(now >= next_connect && current_number < number)
		    {
			    connect_clone(current_number++);
			    next_connect = now + delay;
		    }    
		}

        // order check
	    if(now >= next_check)
	    {
		    check_order();
		    next_check = now + LAG_CHECK_DELAY;
		    o_start = now;
	    }
        
		check_input();
		
        // delay join check
        if(clones_on_chan)
        {
		    if(fast)
			    work_fast();
		    else
			    work();            
        
            if(now >= next_join) 
            {
                if(join_delay)
                {
                    if(clone[i].on_channel) // just switch to next one, if already on channel
                    {
                        i++;
                    }
                    else
                    {
                        next_join = now + join_delay;
                        clone[i].next_join = now + JOIN_CHECK_DELAY;          
                                
                        add_penalty(i, 3);
                        sock_write(clone[i++].sock, "JOIN %s %s\r\n", chan, key);
                    }
                    
                    if(i == loaded)
                    {
                        i = 0;
#ifdef DEBUF                
                        printf("> clones joined %s with delay %d\n", chan, join_delay); 
#endif
                        join_delay = 0;
                        
                        next_join = now + JOIN_CHECK_INTERVAL;
                    }
                }
                else
                {
                    check_join();          
                    next_join = now + JOIN_CHECK_INTERVAL;                
                }
            }
        }		
	}

#ifdef HAVE_SSL
    cleanup_ssl();
#endif
	return 0;
}

