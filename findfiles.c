/*******************************************************************************
********************************************************************************

    findfiles: find files based on various selection criteria
    Copyright (C) 2015 James S. Crook

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************************
*******************************************************************************/

/*******************************************************************************
findfiles takes arguments in the form specified in the help message and searches
for objects in the file system (files, directories and "other") where:
- the names match the extended regular expression (pattern)
- have the specified modification time
- have the specified last access time
and present the result in the specified output format and order.

2015/06/30 James S. Crook		0.0.1  Initial version
*******************************************************************************/
#define PROGRAMVERSIONSTRING	"0.0.1"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <regex.h>


#define SECONDSPERMINUTE	60
#define MINUTESPERHOUR		60
#define HOURSPERDAY		24
#define SECONDSPERHOUR		(SECONDSPERMINUTE*MINUTESPERHOUR)
#define SECONDSPERDAY		(SECONDSPERMINUTE*MINUTESPERHOUR*HOURSPERDAY)
#define SECONDSPERWEEK		(SECONDSPERMINUTE*MINUTESPERHOUR*HOURSPERDAY*7)
#define SECONDSPERMONTH		(SECONDSPERMINUTE*MINUTESPERHOUR*HOURSPERDAY*30)  /* 30D month!!! */
#define SECODNSPERYEAR		(SECONDSPERMINUTE*MINUTESPERHOUR*HOURSPERDAY*365) /* 365D year!!! */
#define MINUTESPERDAY		(MINUTESPERHOUR*HOURSPERDAY)

#define MAXPATHLENGTH		2048
#define INITMAXNUMOBJS		4096
#define PATHDELIMITERCHAR	'/'
#define RELMODAGECHAR		'm'
#define REFMODAGECHAR		'M'
#define SECONDSUNITCHAR		's'
#define BYTESUNITCHAR		'B'
#define DEFAULTAGE		"0s"
#define DEFAULTERE		".*"
#define GETOPTSTR		"+dforia:m:p:A:M:hsuvRt:"
#define USAGEMESSAGE \
"usage (v %s):\n\
%s [-dforihsuvR] [-a|-m <age>] [-A|-M <path>] [-p <ERE>] <target> ...\n\
 Where:\n\
  <age>    is a +/- relative age value (integer or float) followed by a time unit\n\
  <path>   is the pathname of a refrenece object (file, directory, etc.)\n\
  <ERE>    is a POSIX-style Extended Regular Expression (pattern)\n\
  <target> is the pathname of an object (file, directory, etc.) to search\n\
 Options - can be toggled on/off (parsed left to right):\n\
  -d|--directories : directories   (default off)\n\
  -f|--files       : regular files (default off)\n\
  -o|--others      : other files   (default off)\n\
  -r|--recursive   : recursive - traverse file trees (default off)\n\
  -i|--ignore-case : case insensitive pattern match - invoke before -p option (default off)\n\
 Options requiring parameters (parsed left to right):\n\
  -a|--acc-age [-|+]<access age>       : time since last access (no default)\n\
  -m|--mod-age [-|+]<modification age> : time since last modification (default 0s: any time)\n\
  -p|--pattern <ERE>                   : POSIX-style Extended Regular Expression (pattern) (default '.*')\n\
  -A|--acc-ref [-|+]<acc ref path>     : accessed -before or [+]after (no default)\n\
  -M|--mod-ref [-|+]<mod ref path>     : modified -before or [+]after (no default)\n\
 Flags - are 'global' options (can NOT be toggled by setting multiple times):\n\
  -h|--help    : display this help message\n\
  -s|--seconds : display file ages in seconds (default D_hh:mm:ss)\n\
  -u|--units   : display units: s for seconds, B for Bytes (default off)\n\
  -v|--verbose : verbose output - modification times(s) & sizes(B) (default off)\n\
  -R|--reverse : Reverse the (time) order of the output (default off)\n\
 Time units:\n\
  s : seconds  m : minutes     h : hours\n\
  D : Days     W : Weeks (7D)  M : Months (30D)  Y : Years (365D)\n\
 Examples of command line arguments (parsed left to right):\n\
  -f /tmp                      # files in /tmp of any age, including future dates!\n\
  -f -m 1D -p '\\.ant$' /tmp    # files in /tmp ending in '.ant' modified >= 1 day ago\n\
  -fip a /tmp -ip b /var       # files named: /tmp/*a*, /tmp/*A* or /var/*b*\n\
  -rfa -3h src                 # files in the src tree accessed <= 3 hours ago\n\
  -dRp pat junk                # dirs in junk with 'pat' in the name - reverse sort\n\
  -rfM /etc/hosts /lib         # files in the /lib tree modified after /etc/hosts was\n\
  -fvm -3h / /tmp -fda 1h /var # files in / or /tmp modified <= 3 hours, and dirs (but\n\
                               # NOT files) in /var accessed >= 1h, verbose output\n\
\n\
findfiles Copyright (C) 2015 James S. Crook\n\
This program comes with ABSOLUTELY NO WARRANTY.\n\
This is free software, and you are welcome to redistribute it under certain conditions.\n\
This program is licensed under the terms of the GNU General Public License as published\n\
by the Free Software Foundation, either version 3 of the License, or (at your option) any\n\
later version (see <http://www.gnu.org/licenses/>).\n"

