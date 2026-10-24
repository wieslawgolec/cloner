/* add penalty to a clone like ircd does */
void add_penalty(int n, int t);
/* check clone penalty */
int has_penalty(int n);
/* get nick from nick!user@host */
void get_nick(char *who);
/* get clone id from nick */
int get_clone_id(char *nick);
/* set client flags: id, is in oplist */
void set_flags(struct user_list *who);
/* join list management */
void check_join_list(void);
/* someone joined */
void got_join(int n, char *who);
/* NAMES reply from ircd */
void got_names(int n, char *names);
void got_end_of_names(int n);
/* delete user from channel for various reasons */
void got_part_quit_kick(int n, char *who);
void got_kick(int n, char *who, char *who2);
/* nick change */
void got_nick(int n, char *who, char *new_nick);
/* someone changed modes */
void got_mode(int n, char *who, char *mode, char *nick1, char *nick2, char *nick3);
/* a command failed, check command queue */
void got_fail(int n);
/* privmsg: order check */
void got_privmsg(int n, char *who, char *what);
void got_ctcp(int n, char *who, char *what);
#ifdef HAVE_AUTHOR_CHECK
void got_whois(int n, char *nick, char *user, char *host);
#endif
/* main ircd message parser */
short int irc_msg(int n, char *b);
/* network parser */
void irc(int n);
/* add a command to queue */
void add_queue(int id, int type, char *nick);
/* do an order check */
void check_order();
/* do a join check */
void check_join();

/*
 * New clones loading/connecting system
 */
 
/* conecting clone add penalty like ircd does */
void add_conn_penalty(struct clone_connection *conn, int t);
/* conecting clone check penalty */
int has_conn_penalty(struct clone_connection *conn);
/* conecting clones main ircd message parser */
void irc_conn_msg(struct clone_connection *conn, char *b);
/* connecting clones network parser */
void irc_conn(struct clone_connection *conn);

