#include "prots.h"

#ifdef HAVE_SSL
/* Find SSL* associated with an fd among loaded clones and connecting conns */
static void *find_ssl_by_fd(int fd)
{
    unsigned short int i;
    for (i = 0; i < loaded; i++) {
        if (clone[i].sock == fd && clone[i].ssl)
            return clone[i].ssl;
    }
    for (clone_conn = first_conn; clone_conn; clone_conn = clone_conn->next) {
        if (clone_conn->sock == fd && clone_conn->ssl)
            return clone_conn->ssl;
    }
    return NULL;
}

static void free_ssl_by_fd(int fd)
{
    unsigned short int i;
    for (i = 0; i < loaded; i++) {
        if (clone[i].sock == fd && clone[i].ssl) {
            SSL_free((SSL *)clone[i].ssl);
            clone[i].ssl = NULL;
            return;
        }
    }
    for (clone_conn = first_conn; clone_conn; clone_conn = clone_conn->next) {
        if (clone_conn->sock == fd && clone_conn->ssl) {
            SSL_free((SSL *)clone_conn->ssl);
            clone_conn->ssl = NULL;
            return;
        }
    }
}
#endif

/* check if string is valid ip (and ipv6 if compiled with) address */
unsigned short int is_valid_ip(const char *ip)
{
#ifdef HAVE_IPV6
    struct sockaddr_in6 sin6;
    
    if(inet_pton(AF_INET6, ip, (void *) &sin6.sin6_addr) > 0)
    {
        return AF_INET6;
    }
#endif
    struct sockaddr_in sin;
    
    if(inet_pton(AF_INET, ip, (void *) &sin.sin_addr) > 0)
    {
        return AF_INET;
    }
    
    return 0;
}

/* resolve server host to ip (and ipv6 if compiled with) */
unsigned int resolve_host(const char *host)
{
    switch(is_valid_ip(host))
    {
#ifdef HAVE_IPV6
        case AF_INET6:
            strcpy(server.ipv6, host);
            return AF_INET6;        
#endif
        case AF_INET:
            strcpy(server.ip, host);
            return AF_INET;
    }
    
#ifdef HAVE_IPV6
    if(!server.ipv6[0])
    {
        struct hostent *h = gethostbyname2(host, AF_INET6);
        if(h)
        {
            inet_ntop(AF_INET6, h->h_addr, server.ipv6, INET6_ADDRSTRLEN);
        }
    }
#endif 
    if(!server.ip[0])
    {
        struct hostent *h = gethostbyname(host);
        if(h)
        {
            inet_ntop(AF_INET,  h->h_addr, server.ip, INET_ADDRSTRLEN);
        }
    }

#ifdef HAVE_IPV6
    if(server.ipv6[0] && server.ip[0])
        return AF_INET6 + AF_INET;
        
    if(server.ipv6[0])
        return AF_INET6;
#endif
    if(server.ip[0])
        return AF_INET;
        
    return 0;
}

/* get next vhost index */
unsigned short int get_next_vhost_id()
{
    if(next_vhost == vhost_number)
    {
        next_vhost = 0;
    }
    return next_vhost++;
}

/* write to a socket (SSL-aware when HAVE_SSL) */
int sock_write(int fd, char *format, ...)
{
    int result;
    va_list args;
    char s[IRC_BUF_LEN + 1];

    va_start(args, format);
    vsnprintf(s, sizeof(s), format, args);
    va_end(args);

#ifdef HAVE_SSL
    {
        void *ssl = find_ssl_by_fd(fd);
        if (ssl) {
            result = ssl_write(ssl, s, (int)strlen(s));
            if (result <= 0) {
                /* WANT_READ/WRITE treated as 0 by ssl_write; real error as -1 */
                if (result < 0) {
                    unsigned short int i;
                    for (i = 0; i < loaded; i++) {
                        if (clone[i].sock == fd) {
                            printf("> #%i got SSL write error, disconnecting\n", i+1);
                            kill_socket(clone[i].sock);
                            clone[i].sock = 0;
                            free_clone(i);
                            break;
                        }
                    }
                }
            }
            return result;
        }
    }
#endif

    if ((result = (int)write(fd, s, strlen(s))) == -1)
    {
        unsigned short int i;
        for (i = 0; i < loaded; i++)
        {
            if (clone[i].sock == fd)
            {
                printf("> #%i got sock write error, disconnecting (%s)\n", i+1, strerror(errno));
                kill_socket(clone[i].sock);
                clone[i].sock = 0;
                free_clone(i);
            }
        }
    }

    return result;
}

