#include "prots.h"

/* ident network parser */
void parse_ident(struct ident_list *current_ident)
{
	int i, j;
	char b[IDENT_BUF_LEN + 1];
	char c[IDENT_BUF_LEN + 1];

	for(;;)
	{
		j = -1;
		for(i = 0; current_ident->buf[i]; i++)
			if(current_ident->buf[i] == '\n')
			{
				j = i;
				break;
			}
		if(j == -1)
			return;
			
		memset(b, 0, sizeof(b));
		memset(c, 0, sizeof(c));
		if (j > 0)
			memcpy(b, current_ident->buf, (size_t)(j > IDENT_BUF_LEN ? IDENT_BUF_LEN : j));
		b[j > IDENT_BUF_LEN ? IDENT_BUF_LEN : j] = '\0';
		/* strip possible trailing \r */
		if (j > 0 && b[j-1] == '\r')
			b[j-1] = '\0';
		strncpy(c, &current_ident->buf[j + 1], IDENT_BUF_LEN);
		c[IDENT_BUF_LEN] = '\0';

		ident_msg(current_ident, b);
		
		strcpy(current_ident->buf, c);	
		
		if(current_ident->conn) // reply send
		{
		    kill_socket(current_ident->sock);
		    current_ident->sock = 0;
		    return;
		}		
	}
}

/* main ident message parser */
void ident_msg(struct ident_list *current_ident, char *b)
{
    char *bp = b;
    char request[IDENT_BUF_LEN + 1];
	char *serv = NULL, *cli = NULL;

    /* backup request data */
    memset(request, 0, sizeof(request));   
    strncpy(request, b, IDENT_BUF_LEN); 
    request[IDENT_BUF_LEN] = '\0';

	while(*bp != ',' && *bp) bp++;
	*bp++ = 0;
	
	// trim white spaces
	serv = trim(b); 
	cli = trim(bp);
    
    current_ident->conn = 1;  
	
	if(serv[0] && cli[0])
    {
        int server_port = atoi(serv);
        int client_port = atoi(cli);

        struct sockaddr_in sin;
        socklen_t sin_len = sizeof(struct sockaddr_in);
        
        if((client_port < 1 || client_port > 65535) || (server_port < 1 || server_port > 65535))
        {
            sock_write(current_ident->sock, "%s : ERROR : INVALID-PORT\r\n", request);
            
#ifdef DEBUG            
            printf("> send ident error reply: invalid port requested\n");
#endif  
            return;
        }
        
        if(ntohs(client_port) != server.port)
        {
            sock_write(current_ident->sock, "%s : ERROR : NO-USER\r\n", request);   
#ifdef DEBUG            
            printf("> send ident error reply: no user for requested port pair\n");
#endif  
            return;    
        }
        
        if(first_conn)
        {
            for(clone_conn = first_conn; clone_conn; clone_conn = clone_conn->next)
            {
                if(clone_conn->sock)
                {
                // continue on error
                    if(getsockname(clone_conn->sock, (struct sockaddr *) &sin, &sin_len) == -1)
                    {
#ifdef DEBUG            
                        printf("> #%i ident_msg: getsockname: %s", clone_conn->id, strerror(errno));
#endif                        
                        continue;
                    }  
                    
                    if(ntohs(sin.sin_port) == server_port)
                    {
                        sock_write(current_ident->sock, "%s : USERID : UNIX : %s\r\n", request, clone_conn->user);
#ifdef DEBUG            
                        printf("> send ident reply %s for #%i \n", clone_conn->user, clone_conn->id);
#endif                        
                        return;
                    }
                }
            }
        }

        sock_write(current_ident->sock, "%s : ERROR : NO-USER\r\n", request); 
        
#ifdef DEBUG            
        printf("> send ident error reply: no user\n");
#endif    
        return;
    }
    sock_write(current_ident->sock, "%s : ERROR : UNKNOWN-ERROR\r\n", request);    
    
#ifdef DEBUG            
    printf("> send ident error reply: invalid request data\n");
#endif     
}
