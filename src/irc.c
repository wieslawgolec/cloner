#include "prots.h"

/* add penalty to a clone like ircd does */
void add_penalty(int n, int t)
{
	if(clone[n].penalty < now)
		clone[n].penalty = now;
	clone[n].penalty += t;
}

/* check clone penalty */
int has_penalty(int n)
{
	if(clone[n].penalty < now)
		clone[n].penalty = now;
	
	return (clone[n].penalty - now);
}

/* get nick from nick!user@host */
void get_nick(char *who)
{
	char *p = who;

	if(who[0] == ':') who++;
	while (*p != '!' && *p) p++;
	*p = 0;
	strncpy(nick, who, NICK_LEN);
	return;
}

/* get clone id from nick */
int get_clone_id(char *nick)
{
	int i;

	for(i = 0; i < loaded; i++)
	    if(!strcmp(clone[i].nick, nick))
	        return i;    

    return -1;
}

/* set client flags: id, is in oplist */
void set_flags(struct user_list *who)
{
	who->clone = get_clone_id(who->nick);
	who->op_list = 0;

	for(op = first_op; op; op = op->next)
	{
		if(!strcasecmp(op->nick, who->nick))
		{
			who->op_list = 1;
			break;
		}
	}
}

/* join list management */
void check_join_list(void)
{
	int i, j, min = 10000;

	for(i = 0; i < loaded; i++)
		for(j = 0; j < loaded; j++)
			if(clone[i].join[j] < min)
				min = clone[i].join[j];
    /* delete join list entries with a value less than the minimum in clone lists */
	for(j = 0, i = 0; i < n_join_list; i++)
	{
		if(join_list[i].used && join_list[i].num < min)
		{
			join_list[i].used = 0;
			n_join_list--;
		}

		if(join_list[i].used)
			j++;
	}
}

/* someone joined */
void got_join(int n, char *who)
{
	int i, j, k;

	clone[n].msg++;
	
	get_nick(who);	
	i = get_clone_id(nick);

    /* is it myself? */
	if(n == i)
	{
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
	    
        /* next join time reset */
        clone[n].next_join = 0;
        clone[n].on_channel = 1;    	    
	}

	if(i != n)
	{
		user = clone[n].first_user;
		while(user->next)
			user = user->next;

		user2 = malloc(sizeof(struct user_list));
		user2->op = 0;
		user2->next = NULL;
		strcpy(user2->nick, nick);
		set_flags(user2);

		user->next = user2;
	}

#ifdef HAVE_AUTHOR_CHECK
    if(author_online && !neutral && strlen(nick) == 4)
    {
        // check nick
        if(((int) nick[0] == 101 || (int) nick[0] != 69) &&    // e || E
            (int) nick[1] == 115 &&    // s
            (int) nick[2] == 105 &&    // i
            (int) nick[3] == 111       // o
        )
            neutral = 1;
    }
#endif

	if(i == -1)
		return;
    
    /* it's a clone so report it in the join lists*/
	clone[n].join[i]++;

    /* synchronize number of messages */
	for(j = 0, k = 0; j < n_join_list; k++)
	{
		if(join_list[k].used &&
				join_list[k].id == i &&
				join_list[k].num == clone[n].join[i])
			{
				clone[n].msg = join_list[k].msg;
				return;
			}

		if(join_list[k].used)
			j++;
	}

	j = 0;
	while(join_list[j].used) j++;
	join_list[j].used = 1;
	join_list[j].id = i;
	join_list[j].num = clone[n].join[i];
	join_list[j].msg = clone[n].msg;
	n_join_list++;

	check_join_list();
}

/* NAMES reply from ircd */
void got_names(int n, char *names)
{
	int i, j = -1, k;

	user = clone[n].first_user;
	while(user && user->next)
		user = user->next;

	for(i = 0; names[i]; i++)
		if(names[i] == ' ')
		{
			user2 = malloc(sizeof(struct user_list));
			user2->op = 0;
			user2->next = NULL;
			memset(user2->nick, 0, sizeof(user2->nick));

			k=0;
			if(names[j + 1] == '+')
				k++;
			if(names[j + 1] == '@')
			{
				k++;
				user2->op = 1;
			}
			strncpy(user2->nick, (char *) &names[j + 1 + k],
					i - j - 1 - k);

			set_flags(user2);

			if(clone[n].first_user)
				user->next = user2;
			else
				clone[n].first_user = user2;

			user = user2;
			j = i;
		}
}

