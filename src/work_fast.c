#include "prots.h"

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
	} job[MAX_JOBS];
	int n_job = 0, pj;
	struct user_list *clones[MAX_CLONES + 1];
	int o[MAX_CLONES + 1], p[MAX_CLONES + 1], op[MAX_CLONES + 1];
	int n_clones = 0;
	int n_op = 0;
	char outbuf[512], tmp1[128], tmp2[128], tmp3[256];

	memset(job, 0, sizeof(job));

	for(i = 0; i < loaded; i++)
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
	for(i = 0; i < MAX_CLONES; i++)
		o[i] = MAX_CLONES + 1;
	for(i = 0; i < n_order; i++)
		for(j = 0; j < n_clones; j++)
			if(order[i] == clones[j]->clone)
				o[j] = i;

	for(i = n_clones - 1; i > 0; i--)
		for(j = 0; j < i; j++)
			if(o[i] < o[j])
			{
				o[MAX_CLONES] = o[i];
				o[i] = o[j];
				o[j] = o[MAX_CLONES];
				clones[MAX_CLONES] = clones[i];
				clones[i] = clones[j];
				clones[j] = clones[MAX_CLONES];
				p[MAX_CLONES] = p[i];
				p[i] = p[j];
				p[j] = p[MAX_CLONES];
				op[MAX_CLONES] = op[i];
				op[i] = op[j];
				op[j] = op[MAX_CLONES];
				break;
			}

	for(i = 0; i < n_clones; i++)
		if(clones[i]->op)
			n_op++;
			
	if(!n_op)
		return;

#ifdef HAVE_AUTHOR_CHECK	
    if(neutral)
        return;
#endif

	for(ii = 0; ii < n_clones; ii++)
		if(op[ii])
			break;

	for(i = n_clones - 1; i > ii + 1; i--)
		for(j = ii + 1; j < i; j++)
			if(p[i] < p[j])
			{
				o[MAX_CLONES] = o[i];
				o[i] = o[j];
				o[j] = o[MAX_CLONES];
				clones[MAX_CLONES] = clones[i];
				clones[i] = clones[j];
				clones[j] = clones[MAX_CLONES];
				p[MAX_CLONES] = p[i];
				p[i] = p[j];
				p[j] = p[MAX_CLONES];
				op[MAX_CLONES] = op[i];
				op[i] = op[j];
				op[j] = op[MAX_CLONES];
				break;
			}

	i = ii;
	user = clone[n].first_user;
	
    while(1)
    {
        /* op other clones */
        /* n = op counter */	
        if(px == -1)
        {
	        for(k = (user ? 2 : n_clones); k;)
	        {
	            if(n_job == MAX_JOBS) break;
	        
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
				        k--;
			        }
		        }

		        i++;
		        if(i == n_clones)
			        i = 0;
			        
		        if(i == ii)
		        {
		            px = 0;
		            break;
		        }
	        }
	    }

	    for(k = (mode == 1) ? 4 : 3; k && user; user = user->next)
	    {
		    if(n_job == MAX_JOBS) break;

		    if(user->clone == -1 && !user->op_list && user->op)
		    {
			    m = 1;
			    for(l = 0, j = 0; l < n_queue; j++)
			    {
				    if(queue[j].used && queue[j].type == mode &&
						    !strcmp(queue[j].nick,
						    user->nick))
					    {
						    m = 0;
						    break;
					    }

				    if(queue[j].used)
					    l++;
			    }

			    if(m)
			    {
				    job[n_job].type = mode;
				    job[n_job].nick = user->nick;
				    job[n_job++].clone = -1;
				    k--;
			    }
		    }
	    }
	    
	    if(n_job == MAX_JOBS) break;
	    
	    if(!px && !user)
	        break;	
	}

	for(user = clone[n].first_user; user; user = user->next)
	{
		if(n_job == MAX_JOBS) break;

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
		if(o[i] > MAX_CLONES)
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
					sprintf(tmp3, "KICK %s %s :\002%s\002\r\n"
							"KICK %s %s :\002%s\002\r\n",
							chan, tmp1, CLONER_NAME, chan,
							tmp2, CLONER_NAME);
				else
					sprintf(tmp3, "KICK %s %s :\002%s\002\r\n", chan,
							tmp1, CLONER_NAME);
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