typedef struct {	/* storage of object names, ages, modification times & sizes */
    char   *name;
    time_t  age;
    time_t  time;
    off_t   size;
} Objectinfo;

static Objectinfo *objectinfotable;
static time_t	   starttime;

static int maxnumberobjects	= INITMAXNUMOBJS;
static int numobjsfound		= 0;
static int returncode		= 0;

/* Command line option flags - all set to false */
static int  recursiveflag	= 0;
static int  ignorecaseflag	= 0;
static int  regularfileflag	= 0;
static int  directoryflag	= 0;
static int  otherobjectflag	= 0;
static int  verboseflag		= 0;
static int  displaysecondsflag	= 0;
static int  accesstimeflag	= 0;
static int  sortmultiplier	= 1;
static char secondsunitchar	= ' ';
static char bytesunitchar	= ' ';

void processobject(char *, regex_t *, time_t);
void processdirectory(char *, regex_t *, time_t);

/*******************************************************************************
Display the usage (help) message.
*******************************************************************************/
usagemessage(char *progname) {
    printf(USAGEMESSAGE, PROGRAMVERSIONSTRING, progname);
}


/*******************************************************************************
Strip any trailing '/' characters - because of a bug in some versions of lstat.
lstat("symlinktodir",  &buf) returns that symlink is a symbolic link - correct!
lstat("symlinktodir/", &buf) returns that symlink is a directory!!! - INcorrect!
*******************************************************************************/
trimtrailingslashes(char *pathname) {
    char	*chptr;
    int		len;

    len = strlen(pathname);
    if (len > 1) {	/* handle the special case of '/' correctly */
	chptr = pathname+len-1;
	while (chptr >= pathname && *chptr == PATHDELIMITERCHAR) {
	    *chptr-- = '\0';
	}
    }
}


/*******************************************************************************
Process a (file system) pathname (a file, directory or "other").
*******************************************************************************/
void processpath(char *pathname, regex_t *extregexpptr, time_t targetage, int toplevelflag) {
    struct stat statinfo;

    if (!regularfileflag && !directoryflag && !otherobjectflag) {
	fprintf(stderr, "No output target types requested for '%s'!\n",pathname);
	returncode = 1;
	return;
    }

    if (lstat(pathname, &statinfo) == -1) {
	fprintf(stderr, "Cannot access '%s' (processpath)\n", pathname);
	returncode = 1;
	return;
    }

    if (S_ISREG(statinfo.st_mode)) {
	if (regularfileflag) {
	    processobject(pathname, extregexpptr, targetage);
	}
    } else if (S_ISDIR(statinfo.st_mode)) {
	if (directoryflag) {
	    processobject(pathname, extregexpptr, targetage);
	}
	if (recursiveflag || toplevelflag) {
	    processdirectory(pathname, extregexpptr, targetage);
	}
    } else if (otherobjectflag) {
	processobject(pathname, extregexpptr, targetage);
    }
}