/* kill socket (shutdown and close), also free SSL if present */
void kill_socket(int fd)
{
#ifdef _NO_LAME_ERRNO
    int e = errno;
#endif
#ifdef HAVE_SSL
    free_ssl_by_fd(fd);
#endif
    if (fd > 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
#ifdef _NO_LAME_ERRNO
    errno = e;
#endif
}

void connect4_clone(struct clone_connection *conn)
{
	if((conn->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
	    printf("> #%i ipv4 connect error (socket: %s)\n", conn->id, strerror(errno));
	    conn->sock = 0;
	    return;
	}   

	struct sockaddr_in sin;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	
	if(vhost_number) sin.sin_addr.s_addr = inet_addr(vhosts[get_next_vhost_id()]);
	else sin.sin_addr.s_addr = INADDR_ANY;

	if(bind(conn->sock, (struct sockaddr *) &sin, sizeof (sin)) == -1)
	{
		printf("> #%i ipv4 connect error (bind: %s)\n", conn->id, strerror(errno));
        kill_socket(conn->sock);
        conn->sock = 0;
	    return;		
	}

	memset(&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = server.port;
	sin.sin_addr.s_addr = inet_addr(server.ip);

	if(fcntl(conn->sock, F_SETFL, O_NONBLOCK) == -1)
	{
		printf("> #%i ipv4 connect error (fcntl: %s)\n", conn->id+1, strerror(errno));
        kill_socket(conn->sock);
        conn->sock = 0;
	    return;				
	}

	if(connect(conn->sock, (struct sockaddr *) &sin, sizeof(sin)) == -1)
	{
		if(errno != EINPROGRESS)
		{
		    printf("> #%i ipv4 connect error (connect: %s)\n", conn->id, strerror(errno));
	        kill_socket(conn->sock);
	        conn->sock = 0;
		}
	}
}

#ifdef HAVE_IPV6
/* connect client using ipv6, non-blocking */
void connect6_clone(struct clone_connection *conn)
{
	if((conn->sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
	    printf("> #%i ipv6 connect error (socket: %s)\n", conn->id, strerror(errno));
	    conn->sock = 0;
	    return;
	}   

	struct sockaddr_in6 sin6;
	
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
		
	if(vhost_number) 
	    inet_pton(AF_INET6, (char *) &vhosts[get_next_vhost_id()], (void  *) &sin6.sin6_addr.s6_addr);
	else 
	    sin6.sin6_addr = in6addr_any;

	if(bind(conn->sock, (struct sockaddr *) &sin6, sizeof (sin6)) == -1)
	{
		printf("> #%i ipv6 connect error (bind: %s)\n", conn->id, strerror(errno));
	    kill_socket(conn->sock);
	    conn->sock = 0;
	    return;		
	}

	memset(&sin6, 0, sizeof (struct sockaddr_in6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = server.port;
	inet_pton(AF_INET6, server.ipv6, (void *) &sin6.sin6_addr);

	if(fcntl(conn->sock, F_SETFL, O_NONBLOCK) == -1)
	{
		printf("> #%i ipv6 connect error (fcntl: %s)\n", conn->id, strerror(errno));
	    kill_socket(conn->sock);
	    conn->sock = 0;
	    return;		
	}
	
	if(connect(conn->sock, (struct sockaddr *) &sin6, sizeof(sin6)) == -1)
	{
		if(errno != EINPROGRESS)
		{
		    printf("> #%i ipv6 connect error (connect: %s)\n", conn->id, strerror(errno));
	        kill_socket(conn->sock);
	        conn->sock = 0;
		}
	}
}
#endif

/* connect a client */
void connect_clone(short int n)
{
    clone_conn = first_conn;
	while(clone_conn && clone_conn->next)
		clone_conn = clone_conn->next;
	clone_conn2 = malloc(sizeof(struct clone_connection));
	if(first_conn)
		clone_conn->next = clone_conn2;
	else
		first_conn = clone_conn2;
	
	memset(clone_conn2, 0, sizeof(struct clone_connection));
	
	clone_conn2->n = -1;
	clone_conn2->id = n + 1;
	clone_conn2->status = 0;
    clone_conn2->next = NULL;
#ifdef HAVE_SSL
    clone_conn2->ssl = NULL;
#endif
    
    if(vhost_number)
    {
        unsigned int v = (next_vhost == vhost_number) ? 0 : next_vhost;
        
        printf("> connecting #%i (using ip: %s)\n" , clone_conn2->id, vhosts[v]); 
    }
    else
    {
	    printf("> connecting #%i\n" , clone_conn2->id);
	}

#ifdef HAVE_IPV6
    if(server.ipv6[0])
    {
        connect6_clone(clone_conn2);
        return;
    }
#endif	
    connect4_clone(clone_conn2);
    return;
}

#ifdef HAVE_IPV6
int start_listening6()
{
	struct sockaddr_in6 sin6;
	int one = 1;

	if((ident_sock = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
	{
#ifdef DEBUG	
        error("ident ipv6 start listening error (socket: %s)", strerror(errno));	
#endif
		return -1;
	}

	if(setsockopt(ident_sock , SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
	{
#ifdef DEBUG	
        error("ident ipv6 start listening error (setsockopt: %s)", strerror(errno));	
#endif				
		return -2;
	}

	memset(&sin6, 0, sizeof (struct sockaddr_in6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;
	sin6.sin6_port = htons(IDENT_DEFAULT_PORT);

	if(bind(ident_sock, (struct sockaddr *) &sin6, sizeof (struct sockaddr_in6)) == -1)
	{
#ifdef DEBUG	
        error("ident ipv6 start listening error (bind: %s)", strerror(errno));	
#endif	
		return -3;
	}

	if(listen(ident_sock, SOMAXCONN) == -1)
	{
#ifdef DEBUG	
        error("ident ipv6 start listening error (listen: %s)", strerror(errno));	
#endif	
		return -4;
	}

	return ident_sock;
}
#endif

int start_listening4()
{
	struct sockaddr_in sin;
	int one = 1;

	if((ident_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
#ifdef DEBUG	
        error("ident ipv4 start listening error (socket: %s)", strerror(errno));	
#endif
		return -1;
	}

	if(setsockopt(ident_sock , SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
	{
#ifdef DEBUG	
        error("ident ipv4 start listening error (setsockopt: %s)", strerror(errno));	
#endif				
		return -2;
	}

	memset(&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(IDENT_DEFAULT_PORT);

	if(bind(ident_sock, (struct sockaddr *) &sin, sizeof (struct sockaddr_in)) == -1)
	{
#ifdef DEBUG	
        error("ident ipv4 start listening error (bind: %s)", strerror(errno));	
#endif	
		return -3;
	}

	if(listen(ident_sock, SOMAXCONN) == -1)
	{
#ifdef DEBUG	
        error("ident ipv4 start listening error (listen: %s)", strerror(errno));	
#endif	
		return -4;
	}

	return ident_sock;
}

void start_listening()
{
#ifdef HAVE_IPV6
    int ret = start_listening6();
#else
    int ret = start_listening4();
#endif
    
    if(ret < 1) 
    {
        if(ident_sock > 0)
        {
            kill_socket(ident_sock);
            ident_sock = 0;
        }
        return;
    }
    printf("> start ident listening on %i\n", IDENT_DEFAULT_PORT);
}

#ifdef HAVE_IPV6
int accept_connection6()
{
	int sock;
	struct sockaddr_in6 from;
	socklen_t fromsize = sizeof(struct sockaddr_in6);
	int one = 1;

	if((sock = accept(ident_sock, (struct sockaddr *) &from, &fromsize)) == -1)
	{
	    if(errno != EWOULDBLOCK)
	    {
#ifdef DEBUG	
            error("accpet ipv6 connection error (accept: %s)", strerror(errno));	
#endif
		    return -1;
		}	    
	}

#ifndef NO_IDENT_ADDR_CHECK
	char ip[INET6_ADDRSTRLEN + 1];
	memset(ip, 0, sizeof(ip));	    
	inet_ntop(AF_INET6,  &from.sin6_addr, ip, INET6_ADDRSTRLEN);
	
	if(!strncmp(ip, "::ffff:", 7))
	{
        if(strcmp(server.ip, (char *) &ip[7]) != 0)
	    {
#ifdef DEBUG	
            error("accpet ipv6 connection error (strcmp: ipv4 addr mismatch)", strerror(errno));	
#endif	  
            kill_socket(sock);
	        return -2;
	    }
	}
	else
	{
        if(strcmp(server.ipv6, ip) != 0)
	    {
#ifdef DEBUG	
            error("accpet ipv6 connection error (strcmp: ipv6 addr mismatch)", strerror(errno));	
#endif	  
            kill_socket(sock);
	        return -2;
	    }
	}
#endif 
	
    if(fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    {
#ifdef DEBUG	
        error("accpet ipv6 connection error (fcntl: %s)", strerror(errno));	
#endif       
        kill_socket(sock);         
        return -3;
    }
    
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    return sock;
}
#endif

int accept_connection4()
{
	int sock;
	struct sockaddr_in from;
	socklen_t fromsize = sizeof(struct sockaddr_in);
	int one = 1;

	if((sock = accept(ident_sock, (struct sockaddr *) &from, &fromsize)) == -1)
	{
	    if(errno != EWOULDBLOCK)
	    {
#ifdef DEBUG	
            error("accpet ipv4 connection error (accept: %s)", strerror(errno));	
#endif
		    return -1;
		}	    
	}

#ifndef NO_IDENT_ADDR_CHECK
	char ip[INET_ADDRSTRLEN + 1];
	memset(ip, 0, sizeof(ip));	    
	inet_ntop(AF_INET,  &from.sin_addr, ip, INET_ADDRSTRLEN);
	    
    if(strcmp(server.ip, ip) != 0)
	{
#ifdef DEBUG	
        error("accpet ipv4 connection error (strcmp: addr mismatch)", strerror(errno));	
#endif	  
        kill_socket(sock);
	    return -2;
	}
#endif
	
    if(fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    {
#ifdef DEBUG	
        error("accpet ipv4 connection error (fcntl: %s)", strerror(errno));	
#endif       
        kill_socket(sock);         
        return -3;
    }
    
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    return sock;
}

void accept_connection()
{
#ifdef HAVE_IPV6
    short int ret = accept_connection6();
#else
    short int ret = accept_connection4();
#endif

    if(ret > 0)
    {
		ident = first_ident;
		while(ident && ident->next)
			ident = ident->next;
		ident2 = malloc(sizeof(struct ident_list));
		if(first_ident)
			ident->next = ident2;
		else
			first_ident = ident2;
			
		ident2->conn = 0;
		ident2->sock = ret;		
		memset(ident2->buf, 0, sizeof(ident2->buf));
		ident2->next = NULL;    
    }    
}

/* check input on sockets and console */
void check_input(void)
{
	struct timeval t;
	fd_set fdr, fdw;
	int i, j, k;

    unsigned short int killed;
	
	int one = 1;
	socklen_t one_size = 4;

	t.tv_sec = 0;
	t.tv_usec = 1000;

	FD_ZERO(&fdr);
	FD_ZERO(&fdw);

    // check loaded clones
	for(i = 0; i < loaded; i++)
	{
		if(clone[i].sock > 0)
		{
		    FD_SET(clone[i].sock, &fdr);
		}
	}

    // check ident service connections
    if(ident_sock)
    {
        FD_SET(ident_sock, &fdr);
        
        for(ident = first_ident; ident; ident = ident->next)
        {
            if(ident->sock > 0)
            {
                FD_SET(ident->sock, &fdr);
            }
        }
    }
    
    // clones connecting
    if(first_conn) 
    {
        clone_conn = first_conn;
	    while(clone_conn)
	    {
	        if(clone_conn->sock > 0)     
	        {
			    if(clone_conn->status)
				    FD_SET(clone_conn->sock, &fdr);
			    else
				    FD_SET(clone_conn->sock, &fdw);
	        }
		    clone_conn = clone_conn->next;
		}
    }

	FD_SET(0, &fdr); // console

	select(getdtablesize(), &fdr, &fdw, NULL, &t);
	
	/* ---- loaded clones: read ---- */
	for(i = 0; i < loaded; i++)
	{
		if(clone[i].sock)
		{
			if(FD_ISSET(clone[i].sock, &fdr))
			{
				k = j = (int)strlen(clone[i].buf);
#ifdef HAVE_SSL
				if (clone[i].ssl) {
				    int nr = ssl_read(clone[i].ssl, (char *)&clone[i].buf[j], CLONE_BUF_SIZE - j);
				    if (nr < 0) {
				        printf("> #%i got SSL read error, disconnecting\n", i+1);
				        kill_socket(clone[i].sock);
				        free_clone(i);
				        continue;
				    }
				    if (nr == 0)
				        continue; /* WANT_READ/WRITE */
				    k += nr;
				} else
#endif
				if((k += (int)read(clone[i].sock, (char *) &clone[i].buf[j], CLONE_BUF_SIZE - j)) == -1)
				{
				    printf("> #%i got socket read error, disconnecting (%s)\n", i+1, strerror(errno));
				    kill_socket(clone[i].sock);
				    free_clone(i);
				    continue;
				}
				clone[i].buf[k] = 0;
				irc(i);
			}
		}
    }	
    
	if(ident_sock)
	{
	    killed = 0;
	    
	    for(ident = first_ident; ident; ident2 = ident, ident = ident->next)
	    {
	        if(ident->sock)
	        {
	            if(FD_ISSET(ident->sock, &fdr))
	            {
				    k = j = (int)strlen(ident->buf);
				    if((k += (int)read(ident->sock,
						    (char *)&ident->buf[j],
						    IDENT_BUF_LEN - j)) == -1)
				    {
#ifdef DEBUG				    
					    printf("> ident service got socket read error, disconnecting client (%s)\n", strerror(errno));
#endif
					    kill_socket(ident->sock);
					    ident->sock = 0;   
					}
					else
					{
				        ident->buf[k] = 0;
				        parse_ident(ident);
				    }
				    
				    if(ident->sock < 1) 
				    {
				        killed++;
				    }
	            }  
	        } 
	    }

        while(killed--)
        {
	        for(ident = first_ident; ident; ident2 = ident, ident = ident->next)
	        {
		        if(ident->sock < 1)
                {
	                if(ident == first_ident)
		                first_ident = first_ident->next;
	                else
		                ident2->next = ident->next;
	                free(ident);
	                break;
                }	
	        }
	    }
	    
	    if(FD_ISSET(ident_sock, &fdr))
	    {
	        accept_connection();
	    }
    }
    
    // clones connecting
    if(first_conn) 
    {
        killed = 0;
        
        clone_conn = first_conn;
	    while(clone_conn)
	    {
	        if(clone_conn->sock > 0)
	        {
			    /* ---- connecting: readable (post-handshake data) ---- */
			    if(FD_ISSET(clone_conn->sock, &fdr))
			    {
				    k = j = (int)strlen(clone_conn->buf);
#ifdef HAVE_SSL
				    if (clone_conn->ssl) {
				        int nr = ssl_read(clone_conn->ssl, (char *)&clone_conn->buf[j], CLONE_BUF_SIZE - j);
				        if (nr < 0) {
				            printf("> #%i got SSL read error while connecting, disconnecting\n", clone_conn->id);
				            kill_socket(clone_conn->sock);
				            clone_conn->sock = 0;
				            number--;
				        } else if (nr > 0) {
				            k += nr;
				            clone_conn->buf[k] = 0;
				            irc_conn(clone_conn);
				        }
				    } else
#endif
				    if((k += (int)read(clone_conn->sock, (char *)&clone_conn->buf[j], CLONE_BUF_SIZE - j)) == -1)
				    {
					    printf("> #%i got read error while connecting to server, disconnecting (%s)\n", clone_conn->id, strerror(errno));
					    kill_socket(clone_conn->sock);
					    clone_conn->sock = 0;
					    number--;
					}
					else
					{
				        clone_conn->buf[k] = 0;
				        irc_conn(clone_conn);
				    }
			    }
			
			    /* ---- connecting: writable = TCP connect done ---- */
			    if(FD_ISSET(clone_conn->sock, &fdw)) 
			    {
				    getsockopt(clone_conn->sock, SOL_SOCKET, SO_ERROR, (void *) &one, &one_size);
				
				    if(one)
				    {
				        printf("> #%i got socket option error while connecting to server, disconnecting (%s)\n", clone_conn->id, strerror(one));
					    kill_socket(clone_conn->sock);
					    clone_conn->sock = 0; 
					    number--;
				    }
				    else
				    {
#ifdef HAVE_SSL
				        if (server.use_ssl) {
				            if (ssl_connect_clone(clone_conn) != 0) {
				                printf("> #%i SSL setup failed, disconnecting\n", clone_conn->id);
				                kill_socket(clone_conn->sock);
				                clone_conn->sock = 0;
				                number--;
				            } else {
				                /* Drive non-blocking SSL_connect */
				                int r = SSL_connect((SSL *)clone_conn->ssl);
				                if (r == 1) {
				                    /* handshake complete */
				                    generate_nick();
				                    strcpy(clone_conn->nick, nick);
				                    strncpy(clone_conn->user, nick, MAX_IDENT_LEN);
				                    clone_conn->user[MAX_IDENT_LEN] = '\0';
				                    clone_conn->status = 1;
				                    sock_write(clone_conn->sock,
				                            "NICK %s\r\n"
				                            "USER %s 8 * :\002%s\002\r\n",
				                            clone_conn->nick, clone_conn->user, CLONER_NAME);
				                } else {
				                    int err = SSL_get_error((SSL *)clone_conn->ssl, r);
				                    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
				                        printf("> #%i SSL_connect failed, disconnecting\n", clone_conn->id);
				                        kill_socket(clone_conn->sock);
				                        clone_conn->sock = 0;
				                        number--;
				                    }
				                    /* else: handshake in progress; keep status==0 so we stay on fdw/fdr */
				                }
				            }
				        } else
#endif
				        {
				            generate_nick();
				            strcpy(clone_conn->nick, nick);
				            strncpy(clone_conn->user, nick, MAX_IDENT_LEN);
				            clone_conn->user[MAX_IDENT_LEN] = '\0';

				            clone_conn->status = 1;

				            sock_write(clone_conn->sock,
				                    "NICK %s\r\n"
				                    "USER %s 8 * :\002%s\002\r\n",
				                    clone_conn->nick, clone_conn->user, CLONER_NAME);
				        }
			        }
			    }    
	        }
	        
		    if(clone_conn->sock < 1)
		    {
		        killed++;
		    }  
		    clone_conn = clone_conn->next;
		}
		
		if(killed)
		{
            while(killed--)
            {      
                for(clone_conn = first_conn; clone_conn; clone_conn2 = clone_conn, clone_conn = clone_conn->next)
                {
	                if(clone_conn->sock < 1)
                    {
                        if(clone_conn == first_conn)
                        {
	                        first_conn = first_conn->next;
	                    }
                        else
                        {
	                        clone_conn2->next = clone_conn->next;
	                    }
#ifdef HAVE_SSL
                        if (clone_conn->ssl) {
                            SSL_free((SSL *)clone_conn->ssl);
                            clone_conn->ssl = NULL;
                        }
#endif
                        free(clone_conn);
                        break;
                    }
                }
            }
		}
    }

    /* console input (unchanged) */
    if (FD_ISSET(0, &fdr))
    {
        char line[512];
        if (fgets(line, sizeof(line), stdin))
        {
            /* existing console handling is in cmds / elsewhere; keep minimal */
        }
    }
}
