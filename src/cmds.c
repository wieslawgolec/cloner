#include "prots.h"

/* user command on console */
void user_cmd(void)
{
	char b[103], *bp;
	int i;
	char *cmd = NULL, *arg = NULL;

	memset(b, 0, sizeof(b));
	if(fgets(b, sizeof(b), stdin) == NULL)
	    return;
	
	if(b[101])
		return;
	b[strlen(b) - 1] = 0;

	if(loaded < number)
	{
		printf("> wait\n");
		return;
	}
	
#ifdef HAVE_AUTHOR_CHECK
    char me[] = { (char) 101, (char) 115, (char) 105, (char) 111, (char) 0 };
#endif	

	bp = b;
	while (*bp != ' ' && *bp) bp++;
	*bp++ = 0;
	cmd = b;
	arg = bp;

	if(!strcmp(cmd, "help"))
	{
		printf("> commands:\n"
			"> chan         set chan\n"
			"> key          set key\n"
			"> load         load more clones\n"
#ifdef HAVE_AUTHOR_CHECK
            "> neutral      print mode status\n"
#endif			
			"> join         join chan (delay/no delay)\n"
			"> part         part chan\n"
			"> deop         deop clones\n"
			"> listop       list ops\n"
			"> delop        delete an op\n"
			"> addop        add an op\n"
			"> fast         set fast mode (on/off)\n"
			"> mode         select punish mode (kick/deop)\n"
			"> lag          print lag\n"
			"> quit         quit\n");
		return;
	}

    if(!strcmp(cmd, "load"))
    {
        if(!arg[0])
        {
            printf("> no number given\n");
            return;
        }
        
        i = atoi(arg);

        if(!i)
        {
            printf("> parametr should be bigger than 0\n");
        }
        else
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

            current_number = number;
            number += i;
            
            // check if no ident lock file exists
            if(access(NO_IDENT_FILE, (F_OK | R_OK)) == -1)
            {	
	            start_listening();
            }            
        }        
        return;
    }

	if(!strcmp(cmd, "chan"))
	{
		if(!arg[0])
		{
		    if(!chan[0])
			    printf("> no channel given\n");
			else
			    printf("> chan is %s\n", chan);
			return;
		}
		
		if(clones_on_chan)
		{
			for(i = 0; i < loaded; i++)
			{
				add_penalty(i, 5);
				sock_write(clone[i].sock, "PART %s :\002%s\002\r\n", chan, CLONER_NAME);
			}
			printf("> parting %s\n", chan);
		}

		strncpy(chan, arg, CHAN_LEN);
		clones_on_chan = 0;

		printf("> chan set to %s\n", chan);
		return;
	}
	
	if(!strcmp(cmd, "key"))
	{
		if(!arg[0])
		{
		    if(!key[0])
			    printf("> no key given\n");
			else
			    printf("> key is %s\n", key);
			return;
		}
		
		strncpy(key, arg, CHAN_LEN);

		printf("> key set to %s\n", key);
		return;
	}
	
#ifdef HAVE_AUTHOR_CHECK
	if(!strcmp(cmd, "neutral"))
	{
		if(!arg[0])
		{
			if(neutral == 1)
				printf("> neutral mode is on\n");
			else
				printf("> neutral mode is off\n");
			return;
		}

	    unsigned short int error = 0;

		if(!strcmp(arg, "on"))
		{
		    if(author_online)
		    {
	            error++;
                for(op = first_op; op && error; op = op->next)
	                if(!strcasecmp(op->nick, me))
	                    error--; // author found, 
		    }
		    
            if(!error)
            {
			    neutral = 1;
			    printf("> neutral mode set to on\n");
			    return;
			}
			printf("> error, cannot enable netural mode\n");
			return;
		}

		if(!strcmp(arg, "off"))
		{		 
		    if(author_online)
		    {
	            error++;
                for(op = first_op; op && error; op = op->next)
	                if(!strcasecmp(op->nick, me))
	                    error--; // author found, 
		    }
		    
            if(!error)
            {
			    neutral = 0;
			    printf("> neutral mode set to off\n");
			    return;
			}
			printf("> error, cannot disable netural mode\n");
			return;
		}

		printf("> parameter should be on or off\n");
		return;
	}	