/*******************************************************************************
Process a (file system) object - eg, a regular file, directory, symbolic
link, fifo, special file, etc.  If the object's attributes satisfy the command
line arguments (i.e., the name matches the extended regular expression (pattern),
the access xor modification time, etc. then, this object is appended to the
objectinfotable. If objectinfotable is full, its size is dynamically doubled.
*******************************************************************************/
void processobject(char *pathname, regex_t *extregexpptr, time_t targetage) {
    struct	stat statinfo;
    char	objectname[MAXPATHLENGTH], *chptr;
    int		objectage;

    /* extract the object name after the last '/' char */
    if (((chptr=strrchr(pathname, PATHDELIMITERCHAR)) != NULL) && *(chptr+1) != '\0'){
	strcpy(objectname, chptr+1);
    } else {
	strcpy(objectname, pathname);
    }

    if (regexec(extregexpptr, objectname, (size_t)0, NULL, 0) == 0) {
	if (lstat(pathname, &statinfo) == -1) {
	    fprintf(stderr, "Cannot access '%s' (processobject)\n", pathname);
	    returncode = 1;
	    return;
	}

	if (accesstimeflag) {
	    objectage = starttime - statinfo.st_atime;
	} else {
	    objectage = starttime - statinfo.st_mtime;
	}
	if (targetage == 0 || (targetage > 0 && objectage >=  targetage) ||
			      (targetage < 0 && objectage <= -targetage)) {

	    if (numobjsfound >= maxnumberobjects) {
		maxnumberobjects *= 2;
		if ((objectinfotable=realloc(objectinfotable, maxnumberobjects*sizeof(Objectinfo))) == NULL) {
		    perror("realloc failed");
		    exit(2);
		}
	    }

	    if ((objectinfotable[numobjsfound].name=malloc(strlen(pathname)+1)) == NULL) {
		perror("malloc failed");
		exit(2);
	    }
	    strcpy(objectinfotable[numobjsfound].name, pathname);
	    objectinfotable[numobjsfound].age = objectage;
	    objectinfotable[numobjsfound].size = statinfo.st_size;
	    if (accesstimeflag) {
		objectinfotable[numobjsfound].time = statinfo.st_atime;
	    } else {
		objectinfotable[numobjsfound].time = statinfo.st_mtime;
	    }
	    numobjsfound++;
	}
    }
}


/*******************************************************************************
Process a directory. Open it, read all it's entries (objects) and call processpath
for each one (EXCEPT '.' and '..') and close it.
*******************************************************************************/
void processdirectory(char *pathname, regex_t *extregexpptr, time_t targetage) {
    DIR		  *dirptr;
    struct dirent *direntptr;
    char	   newpathname[MAXPATHLENGTH];

    if ((dirptr=opendir(pathname)) == (DIR*)NULL) {
	fprintf(stderr, "opendir error");
	perror(pathname);
	returncode = 1;
	return;
    }

    while ((direntptr=readdir(dirptr)) != (struct dirent *)NULL) {
	if (strcmp(direntptr->d_name, ".") && strcmp(direntptr->d_name, "..")) {
	    /* create newpathname from pathname/objectname */
	    sprintf(newpathname, "%s%c%s", pathname, PATHDELIMITERCHAR, direntptr->d_name);
	    processpath(newpathname, extregexpptr, targetage, 0);
	}
    }

    if (closedir(dirptr)) {
	perror(pathname);
	returncode = 1;
    }
}


/*******************************************************************************
Comparison function for the qsort call (of objectinfotable) in function listobjects.
*******************************************************************************/
static int compareobjectinfo(Objectinfo *firstptr, Objectinfo *secondptr) {
    if (firstptr->age != secondptr->age) {
	return (firstptr->age - secondptr->age)*sortmultiplier;
    } else {
	return (strcmp(firstptr->name, secondptr->name))*sortmultiplier;
    }
}


/*******************************************************************************
Sort objectinfotable by age, and display the object names.
*******************************************************************************/
static void listobjects() {
    struct tm	*localtimeinfoptr;
    time_t	 objectage, absobjectage, days, hrs, mins, secs; 
    int		 foundidx;
    char	 objectagestr[32], *chptr;

    qsort((void*)objectinfotable, (size_t)numobjsfound, (size_t)sizeof(Objectinfo), (void *)compareobjectinfo);
    for (foundidx=0; foundidx<numobjsfound; foundidx++) {
	if (verboseflag) {

	    localtimeinfoptr = localtime(&objectinfotable[foundidx].time);
	    printf("%04d%02d%02d_%02d%02d%02d", localtimeinfoptr->tm_year+1900,
		localtimeinfoptr->tm_mon+1, localtimeinfoptr->tm_mday, localtimeinfoptr->tm_hour,
		localtimeinfoptr->tm_min, localtimeinfoptr->tm_sec);

	    objectage = objectinfotable[foundidx].age;
	    if (displaysecondsflag) {
		printf(" %15d%c ", objectage, secondsunitchar);
	    } else {
		absobjectage = abs(objectage);
		days = absobjectage/SECONDSPERDAY;
		hrs = absobjectage/SECONDSPERHOUR - days*HOURSPERDAY;
		mins = absobjectage/SECONDSPERMINUTE - days*MINUTESPERDAY - hrs*MINUTESPERHOUR;
		secs = absobjectage % 60;
		sprintf(objectagestr," %6dD_%02d:%02d:%02d ", days, hrs, mins, secs);

		/* if objectage is negative (future timestamp), display a - sign */
		if (objectage < 0) {
		    chptr = objectagestr;
		    while (*chptr && *chptr == ' ') {
			chptr++;
		    }
		    *(--chptr) = '-';
		}
		printf("%s", objectagestr);
	    }
	    printf(" %14llu%c  ", objectinfotable[foundidx].size, bytesunitchar);
	}
	printf("%s\n", objectinfotable[foundidx].name);
    }
}


