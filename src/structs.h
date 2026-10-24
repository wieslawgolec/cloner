struct irc_server 
{
    char name[HOST_NAME_MAX + 1];
#ifdef HAVE_IPV6
    char ipv6[INET6_ADDRSTRLEN + 1];
#endif
    char ip[INET_ADDRSTRLEN + 1];    

    unsigned short int port;
};

struct ident_list
{
    short int sock;
    unsigned short int conn;
    char buf[IDENT_BUF_LEN + 1];
    
    struct ident_list *next;
};

struct user_list 
{
	char nick[NICK_LEN + 1];
	short int op; /* is op */
	short int clone; /* is clone */
	short int op_list; /* is in op_list */
	struct user_list *next;
};

struct clone_connection
{
    short int n;
    unsigned short int id;
    char nick[NICK_LEN + 1];
    char user[MAX_IDENT_LEN + 1]; /* user ident */
	short int sock;
	int status; // conn before
	char buf[CLONE_BUF_SIZE + 1];
	int penalty;    

	struct clone_connection *next;
};

struct irc_clone 
{
    char nick[NICK_LEN + 1];
    char uid[NICK_LEN + 1]; /* user unique id */
	short int sock;
	
    time_t next_join; /* next join time try */
    unsigned short int on_channel;
    	
	char buf[CLONE_BUF_SIZE + 1];
	int penalty;    

	int join[MAX_CLONES]; /* local join list */
	int msg; /* number of messages (seen on channel) */	
	struct user_list *first_user;
};

/* global join list: we count how many times we saw other clones joining
main idea: we synchronize number of messages depending on the number of joins
seen by a clone
message numbers: we rely on the clone who saw the most */
struct irc_join_list 
{
	unsigned char used, id, num;
	int msg;
};

/* queue sent commands so that we can check for errors and resend commands */
struct clone_queue 
{
	unsigned char used, id, type; /* type: 0 = op, 1 = kick, 2 = deop */
	char nick[NICK_LEN + 1];
};

/* oplist: we let them be ops */
struct op_list 
{
	char nick[NICK_LEN + 1];
	struct op_list *next;
};

