/* check if string is valid ip (and ipv6 if compiled with) address */
unsigned short int is_valid_ip(const char *ip);
/* resolve server host to ip (and ipv6 if compiled with) */
unsigned int resolve_host(const char *host);
/* get next vhost index */
unsigned short int get_next_vhost_id();
/* write to a socket */
int sock_write(int fd, char *format, ...);
/* kill socket (shutdown and close) */
void kill_socket(int fd);
/* connect client using ipv4, non-blocking */
void connect4_clone(struct clone_connection *conn);
#ifdef HAVE_IPV6
/* connect client using ipv6, non-blocking */
void connect6_clone(struct clone_connection *conn);
#endif
/* connect a client */
void connect_clone(short int n);
/* start ident listening */
#ifdef HAVE_IPV6
/* start ident listening using ipv6, non-blocking */
int start_listening6();
#endif
/* start ident listening using ipv4, non-blocking */
int start_listening4();
/* start ident listening */
void start_listening();
#ifdef HAVE_IPV6
/* accept incomming connection using ipv6, non-blocking */
int accept_connection6();
#endif
/* accept incomming connection using ipv4, non-blocking */
int accept_connection4();
/* accept incomming connection */
void accept_connection();
/* check input on sockets and console */
void check_input(void);