/*******************************************************************************
Set the (target) object (file, etc.) relative age. This age is relative to
starttime (the time the program starts). This relative age may be used for
either the last modification time or the last access time - depending upon the
command line argument(s).
*******************************************************************************/
static time_t setrelativeage(char *targetagestr, char c) {
    float	scaledage;
    char	timeunitch;
    time_t	targetage;

    if (c == RELMODAGECHAR) {
	accesstimeflag = 0;
    } else {
	accesstimeflag = 1;
    }

    scaledage = atof(targetagestr); /* ignores units chars */
    timeunitch = *(targetagestr+strlen(targetagestr+1));

    switch (timeunitch) {
	case 's': targetage = (int)(scaledage);				break;
	case 'm': targetage = (int)(scaledage*SECONDSPERMINUTE);	break;
	case 'h': targetage = (int)(scaledage*SECONDSPERHOUR);		break;
	case 'D': targetage = (int)(scaledage*SECONDSPERDAY);		break;
	case 'W': targetage = (int)(scaledage*SECONDSPERWEEK);		break;
	case 'M': targetage = (int)(scaledage*SECONDSPERMONTH);		break;
	case 'Y': targetage = (int)(scaledage*SECODNSPERYEAR);		break;
	default:  fprintf(stderr, "Illegal time unit '%c'\n", timeunitch);
	    exit(2);
    }
    return targetage;
}


/*******************************************************************************
Set the (target) reference age to be that of the reference file/object. This age
may be used for either the last modification time or the last access time -
depending upon the command line argument(s).
*******************************************************************************/
static time_t setreferenceage(char *targetagestr, char c) {
    char	*filenameptr;
    time_t	 targetage;
    struct stat	 statinfo;

    filenameptr = targetagestr;
    if (*filenameptr && *filenameptr == '-') {
	filenameptr++;
    } else if (*filenameptr && *filenameptr == '+') {
	filenameptr++;
    }

    if (*filenameptr && (lstat(filenameptr, &statinfo) != -1)) {
	if (c == REFMODAGECHAR) {
	    accesstimeflag = 0;
	    targetage = starttime - statinfo.st_mtime;
	} else {
	    accesstimeflag = 1;
	    targetage = starttime - statinfo.st_mtime;
	}
    } else {
	fprintf(stderr, "Cannot access '%s'\n", filenameptr);
	exit(2);
    }
    if (*targetagestr && *targetagestr != '-') {
	return -targetage+1;	/* +1s for OLDER than (NOT >= age!) */
    } else {
	return targetage+1;	/* +1s for YOUNGER than (NOT <= age!) */
    }
}


/*******************************************************************************
Set the extended regular expression (pattern) to be used to match the object names.
*******************************************************************************/
static void setextregexp(char *extregexpstr, regex_t *extregexpptr) {
    int cflags;

    if (ignorecaseflag) {
	cflags = REG_EXTENDED|REG_ICASE;
    } else {
	cflags = REG_EXTENDED;
    }

    if (regcomp(extregexpptr, extregexpstr, cflags) != 0) {
	perror("regcomp error");
    }
}