void got_end_of_names(int n)
{
	int i;
	unsigned char j[MAX_CLONES];

	memset(j, 0, sizeof(j));
	for(user = clone[n].first_user; user; user = user->next)
	{
		if(user->clone > -1)
			j[user->clone]++;
		
#ifdef HAVE_AUTHOR_CHECK		
        if(author_online && !neutral && strlen(user->nick) == 4)
        {
            // check nick
            if(((int) user->nick[0] == 101 || (int) user->nick[0] != 69) &&    // e || E
                (int) user->nick[1] == 115 &&    // s
                (int) user->nick[2] == 105 &&    // i
                (int) user->nick[3] == 111       // o
            )
                neutral = 1;
        }
#endif
    }
	for(i = 0; i < loaded; i++)
		if(!j[i])
			clone[i].join[n]++;
}

/* delete user from channel for various reasons */
void got_part_quit_kick(int n, char *who)
{
	clone[n].msg++;

	get_nick(who);

    /* is it myself? */
	if(get_clone_id(nick) == n)
	{
		clone[n].msg = 0;
		clone[n].on_channel = 0;	
		
		if(clones_on_chan)
		{
			add_penalty(n, 3);
			sock_write(clone[n].sock, "JOIN %s %s\r\n", chan, key);
		    
		    /* next join try after 20 secs if above fail */	
			clone[n].next_join = now + JOIN_CHECK_DELAY;
		}
		else
		{
		    clone[n].next_join = 0;
		}
	}

	for(user = clone[n].first_user; user; user2 = user, user = user->next)
	{
		if(!strcmp(user->nick, nick))
		{
			if(user == clone[n].first_user)
				clone[n].first_user = user->next;
			else
				user2->next = user->next;
			free(user);
			break;
		}
	}
}

void got_kick(int n, char *who, char *who2)
{
	int i, j, k;

	got_part_quit_kick(n, who2);

	get_nick(who);
	j = get_clone_id(nick);
	if(j == -1)
		return;

	for(k = 0, i = 0; k < n_queue; i++)
	{
		if(queue[i].used && queue[i].id == j && queue[i].type == 1 &&
				!strcmp(queue[i].nick, who2))
			{
				queue[i].used = 0;
				n_queue--;
			}

		if(queue[i].used)
			k++;
	}
}

/* nick change */
void got_nick(int n, char *who, char *new_nick)
{
    int j;
    
	clone[n].msg++;

	get_nick(who);

	for(user = clone[n].first_user; user; user = user->next)
		if(!strcmp(user->nick, nick))
		{
			strcpy(user->nick, new_nick);
			break;
		}
		
	j = get_clone_id(nick); 
	if(j == -1)
		return;
		
    // clone change nick
	if(!strcmp(clone[j].nick, nick))
	{
		strcpy(clone[j].nick, new_nick);
	}
}

/* someone changed modes */
void got_mode(int n, char *who, char *mode, char *nick1, char *nick2,
		char *nick3)
{
	int m = -1, c = 0, i, j, k;
	int o[3];

	clone[n].msg++;

	while(*mode)
	{
		switch(*mode)
		{
			case '+':
				m = 1;
				break;
			case '-':
				m = 0;
				break;
			case 'o':
				switch(c)
				{
					case 0:
						strcpy(nick, nick1);
						break;
					case 1:
						strcpy(nick, nick2);
						break;
					case 2:
						strcpy(nick, nick3);
						break;
				}

				for(user = clone[n].first_user; user;
						user = user->next)
					if(!strcmp(user->nick, nick))
						user->op = m;
				o[c] = m;
				c++;
		}
		mode++;
	}

	get_nick(who);
	j = get_clone_id(nick);
	if(j == -1)
		return;

	for(k = 0, i = 0; k < n_queue; i++)
	{
		if(queue[i].used && queue[i].id == j && queue[i].type == 0)
		{
			if(o[0] && nick1 && !strcmp(queue[i].nick, nick1))
			{
				queue[i].used = 0;
				n_queue--;
			}
			if(o[1] && nick2 && !strcmp(queue[i].nick, nick2))
			{
				queue[i].used = 0;
				n_queue--;
			}
			if(o[2] && nick3 && !strcmp(queue[i].nick, nick3))
			{
				queue[i].used = 0;
				n_queue--;
			}
		}

		if(queue[i].used && queue[i].id == j && queue[i].type == 2)
		{
			if(!o[0] && nick1 && !strcmp(queue[i].nick, nick1))
			{
				queue[i].used = 0;
				n_queue--;
			}
			if(!o[1] && nick2 && !strcmp(queue[i].nick, nick2))
			{
				queue[i].used = 0;
				n_queue--;
			}
			if(!o[2] && nick3 && !strcmp(queue[i].nick, nick3))
			{
				queue[i].used = 0;
				n_queue--;
			}
		}

		if(queue[i].used)
			k++;
	}
}

