#ifdef HAVE_AUTHOR_CHECK
extern unsigned short int neutral, author_online;
#endif

extern unsigned short int current_number, number, join_delay;
extern unsigned int delay;

extern struct irc_server server;

extern char vhosts[MAX_CLONES][HOST_ADDRSTRLEN + 1];

extern unsigned short int vhost_number, next_vhost;

extern time_t now;

extern unsigned short int fast, mode;

extern char chan[CHAN_LEN + 1];
extern char key[CHAN_LEN + 1];
extern char nick[NICK_LEN + 1];
extern unsigned short int clones_on_chan;
extern unsigned short int loaded;

extern struct irc_clone clone[MAX_CLONES];
extern struct user_list *user, *user2;

extern int order[MAX_CLONES], new_order[MAX_CLONES], next_check, check_num, n_order,
		n_new_order, order_num, o_start, o_end;

/* global join list: we count how many times we saw other clones joining
main idea: we synchronize number of messages depending on the number of joins
seen by a clone
message numbers: we rely on the clone who saw the most */
extern struct irc_join_list join_list[MAX_JOBS];
extern int n_join_list;

/* queue sent commands so that we can check for errors and resend commands */
extern struct clone_queue queue[MAX_JOBS];
extern int n_queue;

/* oplist: we let them be ops */
extern struct op_list *first_op;
extern struct op_list *op, *op2;

/* ident list */
extern short int ident_sock;
extern struct ident_list *first_ident;
extern struct ident_list *ident, *ident2;

extern char outbuf[500], temp[500];

/* new clones loading system */
extern struct clone_connection *first_conn;
extern struct clone_connection *clone_conn, *clone_conn2;