/*******************************************************************************
Replace (overwrite!) a long format command line ("cl") option (argv[c]) with its
short format equivalent. E.g., replace '--files' with '-f' and '--pattern=foo'
with '-pfoo'. Note: getopt_long doesn't process arguments in left-to-right order.
*******************************************************************************/
cmdlinelongtoshort(char *longopt) {
    typedef struct {
	char *shortform;
	char *longform;
    } Optiontype;

    Optiontype optiontable[] = {
	"-d", "--directories",	"-f", "--files",	"-o", "--others",
	"-r", "--recursive",	"-i", "--ignore-case",	"-a", "--acc-age",
	"-m", "--mod-age",	"-p", "--pattern",	"-A", "--acc-ref",
	"-M", "--mod-ref",	"-h", "--help",		"-s", "--seconds",
	"-u", "--units",	"-v", "--verbose",	"-R", "--reverse",
    };
    int	optiontableidx, optionlength, optionfoundflag = 0;

    for (optiontableidx=0; optiontableidx<sizeof(optiontable)/sizeof(Optiontype); optiontableidx++) {
	optionlength=strlen(optiontable[optiontableidx].longform);

	/* if '--longopt' with or without something following (eg, '--longopt=<param>' */
	if (!strncmp(longopt, optiontable[optiontableidx].longform, optionlength)) {
	    /* if exactly '--longopt' with nothing following */
	    if (*(longopt+optionlength) == '\0') {
		memmove(longopt, optiontable[optiontableidx].shortform, 3);
		optionfoundflag = 1;
	    /* if exactly '--longopt=<param>', and NOT exactly '--longopt=' (<param> missing) */
	    } else if (strlen(longopt) > optionlength+1 && *(longopt+optionlength) == '=') {
		*(longopt+1) = *(optiontable[optiontableidx].shortform+1);
		memmove(longopt+2, longopt+optionlength+1, strlen(longopt+optionlength+1)+1);
		optionfoundflag = 1;
	    }
	}
    }
    /* if '--bogus_option' or '--valid_option=' was found */
    if (!optionfoundflag) {
	printf("Illegal command line option '%s', aborting\n", longopt);
	exit(1);
    }
}


/*******************************************************************************
Parse the command line arguments left to right, processing them in order.  See
the usage message.
*******************************************************************************/
main(int argc, char *argv[]) {
    extern char *optarg;
    extern int	 optind, optopt, opterr;
    int		 c;
    regex_t	 extregexp;
    time_t	 targetage = 0;	/* default */

    if (argc <= 1) {
	usagemessage(argv[0]);
	exit(2);
    }

    if ((objectinfotable=(Objectinfo*)calloc(INITMAXNUMOBJS, sizeof(Objectinfo))) == NULL) {
	perror("Could not calloc initial object info table");
	exit(2);
    }

    for (c=1; c<argc; c++) {		/* loop through all the command line arguments */
	if (!strncmp(argv[c], "--", 2)) {	/* is this one a long (--) argument? */
	    cmdlinelongtoshort(argv[c]);	/* overwrite a long arg with a short one */
	}
    }

    starttime = time(NULL); /* the current time - in seconds */
    targetage = setrelativeage(DEFAULTAGE, RELMODAGECHAR);	/* set default */
    setextregexp(DEFAULTERE, &extregexp);			/* set default */

    /* Note: -t option does not appear in the usage message! "undocumented"!
    it can be used if it makes you happy: -t <target>, but the behavior is
    the same with or without it */
    while (optind < argc) {
	while ((c = getopt(argc, argv, GETOPTSTR)) != -1) {
	    switch(c) {
		case 'd': directoryflag    = !directoryflag;			break;
		case 'f': regularfileflag  = !regularfileflag;			break;
		case 'o': otherobjectflag  = !otherobjectflag;			break;
		case 'r': recursiveflag    = !recursiveflag;			break;
		case 'i': ignorecaseflag   = !ignorecaseflag;			break;
		case 'a': targetage = setrelativeage(optarg, c);		break;
		case 'm': targetage = setrelativeage(optarg, c);		break;
		case 'p': setextregexp(optarg, &extregexp);			break;
		case 'A': targetage = setreferenceage(optarg, c);		break;
		case 'M': targetage = setreferenceage(optarg, c);		break;
		case 'h': usagemessage(argv[0]); exit(0);			break;
		case 's': displaysecondsflag  = 1;				break;
		case 'u': secondsunitchar = SECONDSUNITCHAR;
			  bytesunitchar = BYTESUNITCHAR;			break;
		case 'v': verboseflag    = 1;					break;
		case 'R': sortmultiplier = -1;					break;
		case 't': processpath(optarg, &extregexp, targetage, 1);	break;
	    }
	}
	if (optind < argc) {
	    trimtrailingslashes(argv[optind]);
	    processpath(argv[optind], &extregexp, targetage, 1);
	    optind++;
	}
    }

    listobjects();
    return returncode;
}
