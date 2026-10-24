/*
 * simple, unoptimized, maybe buggy irc cloner program BUT IT ROCKS
 * aka [joco] cloner
 * by [joco] of --sirk-labs--
 * check out crew page at http://www.sirklabs.hu/
 * my homepage: http://joco.sirklabs.hu/
 * my e-mail address: joco@sirklabs.hu
 *
 * released to the public domain: 2002.07.19. please don't abuse.
 *
 * the main idea is not to create a cool taker program, however i admit that
 * under certain conditions it's easy to use this for that purpose. it was
 * written as a kind of a.i. it demonstrates how an irc client can be the
 * fastest if it calculates with ircd's penalty system. i think i managed to
 * do this, but you'd better check it out yourself.
 *
 * about fast mode: it's a lot faster but sometimes it can be a disatvantage.
 * when in fast mode, the cloner assumes that after it sends an op command,
 * then it doesn't wait for the irc server's answer but goes on with the opped
 * client right then. when the lags are small (check out with the lag command)
 * and you have a good connection, then fast mode is really cool! 100 clones
 * can op each other in a millisecond. the problem is when you're lagged. for
 * example client1 ops client2, and client2 ops client3 right after, but when
 * the lag calculations are not correct, ircd sees this: client2 opped client3
 * and then client1 opped client2. so client3 won't have chanop status and
 * the whole thing is messed up.
 *
 * it's so easy to use that i won't write anything about it. see the help
 * command.
 *
 * note about testing it against other similar software: when you test it on
 * small, local networks, beware: the last connected client gets processed
 * first in ircd, i.e. the process order and connect order are reverse. if you
 * want a good testing environment, connect the two (or more) cloners at about
 * the same time and same delay. however, on small networks, the last connected
 * client will _always_ have an advantage.
 *
 * as i mentioned, it's buggy. it doesn't care about channel modes, it always
 * assumes the clients' nicks aren't in use, the clients are not banned, etc.
 *
 * if you're a programmer and use this engine for a new program or just
 * enhance this one, please mention my name and also let me know about what you
 * have done. thanks.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

/* #define DEBUG */

int server, port, number, delay;
char *nick_prefix;

time_t now;

int fast, mode;

char chan[51];
char nick[16];
int clones_on_chan = 0;
int loaded;

struct {
	int sock;
	int conn;
	char *buf;
	int join[100]; /* local join list */
	int msg; /* number of messages seen on channel */
	int penalty;
	struct user_list {
		char nick[16];
		int op; /* is op */
		int clone; /* is clone */
		int op_list; /* is in op_list */
		struct user_list *next;
	} *first_user;
} clone[100];
struct user_list *user, *user2;

int order[100], new_order[100], next_check, check_num = 0, n_order,
		n_new_order, order_num = 0, o_start, o_end;

/* global join list: we count how many times we saw other clones joining
main idea: we synchronize number of messages depending on the number of joins
seen by a clone
message numbers: we rely on the clone who saw the most */
struct {
	unsigned char used, id, num;
	int msg;
} join_list[1000];
int n_join_list = 0;

/* queue sent commands so that we can check for errors and resend commands */
struct {
	unsigned char used, id, type; /* type: 0 = op, 1 = kick, 2 = deop */
	char nick[10];
} queue[1000];
int n_queue = 0;

/* oplist: we let them be ops */
struct op_list {
	char nick[16];
	struct op_list *next;
} *first_op = NULL;
struct op_list *op, *op2;

struct sockaddr_in sa;

char outbuf[500], temp[500];

/* print usage */
void usage(char *prg)
{
	printf("%s server port number delay nick\n", prg);
	exit(0);
}

/* die with an error message */
void error(char *msg)
{
	printf("error: %s\n", msg);
	exit(0);
}

/* write to a socket */
void sock_write(int fd, char *format, ...)
{
	va_list args;
	char s[513];

	va_start(args, format);
	vsprintf(s, format, args);
	va_end(args);

#ifdef DEBUG
	{
		int i;
		FILE *f;
		char fn[16];

		for(i = 0; i < number; i++)
			if(clone[i].sock == fd)
			{
				sprintf(fn, "%s%.2i", nick_prefix, i);
				f = fopen(fn, "a+");
				fprintf(f, "[%i] w [%s]\n", (int) now, s);
				fclose(f);
			}
	}
#endif

	if((write(fd, &s, strlen(s))) == -1) error("write()");
}

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