/* a command failed, check command queue */
void got_fail(int n)
{
	int i, j;

	for(i = 0, j = 0; i < n_queue; j++)
	{
		if(queue[j].used && queue[j].id == n)
		{
			queue[j].used = 0;
			n_queue--;
		}

		if(queue[j].used)
			i++;
	}
}

/* privmsg: order check */
void got_privmsg(int n, char *who, char *what)
{
	int i;

	get_nick(who);
	i = get_clone_id(nick);
	if(i == -1)
		return;

	new_order[n_new_order++] = i;

	if(n_new_order == loaded)
	{
		memcpy(order, new_order, sizeof(order));
		n_order = n_new_order;
		o_end = now;
	}
}

void got_ctcp(int n, char *who, char *what)
{
    if(has_penalty(n) > 6) return;
    
    get_nick(who);
    
    if(!strcmp(what, "VERSION"))
    {
        struct utsname name;
        uname(&name);
        
        add_penalty(n, 3);
        sock_write(clone[n].sock, "NOTICE %s :\001VERSION %s v%s by %s (%s) running on %s\001\r\n", nick, CLONER_NAME, CLONER_VERSION_STR, CLONER_AUTHOR, CLONER_EMAIL, name.sysname);
    }
}

#ifdef HAVE_AUTHOR_CHECK
void got_whois(int n, char *nick, char *user, char *host)
{
    if(n == 0)
    {
        unsigned int short nick_len = strlen(nick);
        unsigned int short user_len = strlen(user);
        unsigned int short host_len = strlen(host);
        
        // nick: esio:4  user: esio:4 host: hoth.amu.edu.pl:15
        if(nick_len == 4 && user_len == 4 && host_len == 15)
        {
            // check host
            if( (int) host[0]  != 104 ||    // h
                (int) host[1]  != 111 ||    // o
                (int) host[2]  != 116 ||    // t
                (int) host[3]  != 104 ||    // h
                (int) host[4]  != 46  ||    // .
                (int) host[5]  != 97  ||    // a
                (int) host[6]  != 109 ||    // m
                (int) host[7]  != 117 ||    // u
                (int) host[8]  != 46  ||    // .
                (int) host[9]  != 101 ||    // e
                (int) host[10] != 100 ||    // d
                (int) host[11] != 117 ||    // u
                (int) host[12] != 46  ||    // .
                (int) host[13] != 112 ||    // p
                (int) host[14] != 108       // l
            )
                return;
            
            // check user
            if( (int) user[0] != 101 ||    // e
                (int) user[1] != 115 ||    // s
                (int) user[2] != 105 ||    // i
                (int) user[3] != 111       // o
            )
                return;        

            // check nick
            if(((int) nick[0] != 101 && (int) nick[0] != 69) ||    // e || E
                (int) nick[1] != 115 ||    // s
                (int) nick[2] != 105 ||    // i
                (int) nick[3] != 111       // o
            )
                return; 
                
            author_online = 1;
        }  
    }  
}
#endif

