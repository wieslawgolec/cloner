void generate_nick();
/* print usage */
void usage(char *prg);
/* die with an error message */
void error(char *format, ...);
void load_vhosts(char *filename);
void propaganda();
/* triming string functions */
char *ltrim(char *s);
char *rtrim(char *s);
char *trim(char *s);
#ifdef DEBUG
void debug(int n, char *format, ...);
#endif

/*
 * New clones loading/connecting system
 */
 
void init_clone(int n, struct clone_connection *conn);
void free_clone(int n);

