
#ifndef _PROCINFO_INCLUDED
#define	_PROCINFO_INCLUDED


#include <stdarg.h>

#define	PROXYNAME	"ftp.proxy"


#define	ERR_EXITCODEMASK	0x00000F
#define	ERR_ZEROEXITCODE	0x00000E
#define	ERR_ERRORMASK		0xFFFF00

#define	ERR_STDERR		0x000080

#define	ERR_ACCESS		0x000100
#define	ERR_CONNECT		0x000200
#define	ERR_TIMEOUT		0x000400
#define	ERR_SERVER		0x000800
#define	ERR_CLIENT		0x001000
#define	ERR_PROXY		0x002000
#define	ERR_OK			0x008000
#define	ERR_CONFIG		0x010000
#define	ERR_OTHER		0x020000
#define	ERR_SYSTEM		0x040000
#define	ERR_ANY			0xFFFFFF


typedef struct _procinfo {
    int		mainpid;
    char	pidfile[200];

    char	statfile[200];
    FILE	*statfp;

    char	exithandler[400];
    } procinfo_t;


extern char varprefix[40];
extern char statdir[200];
extern char sessiondir[200];

extern procinfo_t pi;


extern char *getpidfile();
extern char *getstatdir();
extern char *setpidfile(char *pidfile);
extern char *setstatdir(char *dir);

extern int init_procinfo(char *vp);
FILE *getstatfp(void);

extern char *get_exithandler(void);
extern char *set_exithandler(char *handler);
extern int run_exithandler(int error, char *line);

extern char *getvar(char *var);
extern int setvar(char *var, char *value);
extern int setnumvar(char *var, unsigned long val);

extern int writepidfile();
extern void exithandler(void);

#endif