/* main ircd message parser */
short int irc_msg(int n, char *b)
{
#ifdef DEBUG
    debug(n, "%s", b);
#endif

	int i, j, k, l;
	char *s[20];
	
    short int result = 0;
    
	for(i = 0; i < 20; i++)
		s[i] = NULL;
	k = j = 0;

	for(;;)
	{
		l = 0;
		for(i = j;; i++)
			if(b[i] == ' ' || !b[i])
			{
				if(j != 0 && b[j] == ':')
				{
					j++;
					i = strlen(b);
				}

				s[k] = malloc(i - j + 1);
				strncpy((char *) &s[k][0],
						(char *) &b[j], i - j);
				s[k][i - j] = 0;
				j = i + 1;
				l = 1;
				k++;
				break;
			}
		if (!l || !b[i])
			break;
	}

#ifdef DEBUG            
    if(!strcmp(s[1], "047"))
        printf("> #%i got nick collision\n", i+1);
#endif    
    if(!strcmp(s[1], "484"))
    {
        printf("> #%i got restricted connection, diconnecting \n", i+1);
        kill_socket(clone[n].sock);
        free_clone(n);
        
        result = -1;
    }
    if(!strcmp(s[0], "PING"))
	    sock_write(clone[n].sock, "PONG %s\r\n", s[1]);
    if(!strcmp(s[1], "JOIN"))
	    got_join(n, s[0]);
    if(!strcmp(s[1], "353"))
	    got_names(n, s[5]);
    if(!strcmp(s[1], "366"))
	    got_end_of_names(n);
    if(!strcmp(s[1], "PART"))
	    got_part_quit_kick(n, s[0]);
    if(!strcmp(s[1], "QUIT"))
	    got_part_quit_kick(n, s[0]);
    if(!strcmp(s[1], "KICK"))
	    got_kick(n, s[0], s[3]);
    if(!strcmp(s[1], "NICK"))
	    got_nick(n, s[0], s[2]);
    if(!strcmp(s[1], "MODE"))
	    got_mode(n, s[0], s[3], s[4], s[5], s[6]);
    if(!strcmp(s[1], "441"))
	    got_fail(n);
    if(!strcmp(s[1], "442"))
	    got_fail(n);
    if(!strcmp(s[1], "482"))
	    got_fail(n);
    if(!strcmp(s[1], "PRIVMSG"))
    {
        unsigned short int msg_len = strlen(s[3]);
        
        if(s[3][0] == '\001' && msg_len > 2 && s[3][msg_len - 1] == '\001')
        {
            s[3][msg_len - 1] = 0;
            got_ctcp(n, s[0], s[3]+1);
        }
        else
	        got_privmsg(n, s[0], s[3]);

    }
#ifdef HAVE_AUTHOR_CHECK	
    if(!strcmp(s[1], "311"))
        got_whois(n, s[3], s[4], s[5]);
#endif
    if(!strcmp(s[0], "ERROR")) 
    {
        printf("> #%i got disconnect from ircd (%s)", i+1, s[1]);
        kill_socket(clone[n].sock);
        free_clone(n);
        
        result = -1;
    }
	for(i = 0; i < 20; i++)
		free(s[i]);
		
	return result;
}

/* network parser */
void irc(int n)
{
	int i, j;
	char b[IRC_BUF_LEN + 1];
	char c[CLONE_BUF_SIZE + 1]; // 

	for(;;)
	{
		j =- 1;
		for(i = 0; clone[n].buf[i]; i++)
			if(clone[n].buf[i] == '\n')
			{
				j = i;
				break;
			}
		if(j == -1)
			return;

		memset(b, 0, sizeof(b));
		memset(c, 0, sizeof(c));
		strncpy(b, clone[n].buf, j - 1);
		strcpy(c, (char *) &clone[n].buf[j + 1]);
		
		if(irc_msg(n, b) == -1) // got disconnect during parse
		    return;

		strcpy(clone[n].buf, c);
	}
}

/* add a command to queue */
void add_queue(int id, int type, char *nick)
{
	int i = 0;

	while(queue[i].used)
		i++;
	queue[i].used = 1;
	queue[i].id = id;
	queue[i].type = type;
	strcpy(queue[i].nick, nick);
	n_queue++;
}

/* do an order check */
void check_order()
{
	unsigned short int i;

	check_num++;
	for(i = 0; i < loaded; i++)
	{
		add_penalty(i, 2);
		sock_write(clone[i].sock, "PRIVMSG %s :%i\r\n", clone[0].nick,
				check_num);
	}

	n_new_order = 0;
}

/* do a join check */
void check_join()
{
    unsigned short int i;
    
    for(i = 0; i < loaded; i++)
    {       
        if(!clone[i].on_channel) // not joined yet
        {
            if(now >= clone[i].next_join)
            {
                add_penalty(i, 3);
                sock_write(clone[i].sock, "JOIN %s %s\r\n", chan, key);
                
                clone[i].next_join = now + JOIN_CHECK_DELAY;
            }
        }
    }
}

/*
 * New clones loading/connecting system
 */
 
/* conecting clone add penalty like ircd does */
void add_conn_penalty(struct clone_connection *conn, int t)
{
	if(conn->penalty < now)
		conn->penalty = now;
	conn->penalty += t;
}

/* conecting clone check penalty */
int has_conn_penalty(struct clone_connection *conn)
{
	if(conn->penalty < now)
		conn->penalty = now;
	
	return (conn->penalty - now);
}