/* user command on console */
void user_cmd(void)
{
	char b[103], *bp;
	int i;
	char *cmd = NULL, *arg = NULL;

	memset(b, 0, sizeof(b));
	fgets(b, sizeof(b), stdin);
	if(b[101])
		return;
	b[strlen(b) - 1] = 0;

	if(loaded < number)
	{
		printf("> wait\n");
		return;
	}

	bp = b;
	while (*bp != ' ' && *bp) bp++;
	*bp++ = 0;
	cmd = b;
	arg = bp;

	if(!strcmp(cmd, "help"))
	{
		printf("> commands:\n"
			"> chan         set chan\n"
			"> join         join chan\n"
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

	if(!strcmp(cmd, "chan"))
	{
		if(!arg[0])
		{
			printf("> no channel given\n");
			return;
		}
		if(clones_on_chan)
		{
			for(i = 0; i < number; i++)
			{
				add_penalty(i, 5);
				sock_write(clone[i].sock, "PART %s :\002<([joco])>\002\r\n", chan);
			}
			printf("> parting %s\n", chan);
		}

		strcpy(chan, arg);
		clones_on_chan = 0;

		printf("> chan set to %s\n", chan);
		return;
	}

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

		printf("> joining %s\n", chan);

		for(i = 0; i < number; i++)
		{
			add_penalty(i, 3);
			sock_write(clone[i].sock, "JOIN %s\r\n", chan);
		}
		clones_on_chan = 1;

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

		for(i = 0; i < number; i++)
		{
			add_penalty(i, 5);
			sock_write(clone[i].sock, "PART %s :\002<([joco])>\002\r\n", chan);
		}
		clones_on_chan = 0;

		return;
	}

	if(!strcmp(cmd, "deop"))
	{
		for(i = 0; i < number; i++)
		{
			add_penalty(i, 4);
			sock_write(clone[i].sock, "MODE %s -o %s%.2i\r\n",
					chan, nick_prefix, i);
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
				if(op == first_op)
					first_op = first_op->next;
				else
					op2->next = op->next;
				free(op);
				for(i = 0; i < number; i++)
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

		op = first_op;
		while(op && op->next)
			op = op->next;
		op2 = malloc(sizeof(struct op_list));
		if(first_op)
			op->next = op2;
		else
			first_op = op2;
		strcpy(op2->nick, arg);
		op2->next = NULL;

		for(i = 0; i < number; i++)
			for(user = clone[i].first_user; user; user = user->next)
				if(!strcasecmp(user->nick, arg))
					user->op_list = 1;

		printf("> added %s\n", arg);
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
			printf("%s%.2i: %i%s", nick_prefix, order[i],
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
		for(i = 0; i < number; i++)
		{
			free(clone[i].buf);
			sock_write(clone[i].sock, "QUIT :\002<([joco])>\002\r\n");
		}
		sleep(1);
		printf("> byebye\n");
		exit(0);
	}

	printf("> unknown command, try help\n");
}

/* connect a client, non-blocking */
void connect_clone(int n)
{
	printf("> connecting #%i\n" , n + 1);

	clone[n].buf = malloc(1025);
	clone[n].buf[0] = 0;

	if((clone[n].sock = socket(PF_INET, SOCK_STREAM,
			IPPROTO_TCP)) == -1)
		error("socket()");
	fcntl(clone[n].sock, F_SETFL, O_NONBLOCK);
	if(connect(clone[n].sock, (struct sockaddr *) &sa,
			sizeof(sa)) < 0)
	{
		if(errno != EINPROGRESS)
			error("connect()");
	}
}

/* get nick from nick!user@host */
void get_nick(char *who)
{
	char *p = who;

	if(who[0] == ':') who++;
	while (*p != '!' && *p) p++;
	*p = 0;
	strcpy(nick, who);
	return;
}

/* get clone id from nick */
int get_clone_id(char *nick)
{
	int i;
	char n[10];

	if(strncmp(nick, nick_prefix, strlen(nick_prefix)))
		return -1;
	i = atoi((char *) &nick[strlen(nick_prefix)]);
	sprintf(n, "%s%.2i", nick_prefix, i);
	if(strcmp(n, nick)) return -1;
	return i;
}

/* set client flags: id, is in oplist */
void set_flags(struct user_list *who)
{
	who->clone = get_clone_id(who->nick);
	who->op_list = 0;

	for(op = first_op; op; op = op->next)
		if(!strcasecmp(op->nick, who->nick))
		{
			who->op_list = 1;
			break;
		}
}

/* join list management */
void check_join_list(void)
{
	int i, j, min = 10000;

	for(i = 0; i < number; i++)
		for(j = 0; j < number; j++)
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
	if(n == i && clone[n].first_user)
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
			memset(user2->nick, 0, 10);

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
	unsigned char j[100];

	memset(j, 0, sizeof(j));
	for(user = clone[n].first_user; user; user = user->next)
		if(user->clone > -1)
			j[user->clone]++;

	for(i = 0; i < number; i++)
		if(!j[i])
			clone[i].join[n]++;
}

/* delete user from channel for various reasons */
void got_part_quit_kick(int n, char *who)
{
	clone[n].msg++;

	get_nick(who);

	if(get_clone_id(nick) == n)
	{
		clone[n].msg = 0;
		if(clones_on_chan)
		{
			add_penalty(n, 3);
			sock_write(clone[n].sock, "JOIN %s\r\n", chan);
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
	clone[n].msg++;

	get_nick(who);

	for(user = clone[n].first_user; user; user = user->next)
		if(!strcmp(user->nick, nick))
		{
			strcpy(user->nick, new_nick);
			break;
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
	int i, j;

	get_nick(who);
	i = get_clone_id(nick);
	if(i == -1)
		return;

	j = atoi(what);
	new_order[n_new_order++] = i;

	if(n_new_order == number)
	{
		memcpy(order, new_order, sizeof(order));
		n_order = n_new_order;
		o_end = now;
	}
}

/* main ircd message parser */
void irc_msg(int n, char *b)
{
	int i, j, k, l;
	char *s[20];

	for(i = 0; i < 20; i++)
		s[i] = NULL;
	k = j = 0;

#ifdef DEBUG
	{
		FILE *f;
		char fn[10];

		sprintf(fn, "%s%.2i", nick_prefix, n);
		f = fopen(fn, "a");
		fprintf(f, "[%i] r [%s]\n", (int) now, b);
		fclose(f);
	}
#endif

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

	if(!strcmp(s[1], "001"))
	{
		printf("> #%i on irc\n", n + 1);
		clone[n].conn = 2;
		add_penalty(n, 2);
		loaded++;
		if(loaded == number)
			printf("> clones loaded\n");
	}

	if(strcmp(s[1], "020") && clone[n].conn != 2)
	{
		printf("[%s]\n", b);
		error("not on irc");
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
		got_privmsg(n, s[0], s[3]);

	for(i = 0; i < 20; i++)
		free(s[i]);
}

/* network parser */
void irc(int n)
{
	int i, j;
	char b[513];

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
		strncpy(b, clone[n].buf, j - 1);
		irc_msg(n, b);
		strcpy(clone[n].buf, (char *) &clone[n].buf[j + 1]);
	}
}

/* check input on sockets and console */
void check_input(void)
{
	struct timeval t;
	fd_set fdr, fdw;
	int i, j, k;

	t.tv_sec = 0;
	t.tv_usec = 1000;

	FD_ZERO(&fdr);
	FD_ZERO(&fdw);

	for(i = 0; i < number; i++)
		if(clone[i].sock)
		{
			if(clone[i].conn)
				FD_SET(clone[i].sock, &fdr);
			else
				FD_SET(clone[i].sock, &fdw);
		}
	FD_SET(0, &fdr);

	select(getdtablesize(), &fdr, &fdw, NULL, &t);

	for(i = 0; i < number; i++)
		if(clone[i].sock)
		{
			if(FD_ISSET(clone[i].sock, &fdr))
			{
				k = j = strlen(clone[i].buf);
				if((k += read(clone[i].sock,
						(char *)&clone[i].buf[j],
						1024 - j)) == -1)
					error("read()");
				clone[i].buf[k] = 0;
				irc(i);
			}
			if(FD_ISSET(clone[i].sock, &fdw)) {
				int j;
				socklen_t k = 4;

				getsockopt(clone[i].sock, SOL_SOCKET,
						SO_ERROR, (void *) &j, &k);
				if(j)
					error("connect()");
				clone[i].conn = 1;
				sock_write(clone[i].sock,
						"NICK %s%.2i\r\n"
						"USER %s * * :\002<([joco])>\002\r\n",
						nick_prefix, i, nick_prefix,
						nick_prefix);
			}
		}

	if(FD_ISSET(0, &fdr))
		user_cmd();
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

/* here comes the intelligence */
void work(void)
{
	int i, j = -1, k, l, m = -1, n = -1, px = -1;
	struct {
		int type;
		char *nick;
		int clone;
	} job[1000];
	int n_job = 0, pj;
	struct user_list *clones[101];
	int pc[101];
	int n_clones = 0;
	struct user_list *op[101];
	int p[101];
	int n_op = 0;
	char outbuf[512], tmp1[128], tmp2[128], tmp3[256];

	memset(job, 0, sizeof(job));

/* who do we rely on? */
	for(i = 0; i < number; i++)
		if(clone[i].msg > m)
		{
			m = clone[i].msg;
			n = i;
		}

/* sort clones depending on penalties */
	for(user = clone[n].first_user; user; user = user->next)
	{
		if(user->clone != -1)
		{
			clones[n_clones] = user;
			pc[n_clones] = has_penalty(user->clone);
			n_clones++;
		}
	}

/* sort clones depending on penalty and op status */
	for(i = n_clones - 1; i >= 0; i--)
		for(j = 0; j < i; j++)
			if(pc[j] > pc[i])
			{
				clones[100] = clones[i];
				clones[i] = clones[j];
				clones[j] = clones[100];
				pc[100] = pc[i];
				pc[i] = pc[j];
				pc[j] = pc[100];
				break;
			}

	for(i = 0; i < n_clones; i++)
	{
		if(clones[i]->op)
		{
			op[n_op] = clones[i];
			p[n_op++] = pc[i];
		}
	}

	if(!n_op)
		return;

/* op other clones */
	for(i = 0; i < n_clones; i++)
	{
		if(!clones[i]->op)
		{
			m = 1;
			for(l = 0, j = 0; l < n_queue; j++)
			{
				if(queue[j].used && queue[j].type == 0 &&
						!strcmp(queue[j].nick,
						clones[i]->nick))
					{
						m = 0;
						break;
					}

				if(queue[j].used)
					l++;
			}

			if(m)
			{
				job[n_job].type = 0;
				job[n_job].nick = clones[i]->nick;
				job[n_job++].clone = clones[i]->clone;
			}
		}
	}

/* deop clients */
	for(user = clone[n].first_user; user; user = user->next)
	{
		if(n_job == 1000) break;

		if(user->clone == -1 && !user->op_list && user->op)
		{
			m = 1;
			for(l = 0, i = 0; l < n_queue; i++)
			{
				if(queue[i].used && queue[i].type == mode &&
						!strcmp(queue[i].nick,
						user->nick))
					{
						m = 0;
						break;
					}

				if(queue[i].used)
					l++;
			}

			if(m)
			{
				job[n_job].type = mode;
				job[n_job].nick = user->nick;
				job[n_job++].clone = user->clone;
			}
		}
	}

/* op users in oplist */
	for(user = clone[n].first_user; user; user = user->next)
	{
		if(n_job == 1000) break;

		if(user->op_list && !user->op)
		{
			m = 1;
			for(l = 0, i = 0; l < n_queue; i++)
			{
				if(queue[i].used && queue[i].type == 0 &&
						!strcmp(queue[i].nick,
						user->nick))
					{
						m = 0;
						break;
					}

				if(queue[i].used)
					l++;
			}

			if(m)
			{
				job[n_job].type = 0;
				job[n_job].nick = user->nick;
				job[n_job++].clone = user->clone;
			}
		}
	}

	if(!n_job)
		return;

/* output buffered commands */
	for(i = 0, pj = 0;;)
	{
		if(p[i] >= 10)
			return;

		outbuf[0] = 0;

check:
		px=0;

		j = (int) (((10 - p[i]) - 2) / 3) * 3;
		if(j < 0)
			j = 0;

		if(job[pj].type == 1)
			l = (j / 3) + 4;
		else
			l = (j / 3) + 3;

		k = pj;
		while(((job[k].type & 1) == (job[pj].type & 1)) &&
				k < pj + l && k < n_job)
			k++;

		if(job[pj].type == 1)
		{
			tmp1[0] = 0;
			tmp2[0] = 0;

			switch(k - pj)
			{
				case 1:
					sprintf(tmp1, "%s", job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					px = 4;
					break;
				case 2:
					sprintf(tmp1, "%s,%s", job[pj].nick,
							job[pj + 1].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj + 1].nick);
					px = 7;
					break;
				case 3:
					sprintf(tmp1, "%s,%s,%s", job[pj].nick,
							job[pj + 1].nick,
							job[pj + 2].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj + 1].nick);
					add_queue(op[i]->clone, 1, job[pj + 2].nick);
					px = 10;
					break;
				case 4:
					sprintf(tmp1, "%s,%s,%s,%s",
							job[pj].nick,
							job[pj + 1].nick,
							job[pj + 2].nick,
							job[pj + 3].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj + 1].nick);
					add_queue(op[i]->clone, 1, job[pj + 2].nick);
					add_queue(op[i]->clone, 1, job[pj + 3].nick);
					px = 13;
					break;
				case 5:
					sprintf(tmp1, "%s", job[pj].nick);
					sprintf(tmp2, "%s,%s,%s,%s",
							job[pj + 1].nick,
							job[pj + 2].nick,
							job[pj + 3].nick,
							job[pj + 4].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj + 1].nick);
					add_queue(op[i]->clone, 1, job[pj + 2].nick);
					add_queue(op[i]->clone, 1, job[pj + 3].nick);
					add_queue(op[i]->clone, 1, job[pj + 4].nick);
					px = 4 + 13;
					break;
				case 6:
					sprintf(tmp1, "%s,%s", job[pj].nick,
							job[pj + 1].nick);
					sprintf(tmp2, "%s,%s,%s,%s",
							job[pj + 2].nick,
							job[pj + 3].nick,
							job[pj + 4].nick,
							job[pj + 5].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj + 1].nick);
					add_queue(op[i]->clone, 1, job[pj + 2].nick);
					add_queue(op[i]->clone, 1, job[pj + 3].nick);
					add_queue(op[i]->clone, 1, job[pj + 4].nick);
					add_queue(op[i]->clone, 1, job[pj + 5].nick);
					px = 7 + 13;
					break;
				case 7:
					sprintf(tmp1, "%s,%s,%s", job[pj].nick,
							job[pj + 1].nick,
							job[pj + 2].nick);
					sprintf(tmp2, "%s,%s,%s,%s",
							job[pj + 3].nick,
							job[pj + 4].nick,
							job[pj + 5].nick,
							job[pj + 6].nick);
					add_queue(op[i]->clone, 1, job[pj].nick);
					add_queue(op[i]->clone, 1, job[pj + 1].nick);
					add_queue(op[i]->clone, 1, job[pj + 2].nick);
					add_queue(op[i]->clone, 1, job[pj + 3].nick);
					add_queue(op[i]->clone, 1, job[pj + 4].nick);
					add_queue(op[i]->clone, 1, job[pj + 5].nick);
					add_queue(op[i]->clone, 1, job[pj + 6].nick);
					px = 10 + 13;
			}

			if(tmp1[0])
			{
				if(tmp2[0])
					sprintf(tmp3, "KICK %s %s :\002<([joco])>\002\r\n"
							"KICK %s %s :\002<([joco])>\002\r\n",
							chan, tmp1, chan,
							tmp2);
				else
					sprintf(tmp3, "KICK %s %s :\002<([joco])>\002\r\n", chan,
							tmp1);
				strcat(outbuf, tmp3);
			}
		}
		else
		{
			char buf1[6], buf2[6];
			int bl, ls, n;
			char *nn[6], t[6];
			int np = 0;

			memset(buf1, 0, 6);
			memset(buf2, 0, 6);

			for(ls = -1, bl = 0, j = k - 1, n = 0; j >= pj &&
					n < 3; j--)
			{
				if(!job[j].nick)
					continue;

				if(job[j].type != ls)
				{
					if(job[j].type == 0)
						buf2[bl++] = '+';
					else
						buf2[bl++] = '-';
					ls = job[j].type;
				}
				buf2[bl++] = 'o';
				nn[np] = job[j].nick;
				t[np++] = job[j].type;
				n++;
			}

			for(ls = -1, bl = 0; j >= pj; j--)
			{
				if(!job[j].nick)
					continue;

				if(job[j].type != ls)
				{
					if(job[j].type == 0)
						buf1[bl++] = '+';
					else
						buf1[bl++] = '-';
					ls = job[j].type;
				}
				buf1[bl++] = 'o';
				nn[np] = job[j].nick;
				t[np++] = job[j].type;
				n++;
			}

			tmp1[0] = 0;
			tmp2[0] = 0;

			switch(np)
			{
				case 1:
					sprintf(tmp1, "%s %s", buf2, nn[0]);
					add_queue(op[i]->clone, t[0], nn[0]);
					px = 4;
					break;
				case 2:
					sprintf(tmp1, "%s %s %s", buf2, nn[0],
							nn[1]);
					add_queue(op[i]->clone, t[0], nn[0]);
					add_queue(op[i]->clone, t[1], nn[1]);
					px = 7;
					break;
				case 3:
					sprintf(tmp1, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(op[i]->clone, t[0], nn[0]);
					add_queue(op[i]->clone, t[1], nn[1]);
					add_queue(op[i]->clone, t[2], nn[2]);
					px = 10;
					break;
				case 4:
					sprintf(tmp1, "%s %s", buf1, nn[3]);
					sprintf(tmp2, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(op[i]->clone, t[0], nn[0]);
					add_queue(op[i]->clone, t[1], nn[1]);
					add_queue(op[i]->clone, t[2], nn[2]);
					add_queue(op[i]->clone, t[3], nn[3]);
					px = 4 + 10;
					break;
				case 5:
					sprintf(tmp1, "%s %s %s", buf1, nn[3],
							nn[4]);
					sprintf(tmp2, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(op[i]->clone, t[0], nn[0]);
					add_queue(op[i]->clone, t[1], nn[1]);
					add_queue(op[i]->clone, t[2], nn[2]);
					add_queue(op[i]->clone, t[3], nn[3]);
					add_queue(op[i]->clone, t[4], nn[4]);
					px = 7 + 10;
					break;
				case 6:
					sprintf(tmp1, "%s %s %s %s", buf1,
							nn[3], nn[4], nn[5]);
					sprintf(tmp2, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(op[i]->clone, t[0], nn[0]);
					add_queue(op[i]->clone, t[1], nn[1]);
					add_queue(op[i]->clone, t[2], nn[2]);
					add_queue(op[i]->clone, t[3], nn[3]);
					add_queue(op[i]->clone, t[4], nn[4]);
					add_queue(op[i]->clone, t[5], nn[5]);
					px = 10 + 10;
			}

			if(tmp1[0])
			{
				if(tmp2[0])
					sprintf(tmp3, "MODE %s %s\r\n"
							"MODE %s %s\r\n",
							chan, tmp1, chan,
							tmp2);
				else
					sprintf(tmp3, "MODE %s %s\r\n", chan,
							tmp1);
				strcat(outbuf, tmp3);
			}
		}

		pj = k;
		add_penalty(op[i]->clone, px);
		p[i] = has_penalty(op[i]->clone);

		if(pj < n_job && p[i] < 10)
			goto check;

		sock_write(clone[op[i]->clone].sock, "%s", outbuf);

		i++;
		if(i == n_op)
			break;
		if(pj == n_job)
			break;
	}
}

/* work in fast mode (rules!)
it's like work() but it uses clones who we tried to op but don't know if that
succeeded */
void work_fast(void)
{
	int i, ii, j = -1, k, l, m = -1, n = -1, px = -1;
	struct {
		int type;
		char *nick;
		int clone;
	} job[1000];
	int n_job = 0, pj;
	struct user_list *clones[101];
	int o[101], p[101], op[101];
	int n_clones = 0;
	int n_op = 0;
	char outbuf[512], tmp1[128], tmp2[128], tmp3[256];

	memset(job, 0, sizeof(job));

	for(i = 0; i < number; i++)
		if(clone[i].msg > m)
		{
			m = clone[i].msg;
			n = i;
		}

	for(user = clone[n].first_user; user; user = user->next)
	{
		if(user->clone != -1)
		{
			clones[n_clones] = user;
			p[n_clones] = has_penalty(user->clone);
			op[n_clones] = user->op;
			n_clones++;
		}
	}
	for(i = 0; i < 100; i++)
		o[i] = 101;
	for(i = 0; i < n_order; i++)
		for(j = 0; j < n_clones; j++)
			if(order[i] == clones[j]->clone)
				o[j] = i;

	for(i = n_clones - 1; i > 0; i--)
		for(j = 0; j < i; j++)
			if(o[i] < o[j])
			{
				o[100] = o[i];
				o[i] = o[j];
				o[j] = o[100];
				clones[100] = clones[i];
				clones[i] = clones[j];
				clones[j] = clones[100];
				p[100] = p[i];
				p[i] = p[j];
				p[j] = p[100];
				op[100] = op[i];
				op[i] = op[j];
				op[j] = op[100];
				break;
			}

	for(i = 0; i < n_clones; i++)
		if(clones[i]->op)
			n_op++;
	if(!n_op)
		return;

	for(ii = 0; ii < n_clones; ii++)
		if(op[ii])
			break;

	for(i = n_clones - 1; i > ii + 1; i--)
		for(j = ii + 1; j < i; j++)
			if(p[i] < p[j])
			{
				o[100] = o[i];
				o[i] = o[j];
				o[j] = o[100];
				clones[100] = clones[i];
				clones[i] = clones[j];
				clones[j] = clones[100];
				p[100] = p[i];
				p[i] = p[j];
				p[j] = p[100];
				op[100] = op[i];
				op[i] = op[j];
				op[j] = op[100];
				break;
			}

	for(i = ii;;)
	{
		if(!clones[i]->op)
		{
			m = 1;
			for(l = 0, j = 0; l < n_queue; j++)
			{
				if(queue[j].used && queue[j].type == 0 &&
						!strcmp(queue[j].nick,
						clones[i]->nick))
					{
						m = 0;
						break;
					}

				if(queue[j].used)
					l++;
			}

			if(m)
			{
				job[n_job].type = 0;
				job[n_job].nick = clones[i]->nick;
				job[n_job++].clone = i;
			}
		}

		i++;
		if(i == n_clones)
			i = 0;
		if(i == ii)
			break;
	}

	for(user = clone[n].first_user; user; user = user->next)
	{
		if(n_job == 1000) break;

		if(user->clone == -1 && !user->op_list && user->op)
		{
			m = 1;
			for(l = 0, i = 0; l < n_queue; i++)
			{
				if(queue[i].used && queue[i].type == mode &&
						!strcmp(queue[i].nick,
						user->nick))
					{
						m = 0;
						break;
					}

				if(queue[i].used)
					l++;
			}

			if(m)
			{
				job[n_job].type = mode;
				job[n_job].nick = user->nick;
				job[n_job++].clone = -1;
			}
		}
	}

	for(user = clone[n].first_user; user; user = user->next)
	{
		if(n_job == 1000) break;

		if(user->op_list && !user->op)
		{
			m = 1;
			for(l = 0, i = 0; l < n_queue; i++)
			{
				if(queue[i].used && queue[i].type == 0 &&
						!strcmp(queue[i].nick,
						user->nick))
					{
						m = 0;
						break;
					}

				if(queue[i].used)
					l++;
			}

			if(m)
			{
				job[n_job].type = 0;
				job[n_job].nick = user->nick;
				job[n_job++].clone = -1;
			}
		}
	}

	if(!n_job)
		return;

	for(i = 0, pj = 0; i < n_clones; i++)
	{
		if(p[i] >= 10)
			continue;
		if(o[i] > 100)
			break;
		if(!op[i])
			continue;

		outbuf[0] = 0;

check:
		px = 0;

		j = (int) (((10 - p[i]) - 2) / 3) * 3;
		if(j < 0)
			j = 0;

		if(job[pj].type == 1)
			l = (j / 3) + 4;
		else
			l = (j / 3) + 3;

		k = pj;
		while(((job[k].type & 1) == (job[pj].type & 1)) &&
				k < pj + l && k < n_job)
			k++;

		if(job[pj].type == 1)
		{
			tmp1[0] = 0;
			tmp2[0] = 0;

			switch(k - pj)
			{
				case 1:
					sprintf(tmp1, "%s", job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					px = 4;
					break;
				case 2:
					sprintf(tmp1, "%s,%s", job[pj].nick,
							job[pj + 1].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj + 1].nick);
					px = 7;
					break;
				case 3:
					sprintf(tmp1, "%s,%s,%s", job[pj].nick,
							job[pj + 1].nick,
							job[pj + 2].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj + 1].nick);
					add_queue(clones[i]->clone, 1, job[pj + 2].nick);
					px = 10;
					break;
				case 4:
					sprintf(tmp1, "%s,%s,%s,%s",
							job[pj].nick,
							job[pj + 1].nick,
							job[pj + 2].nick,
							job[pj + 3].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj + 1].nick);
					add_queue(clones[i]->clone, 1, job[pj + 2].nick);
					add_queue(clones[i]->clone, 1, job[pj + 3].nick);
					px = 13;
					break;
				case 5:
					sprintf(tmp1, "%s", job[pj].nick);
					sprintf(tmp2, "%s,%s,%s,%s",
							job[pj + 1].nick,
							job[pj + 2].nick,
							job[pj + 3].nick,
							job[pj + 4].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj + 1].nick);
					add_queue(clones[i]->clone, 1, job[pj + 2].nick);
					add_queue(clones[i]->clone, 1, job[pj + 3].nick);
					add_queue(clones[i]->clone, 1, job[pj + 4].nick);
					px = 4 + 13;
					break;
				case 6:
					sprintf(tmp1, "%s,%s", job[pj].nick,
							job[pj + 1].nick);
					sprintf(tmp2, "%s,%s,%s,%s",
							job[pj + 2].nick,
							job[pj + 3].nick,
							job[pj + 4].nick,
							job[pj + 5].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj + 1].nick);
					add_queue(clones[i]->clone, 1, job[pj + 2].nick);
					add_queue(clones[i]->clone, 1, job[pj + 3].nick);
					add_queue(clones[i]->clone, 1, job[pj + 4].nick);
					add_queue(clones[i]->clone, 1, job[pj + 5].nick);
					px = 7 + 13;
					break;
				case 7:
					sprintf(tmp1, "%s,%s,%s", job[pj].nick,
							job[pj + 1].nick,
							job[pj + 2].nick);
					sprintf(tmp2, "%s,%s,%s,%s",
							job[pj + 3].nick,
							job[pj + 4].nick,
							job[pj + 5].nick,
							job[pj + 6].nick);
					add_queue(clones[i]->clone, 1, job[pj].nick);
					add_queue(clones[i]->clone, 1, job[pj + 1].nick);
					add_queue(clones[i]->clone, 1, job[pj + 2].nick);
					add_queue(clones[i]->clone, 1, job[pj + 3].nick);
					add_queue(clones[i]->clone, 1, job[pj + 4].nick);
					add_queue(clones[i]->clone, 1, job[pj + 5].nick);
					add_queue(clones[i]->clone, 1, job[pj + 6].nick);
					px = 10 + 13;
			}

			if(k - pj)
			{
				if(tmp2[0])
					sprintf(tmp3, "KICK %s %s :\002<([joco])>\002\r\n"
							"KICK %s %s :\002<([joco])>\002\r\n",
							chan, tmp1, chan,
							tmp2);
				else
					sprintf(tmp3, "KICK %s %s :\002<([joco])>\002\r\n", chan,
							tmp1);
				strcat(outbuf, tmp3);
			}
		}
		else
		{
			char buf1[6], buf2[6];
			int bl, ls, n;
			char *nn[6], t[6];
			int cc[6];
			int np = 0;

			memset(buf1, 0, 6);
			memset(buf2, 0, 6);

			for(ls = -1, bl = 0, j = k - 1, n = 0; j >= pj &&
					n < 3; j--)
			{
				if(!job[j].nick)
					continue;

				if(job[j].type != ls)
				{
					if(job[j].type == 0)
						buf2[bl++] = '+';
					else
						buf2[bl++] = '-';
					ls = job[j].type;
				}
				buf2[bl++] = 'o';
				nn[np] = job[j].nick;
				cc[np] = job[j].clone;
				t[np++] = job[j].type;
				n++;
			}

			for(ls = -1, bl = 0; j >= pj; j--)
			{
				if(!job[j].nick)
					continue;

				if(job[j].type != ls)
				{
					if(job[j].type == 0)
						buf1[bl++] = '+';
					else
						buf1[bl++] = '-';
					ls = job[j].type;
				}
				buf1[bl++] = 'o';
				nn[np] = job[j].nick;
				cc[np] = job[j].clone;
				t[np++] = job[j].type;
				n++;
			}

			tmp1[0] = 0;
			tmp2[0] = 0;

			switch(np)
			{
				case 1:
					sprintf(tmp1, "%s %s", buf2, nn[0]);
					add_queue(clones[i]->clone, t[0], nn[0]);
					if(!t[0] && cc[0] != -1)
						op[cc[0]] = 1;
					px = 4;
					break;
				case 2:
					sprintf(tmp1, "%s %s %s", buf2, nn[0],
							nn[1]);
					add_queue(clones[i]->clone, t[0], nn[0]);
					if(!t[0] && cc[0] != -1)
						op[cc[0]] = 1;
					add_queue(clones[i]->clone, t[1], nn[1]);
					if(!t[1] && cc[1] != -1)
						op[cc[1]] = 1;
					px = 7;
					break;
				case 3:
					sprintf(tmp1, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(clones[i]->clone, t[0], nn[0]);
					if(!t[0] && cc[0] != -1)
						op[cc[0]] = 1;
					add_queue(clones[i]->clone, t[1], nn[1]);
					if(!t[1] && cc[1] != -1)
						op[cc[1]] = 1;
					add_queue(clones[i]->clone, t[2], nn[2]);
					if(!t[2] && cc[2] != -1)
						op[cc[2]] = 1;
					px = 10;
					break;
				case 4:
					sprintf(tmp1, "%s %s", buf1, nn[3]);
					sprintf(tmp2, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(clones[i]->clone, t[0], nn[0]);
					if(!t[0] && cc[0] != -1)
						op[cc[0]] = 1;
					add_queue(clones[i]->clone, t[1], nn[1]);
					if(!t[1] && cc[1] != -1)
						op[cc[1]] = 1;
					add_queue(clones[i]->clone, t[2], nn[2]);
					if(!t[2] && cc[2] != -1)
						op[cc[2]] = 1;
					add_queue(clones[i]->clone, t[3], nn[3]);
					if(!t[3] && cc[3] != -1)
						op[cc[3]] = 1;
					px = 4 + 10;
					break;
				case 5:
					sprintf(tmp1, "%s %s %s", buf1, nn[3],
							nn[4]);
					sprintf(tmp2, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(clones[i]->clone, t[0], nn[0]);
					if(!t[0] && cc[0] != -1)
						op[cc[0]] = 1;
					add_queue(clones[i]->clone, t[1], nn[1]);
					if(!t[1] && cc[1] != -1)
						op[cc[1]] = 1;
					add_queue(clones[i]->clone, t[2], nn[2]);
					if(!t[2] && cc[2] != -1)
						op[cc[2]] = 1;
					add_queue(clones[i]->clone, t[3], nn[3]);
					if(!t[3] && cc[3] != -1)
						op[cc[3]] = 1;
					add_queue(clones[i]->clone, t[4], nn[4]);
					if(!t[4] && cc[4] != -1)
						op[cc[4]] = 1;
					px = 7 + 10;
					break;
				case 6:
					sprintf(tmp1, "%s %s %s %s", buf1,
							nn[3], nn[4], nn[5]);
					sprintf(tmp2, "%s %s %s %s", buf2,
							nn[0], nn[1], nn[2]);
					add_queue(clones[i]->clone, t[0], nn[0]);
					if(!t[0] && cc[0] != -1)
						op[cc[0]] = 1;
					add_queue(clones[i]->clone, t[1], nn[1]);
					if(!t[1] && cc[1] != -1)
						op[cc[1]] = 1;
					add_queue(clones[i]->clone, t[2], nn[2]);
					if(!t[2] && cc[2] != -1)
						op[cc[2]] = 1;
					add_queue(clones[i]->clone, t[3], nn[3]);
					if(!t[3] && cc[3] != -1)
						op[cc[3]] = 1;
					add_queue(clones[i]->clone, t[4], nn[4]);
					if(!t[4] && cc[4] != -1)
						op[cc[4]] = 1;
					add_queue(clones[i]->clone, t[5], nn[5]);
					if(!t[5] && cc[5] != -1)
						op[cc[5]] = 1;
					px = 10 + 10;
			}

			if(np)
			{
				if(tmp2[0])
					sprintf(tmp3, "MODE %s %s\r\n"
							"MODE %s %s\r\n",
							chan, tmp1, chan,
							tmp2);
				else
					sprintf(tmp3, "MODE %s %s\r\n", chan,
							tmp1);
				strcat(outbuf, tmp3);
			}
		}

		pj = k;
		add_penalty(clones[i]->clone, px);
		p[i] = has_penalty(clones[i]->clone);

		if(pj < n_job && p[i] < 10)
			goto check;

		sock_write(clone[clones[i]->clone].sock, "%s", outbuf);

		if(pj == n_job)
			break;
	}
}

/* do an order check */
void check_order()
{
	int i;

	check_num++;
	for(i = 0; i < number; i++)
	{
		add_penalty(i, 2);
		sock_write(clone[i].sock, "PRIVMSG %s00 :%i\r\n", nick_prefix,
				check_num);
	}

	n_new_order = 0;
}

/* main routine */
int main(int argc, char *argv[])
{
	struct hostent *he;
	time_t next_connect;
	int i;

	if(argc != 6)
		usage(argv[0]);

	if (!(he = gethostbyname(argv[1])))
		error("gethostbyname()");
	memcpy(&server, he->h_addr, 4);
	port = htons(atoi(argv[2]));
	number = atoi(argv[3]);
	delay = atoi(argv[4]);
	nick_prefix = argv[5];

	fast = 0;
	mode = 1;

	if(!port)
		error("invalid port");
	if(number < 1 || number > 100)
		error("number not 1..100");
	if(strlen(nick_prefix) > 7)
		error("nick longer than 7");

	printf("> cloner by [joco], http://jocoweb.fw.hu/\n");

	printf("> %i clones to %i.%i.%i.%i:%i with %i sec delay and nickname "
			"%s\n", number, server & 0xff,
			(server >> 8) & 0xff, (server >> 16) & 0xff,
			(server >> 24) & 0xff, ntohs(port), delay,
			nick_prefix);

	memset(clone, 0, sizeof(clone));
	memset(&sa, 0, sizeof(sa));
	memset(join_list, 0, sizeof(join_list));
	memset(queue, 0, sizeof(queue));
	chan[0] = 0;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = server;
	sa.sin_port = port;
	next_connect = time(NULL);

#ifdef DEBUG
	{
		FILE *f;
		char fn[10];
		int i;

		for(i = 0; i < number; i++)
		{
			sprintf(fn,"%s%.2i", nick_prefix, i);
			f = fopen(fn, "w+");
			fclose(f);
		}
	}
#endif

	for(i = 0; loaded < number;)
	{
		now = time(NULL);
		if(now >= next_connect && i < number)
		{
			connect_clone(i++);
			next_connect = now + delay;
		}
		check_input();
	}

	for(;;)
	{
		now = time(NULL);

		if(now >= next_check)
		{
			check_order();
			next_check = now + 20;
			o_start = now;
		}

		check_input();
		if(clones_on_chan)
		{
			if(fast)
				work_fast();
			else
				work();
		}
	}

	return 0;
}
