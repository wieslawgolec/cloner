
char *_memmem(void *vsp, size_t len1, void *vpp, size_t len2)
{
	char *sp = (char *) vsp, *pp = (char *) vpp;
	char *eos   = sp + len1 - len2;

	if(!(sp && pp && len1 && len2))	return NULL;

	while (sp <= eos)
	{
		if (*sp == *pp)
		if (memcmp(sp, pp, len2) == 0) return sp;
		sp++;
	}
    return NULL;
}

long int nanotime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_usec;
}