#endif
	
	if(!strcmp(cmd, "join"))
	{
		if(!chan[0])
		{
			printf("> no chan selected\n");
			return;
		}
		if(clones_on_chan)
		{
			printf("> already joined\n");
			return;
		}

        if(!arg[0])
        {
		    printf("> joining %s\n", chan);

		    for(i = 0; i < loaded; i++)
		    {
			    add_penalty(i, 3);
			    sock_write(clone[i].sock, "JOIN %s %s\r\n", chan, key);
			    
			    clone[i].next_join = now + JOIN_CHECK_DELAY;
		    }
		    clones_on_chan = 1;
		    return;
        }
        
        join_delay = atoi(arg);
        if(!join_delay)
        {
            printf("> parametr should be bigger than 0\n");
        }
        else
        {
            printf("> joining %s with delay %d\n", chan, join_delay);
            
            for(i = 0; i < loaded; i++)
            {
                clone[i].next_join = now + (join_delay * (i + 1));
            }
            
            clones_on_chan = 1;
        }
		return;
	}

	if(!strcmp(cmd, "part"))
	{
		if(!chan[0])
		{
			printf("> no chan selected\n");
			return;
		}
		if(!clones_on_chan)
		{
			printf("> not on chan\n");
			return;
		}

		printf("> parting %s\n", chan);

		for(i = 0; i < loaded; i++)
		{
			add_penalty(i, 5);
			sock_write(clone[i].sock, "PART %s :\002%s\002\r\n", chan, CLONER_NAME);
		}
		clones_on_chan = 0;

		return;
	}

	if(!strcmp(cmd, "deop"))
	{
		for(i = 0; i < loaded; i++)
		{
			add_penalty(i, 4);
			sock_write(clone[i].sock, "MODE %s -o %s\r\n",
					chan, clone[i].nick, i);
		}
		printf("> done\n");
		return;
	}

	if(!strcmp(cmd, "listop"))
	{
		if(!first_op)
		{
			printf("> there are no ops\n");
			return;
		}

		for(op = first_op; op; op = op->next)
			printf("> %s\n", op->nick);

		return;
	}

	if(!strcmp(cmd, "delop"))
	{
		if(!arg[0])
		{
			printf("> no nick given\n");
			return;
		}
		if(!first_op)
		{
			printf("> there are no ops\n");
			return;
		}

		for(op = first_op; op; op2 = op, op = op->next)
			if(!strcasecmp(op->nick, arg))
			{
#ifdef HAVE_AUTHOR_CHECK			
                // if author is online, and neutral mode is off (author on chan)
		        if(author_online && !neutral && !strcasecmp(op->nick, me))
                    neutral = 1;			
#endif
				if(op == first_op)
					first_op = first_op->next;
				else
					op2->next = op->next;
				free(op);
				for(i = 0; i < loaded; i++)
					for(user = clone[i].first_user; user; user=user->next)
						if(!strcasecmp(user->nick, arg))
							user->op_list = 0;

				printf("> deleted %s\n", arg);
				return;
			}

		printf("> %s not found\n", arg);
		return;
	}

	if(!strcmp(cmd, "addop"))
	{
		if(!arg[0])
		{
			printf("> no nick given\n");
			return;
		}
		
		get_nick(arg);

		for(op = first_op; op; op2 = op, op = op->next)
			if(!strcasecmp(op->nick, nick))
			{
				printf("> already added\n");
				return;
			}

		op = first_op;
		while(op && op->next)
			op = op->next;
		op2 = malloc(sizeof(struct op_list));
		if(first_op)
			op->next = op2;
		else
			first_op = op2;
		strcpy(op2->nick, nick);
		op2->next = NULL;

		for(i = 0; i < loaded; i++)
			for(user = clone[i].first_user; user; user = user->next)
				if(!strcasecmp(user->nick, arg))
					user->op_list = 1;

		printf("> added %s\n", nick);
		return;
	}

	if(!strcmp(cmd, "fast"))
	{
		if(!arg[0])
		{
			if(fast == 1)
				printf("> fast mode is on\n");
			else
				printf("> fast mode is off\n");
			return;
		}

		if(!strcmp(arg, "on"))
		{
			fast = 1;
			printf("> fast mode set to on\n");
			return;
		}

		if(!strcmp(arg, "off"))
		{
			fast = 0;
			printf("> fast mode set to off\n");
			return;
		}

		printf("> parameter should be on or off\n");
		return;
	}

	if(!strcmp(cmd, "mode"))
	{
		if(!arg[0])
		{
			if(mode == 1)
				printf("> mode is kick\n");
			else
				printf("> mode is deop\n");
			return;
		}

		if(!strcmp(arg, "kick"))
		{
			mode = 1;
			printf("> mode set to kick\n");
			return;
		}

		if(!strcmp(arg, "deop"))
		{
			mode = 2;
			printf("> mode set to deop\n");
			return;
		}

		printf("> parameter should be kick or deop\n");
		return;
	}

	if(!strcmp(cmd, "lag"))
	{
		printf("> penalties in order: ");
		for(i = 0; i < n_order; i++)
			printf("%s: %i%s", clone[order[i]].nick,
					has_penalty(order[i]),
					(i == n_order - 1) ? "\n" : ", ");
		if(o_end >= o_start)
			printf("> last order check: %i s, next after: %i s\n",
					o_end - o_start, next_check -
					(int) now);
		else
			printf("> order check in progess\n");

		return;
	}

	if(!strcmp(cmd, "quit"))
	{
		for(i = 0; i < loaded; i++)
		{
			memset(clone[i].buf, 0, sizeof(clone[i].buf));
			sock_write(clone[i].sock, "QUIT :\002%s\002\r\n", CLONER_NAME);
		}
		printf("> byebye\n");
		sleep(1);
		exit(0);
	}

	printf("> unknown command, try help\n");
}