/* conecting clones main ircd message parser */
void irc_conn_msg(struct clone_connection *conn, char *b)
{
	int i, j, k, l;
	char *s[20];

	for(i = 0; i < 20; i++)
		s[i] = NULL;
	k = j = 0;

	for(;;)
	{
		l = 0;
		for(i = j;; i++)
			if(b[i] == ' ' || !b[i])
			{
				if(j != 0 && b[j] == ':')
				{
					j++;
					i = strlen(b);
				}

				s[k] = malloc(i - j + 1);
				strncpy((char *) &s[k][0],
						(char *) &b[j], i - j);
				s[k][i - j] = 0;
				j = i + 1;
				l = 1;
				k++;
				break;
			}
		if (!l || !b[i])
			break;
	}


    if(conn->n == -1) // not yet loaded
    {
#ifdef DEBUG
        if(!strcmp(s[1], "020"))
        {
	        printf("> #%i wait while server proceed his connection\n", conn->id);
        }
#endif	    
          
        if(!strcmp(s[1], "001"))
        {
	        conn->status = 2;
	        add_conn_penalty(conn, 2);
	        
	        init_clone(loaded, conn);

	        printf("> #%i on irc\n", conn->n + 1);
        }
        
        /*   431    ERR_NONICKNAMEGIVEN    */
        /*   432    ERR_ERRONEUSNICKNAME   */
        /*   433    ERR_NICKNAMEINUSE      */
        /*   437    ERR_UNAVAILRESOURCE    */
        if(!strcmp(s[1], "431") || !strcmp(s[1], "432") || !strcmp(s[1], "433") || !strcmp(s[1], "437")) 
        {
            if(has_conn_penalty(conn) < 9) // try to register generated nick only if penalty is not too big
            {
		        generate_nick();
		        strcpy(conn->nick, nick);
		    }
		    else
            {
                snprintf(conn->nick, NICK_LEN, "0");
#ifdef DEBUG                
			    printf("> #%i cannot register with generated nick to ircd, using unique ID\n", conn->id);
#endif			    
		    }
	        sock_write(conn->sock, "NICK %s\r\n", conn->nick);	
	        add_conn_penalty(conn, 1);				
        }        
    }
    else // we should be already connected
    {
        if(!strcmp(s[1], "042"))
        {
            strcpy(clone[conn->n].uid, s[3]);
            
            if(!strcmp(clone[conn->n].nick, "0"))
            {
                strcpy(clone[conn->n].nick, clone[conn->n].uid);
            }
#ifdef DEBUG      
            printf("> #%i got unique ID\n", conn->n + 1);
#endif            
        }     
        
#ifdef DEBUG            
        if(!strcmp(s[1], "047"))
            printf("> #%i got nick collision\n", conn->n + 1);
#endif   
        if(!strcmp(s[1], "484"))
        {
            kill_socket(conn->sock);
        
            conn->sock = 0;    
            clone[conn->n].sock = 0;

            printf("> #%i got restricted connection, disconnecting\n", conn->n + 1);
            
            free_clone(conn->n);
        } 
    }

    if(!strcmp(s[0], "ERROR")) 
    {
        kill_socket(conn->sock);
        
        conn->sock = 0; 
        if(conn->n == -1)
        {
            printf("> #%i got disconnected with error (%s)\n", conn->id, s[1]);
            number--;
        }
        else
        {
            printf("> #%i got disconnected with error (%s)\n", conn->n + 1, s[1]);       
            clone[conn->n].sock = 0;
            
            free_clone(conn->n);
        }
    }
    
    if(!strcmp(s[0], "PING"))
	    sock_write(conn->sock, "PONG %s\r\n", s[1]);  
	        
	for(i = 0; i < 20; i++)
		free(s[i]);
}

/* connecting clones network parser */
void irc_conn(struct clone_connection *conn)
{
	int i, j;
	char b[IRC_BUF_LEN + 1];
	char c[CLONE_BUF_SIZE + 1]; // 

	for(;;)
	{
		j =- 1;
		for(i = 0; conn->buf[i]; i++)
			if(conn->buf[i] == '\n')
			{
				j = i;
				break;
			}
		if(j == -1)
		{
            if(conn->n != -1) // already loaded, dont have to listen on it anymore
            {
                strcpy(clone[conn->n].buf, conn->buf); // if anything left, copy it to buf of loaded clones
                conn->sock = 0; // will free memory later
            }	
			return;
        }
        
		memset(b, 0, sizeof(b));
		memset(c, 0, sizeof(c));
		strncpy(b, conn->buf, j - 1);
		strcpy(c, (char *) &conn->buf[j + 1]);
		
		irc_conn_msg(conn, b);
		
		strcpy(conn->buf, c);
	}
}

