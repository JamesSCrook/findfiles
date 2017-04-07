/*******************************************************************************
********************************************************************************

    findfiles: find files based on various selection criteria
    Copyright (C) 2016-2017 James S. Crook

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
********************************************************************************
*******************************************************************************/

/*******************************************************************************
findfiles searches Linux/UNIX file systems for objects (files, directories and
"other") and lists them in sorted order of last modification and/or last access.

The behavior is controlled by command line arguments as follows:
 1. arguments are processed left-to-right
 2. only specified target(s) are searched
 3. (optional) selection by last modification age(s)
 4. (optional) selection by last access age(s)
 5. (optional) selection by object name pattern matching using ERE(s)
See the usage/help message for additional options.

findfiles sets starttime to the current system time when it starts. Both of the
optional age of last modification ('-m') and age of last access ('-a')
calculate a targettime relative to startime - i.e., an age. Note that targettime
is always earlier (smaller than) starttime. I.e., targettime cannot be in the
future.

For example:
 "-fm -10m" : find files modified <= 10 minutes ago (modified after targettime)
 "-fm  10m" : find files modified >= 10 minutes ago (modified before targettime)
Note that in both cases, targettime is 10 minutes before starttime! So, the
numerical value and unit ("10m", in both cases above) sets targettime, and "-" 
causes findfiles to list objects last modified/accessed more recently ("newer")
than targettime.

The optional last modificaton reference object ("-M") and last access reference
object ("-A") use the minus sign ("-") in the same way as "-m" and "-a". I.e.,
 "-fA -ref_file" : find files accessed after ref_file was (after targettime)
 "-fA  ref_file" : find files accessed before ref_file was (before targettime)

Here is a time line with time increasing to the right:

				 targettime			starttime
				 v				v
-------------olderthanttargettimeInewerthanttargettime---------------> -m & -a
------------olderthanttargettime) (newerthanttargettime--------------> -M & -A

Note that "-m" and "-a" use <= and/or >=, but, "-M" and "-A" use < and/or >!

It is assumed that, in general, the cases of file system objects having future
last access and/or last modification times are both rare and uninteresting.
*******************************************************************************/
#define PROGRAMVERSIONSTRING	"1.0.0"		/* 2017/04/07 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <locale.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>


#define SECONDSPERMINUTE	60
#define MINUTESPERHOUR		60
#define HOURSPERDAY		24
#define MAXOBJAGESTRLEN		32
#define TMBASEMONTH		1
#define TMBASEYEAR		1900
#define SECONDSPERHOUR		(SECONDSPERMINUTE*MINUTESPERHOUR)
#define SECONDSPERDAY		(SECONDSPERMINUTE*MINUTESPERHOUR*HOURSPERDAY)
#define SECONDSPERWEEK		(SECONDSPERMINUTE*MINUTESPERHOUR*HOURSPERDAY*7)
#define MINUTESPERDAY		(MINUTESPERHOUR*HOURSPERDAY)

#define MAXDATESTRLENGTH	64
#define MAXPATHLENGTH		2048
#define INITMAXNUMOBJS		4096
#define PATHDELIMITERCHAR	'/'
#define RELMODAGECHAR		'm'
#define REFMODAGECHAR		'M'
#define SECONDSUNITCHAR		's'
#define BYTESUNITCHAR		'B'
#define DEFAULTAGE		0
#define DEFAULTERE		".*"

#define GETOPTSTR		"+dforia:m:p:A:M:hsuvRt:"
#define USAGEMESSAGE \
"usage (version %s):\n\
%s [OPTION]... [target|-t target]... [OPTION]... [target|-t target]...\n\
 Some OPTIONs require arguments. These are:\n\
  age    : a +/- relative age value followed by a time unit\n\
  ERE    : a POSIX-style Extended Regular Expression (pattern)\n\
  path   : the pathname of a reference object (file, directory, etc.)\n\
  target : the pathname of an object (file, directory, etc.) to search\n\
 OPTIONs - can be toggled on/off (parsed left to right):\n\
  -d|--directories : directories   (default off)\n\
  -f|--files       : regular files (default off)\n\
  -o|--others      : other files   (default off)\n\
  -r|--recursive   : recursive - traverse file trees (default off)\n\
  -i|--ignore-case : case insensitive pattern match - invoke before -p option (default off)\n\
 OPTIONs requiring an argument (parsed left to right):\n\
  -a|--acc-age [-|+]access_age       : - for newer/=, [+] for older/= ages (no default)\n\
  -m|--mod-age [-|+]modification_age : - for newer/=, [+] for older/= ages (default 0s: any time)\n\
  -p|--pattern ERE                   : POSIX-style Extended Regular Expression (pattern) (default '.*')\n\
  -A|--acc-ref [-|+]acc_ref_path     : - for newer, [+] for older ages (no default)\n\
  -M|--mod-ref [-|+]mod_ref_path     : - for newer, [+] for older ages (no default)\n\
  -t|--target target_path            : target path (no default)\n\
 Flags - are 'global' options (can NOT be toggled by setting multiple times):\n\
  -h|--help    : display this help message\n\
  -s|--seconds : display file ages in seconds (default D_hh:mm:ss)\n\
  -u|--units   : display units: s for seconds, B for Bytes (default off)\n\
  -R|--reverse : Reverse the (time) order of the output (default off)\n\
 Verbosity: (May be specified more than once for additional information.)\n\
  -v|--verbosity : also display modification time, age & size(B) (default 0[off])\n\
 Time units:\n\
  Y: Years    M: Months     W: Weeks      D: Days\n\
  h: hours    m: minutes    s: seconds\n\
  Note: Specify Y, M & s with integer values. W, D, h & m can also take floating point values.\n\
 Examples of command line arguments (parsed left to right):\n\
  -f /tmp                      # files in /tmp of any age, including future dates!\n\
  -f -m 1D -p '\\.ant$' /tmp    # files in /tmp ending in '.ant' modified >= 1 day ago\n\
  -fip a /tmp -ip b /var       # files named /tmp/*a*, /tmp/*A* or /var/*b*\n\
  -rfa -3h src                 # files in the src tree accessed <= 3 hours ago\n\
  -dRp pat junk                # directories named junk/*pat* - reverse sort\n\
  -rfM -/etc/hosts /lib        # files in the /lib tree modified after /etc/hosts was\n\
  -fvm -3h / /tmp -fda 1h /var # files in / or /tmp modified <= 3 hours, and dirs (but\n\
                               # NOT files) in /var accessed >= 1h, verbose output\n\
\n\
findfiles Copyright (C) 2016-2017 James S. Crook\n\
This program comes with ABSOLUTELY NO WARRANTY.\n\
This is free software, and you are welcome to redistribute it under certain conditions.\n\
This program is licensed under the terms of the GNU General Public License as published\n\
by the Free Software Foundation, either version 3 of the License, or (at your option) any\n\
later version (see <http://www.gnu.org/licenses/>).\n"

typedef struct {	/* storage of object names, ages, modification times & sizes */
    char	  *name;
    time_t	  age;
    time_t	  time;
    off_t	  size;
} Objectinfo;

Objectinfo	*objectinfotable;
time_t		starttime;

int	maxnumberobjects	= INITMAXNUMOBJS;
int	numobjsfound		= 0;
int	numtargets		= 0;
int	returncode		= 0;

/* Command line option flags - all set to false */
int	recursiveflag		= 0;
int	ignorecaseflag		= 0;
int	regularfileflag		= 0;
int	directoryflag		= 0;
int	otherobjectflag		= 0;
int	verbosity		= 0;
int	displaysecondsflag	= 0;
int	accesstimeflag		= 0;
int	newerthantargetflag	= 0;
int	sortmultiplier		= 1;
char	secondsunitchar		= ' ';
char	bytesunitchar		= ' ';

void process_directory(char *, regex_t *, time_t);


/*******************************************************************************
Display the usage (help) message.
*******************************************************************************/
void display_usage_message(char *progname) {
    printf(USAGEMESSAGE, PROGRAMVERSIONSTRING, progname);
}


/*******************************************************************************
Process a (file system) object - eg, a regular file, directory, symbolic
link, fifo, special file, etc.  If the object's attributes satisfy the command
line arguments (i.e., the name matches the extended regular expression (pattern),
the access xor modification time, etc. then, this object is appended to the
objectinfotable. If objectinfotable is full, its size is dynamically doubled.
*******************************************************************************/
void process_object(char *pathname, regex_t *extregexpptr, time_t targettime) {
    struct	stat statinfo;
    char	objectname[MAXPATHLENGTH], *chptr;
    int		objecttime;

    /* extract the object name after the last '/' char */
    if (((chptr=strrchr(pathname, PATHDELIMITERCHAR)) != NULL) && *(chptr+1) != '\0'){
	strcpy(objectname, chptr+1);
    } else {
	strcpy(objectname, pathname);
    }

    if (regexec(extregexpptr, objectname, (size_t)0, NULL, 0) == 0) {
	if (lstat(pathname, &statinfo) == -1) {
	    fprintf(stderr, "process_object: Cannot access '%s'\n", pathname);
	    returncode = 1;
	    return;
	}

	if (accesstimeflag) {
	    objecttime = statinfo.st_atime;
	} else {
	    objecttime = statinfo.st_mtime;
	}

	if (targettime == 0 || (newerthantargetflag && objecttime >= targettime) ||
			      (!newerthantargetflag && objecttime <= targettime)) {

	    if (numobjsfound >= maxnumberobjects) {
		maxnumberobjects *= 2;
		if ((objectinfotable=realloc(objectinfotable, maxnumberobjects*sizeof(Objectinfo))) == NULL) {
		    perror("realloc failed");
		    exit(1);
		}
	    }

	    if ((objectinfotable[numobjsfound].name=malloc(strlen(pathname)+1)) == NULL) {
		perror("malloc failed");
		exit(1);
	    }
	    strcpy(objectinfotable[numobjsfound].name, pathname);
	    objectinfotable[numobjsfound].age = starttime - objecttime;
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
Strip any trailing '/' characters - because of a bug in some versions of lstat.
lstat("symlinktodir",  &buf) returns that symlink is a symbolic link - correct!
lstat("symlinktodir/", &buf) returns that symlink is a directory!!! - INcorrect!
*******************************************************************************/
void trim_trailing_slashes(char *pathname) {
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
void process_path(char *pathname, regex_t *extregexpptr, time_t targettime, int toplevelflag) {
    struct stat	statinfo;

    if (toplevelflag) {
	trim_trailing_slashes(pathname);
    }

    if (!regularfileflag && !directoryflag && !otherobjectflag) {
	fprintf(stderr, "No output target types requested for '%s'!\n",pathname);
	returncode = 1;
	return;
    }

    if (lstat(pathname, &statinfo) == -1) {
	fprintf(stderr, "process_path: Cannot access '%s'\n", pathname);
	returncode = 1;
	return;
    }

    if (S_ISREG(statinfo.st_mode)) {
	if (regularfileflag) {
	    process_object(pathname, extregexpptr, targettime);
	}
    } else if (S_ISDIR(statinfo.st_mode)) {
	if (directoryflag) {
	    process_object(pathname, extregexpptr, targettime);
	}
	if (recursiveflag || toplevelflag) {
	    process_directory(pathname, extregexpptr, targettime);
	}
    } else if (otherobjectflag) {
	process_object(pathname, extregexpptr, targettime);
    }
}


/*******************************************************************************
Process a directory. Open it, read all it's entries (objects) and call
process_path for each one (EXCEPT '.' and '..') and close it.
*******************************************************************************/
void process_directory(char *pathname, regex_t *extregexpptr, time_t targettime) {
    DIR		  *dirptr;
    struct dirent *direntptr;
    char	   newpathname[MAXPATHLENGTH];
    char	   dirpath[MAXPATHLENGTH];

    if (strcmp(pathname, "/")) {
	sprintf(dirpath, "%s%c", pathname, PATHDELIMITERCHAR);  /* not "/" */
    } else {
	sprintf(dirpath, "%c", PATHDELIMITERCHAR);	    /* pathname is "/" */
    }

    if ((dirptr=opendir(pathname)) == (DIR*)NULL) {
	fprintf(stderr, "opendir error");
	perror(pathname);
	returncode = 1;
	return;
    }

    while ((direntptr=readdir(dirptr)) != (struct dirent *)NULL) {
	if (strcmp(direntptr->d_name, ".") && strcmp(direntptr->d_name, "..")) {
	    /* create newpathname from pathname/objectname */
	    sprintf(newpathname, "%s%s", dirpath, direntptr->d_name);
	    process_path(newpathname, extregexpptr, targettime, 0);
	}
    }

    if (closedir(dirptr)) {
	perror(pathname);
	returncode = 1;
    }
}


/*******************************************************************************
Comparison function for the qsort call (of objectinfotable) in function list_objects.
*******************************************************************************/
int compare_object_info(const void *firstptr, const void *secondptr) {
    const Objectinfo	*firstobjinfoptr = firstptr;	/* to keep gcc happy */
    const Objectinfo	*secondobjinfoptr = secondptr;

    if (firstobjinfoptr->age != secondobjinfoptr->age) {
	return (firstobjinfoptr->age - secondobjinfoptr->age)*sortmultiplier;
    } else {
	return (strcmp(firstobjinfoptr->name, secondobjinfoptr->name))*sortmultiplier;
    }
}


/*******************************************************************************
Sort objectinfotable by age, and display the object names.
*******************************************************************************/
void list_objects() {
    struct tm	*localtimeinfoptr;
    time_t	 objectage, absobjectage, days, hrs, mins, secs;
    int		 foundidx;
    char	 objectagestr[MAXOBJAGESTRLEN], *chptr;

    qsort((void*)objectinfotable, (size_t)numobjsfound, (size_t)sizeof(Objectinfo), compare_object_info);
    for (foundidx=0; foundidx<numobjsfound; foundidx++) {
	if (verbosity > 0) {

	    localtimeinfoptr = localtime(&objectinfotable[foundidx].time);
	    printf("%04d%02d%02d_%02d%02d%02d", localtimeinfoptr->tm_year+TMBASEYEAR,
		localtimeinfoptr->tm_mon+TMBASEMONTH, localtimeinfoptr->tm_mday,
		localtimeinfoptr->tm_hour, localtimeinfoptr->tm_min,
		localtimeinfoptr->tm_sec);

	    objectage = objectinfotable[foundidx].age;
	    if (displaysecondsflag) {
		printf(" %15ld%c ", objectage, secondsunitchar);
	    } else {
		absobjectage = abs(objectage);
		days = absobjectage/SECONDSPERDAY;
		hrs = absobjectage/SECONDSPERHOUR - days*HOURSPERDAY;
		mins = absobjectage/SECONDSPERMINUTE - days*MINUTESPERDAY - hrs*MINUTESPERHOUR;
		secs = absobjectage % SECONDSPERMINUTE;
		sprintf(objectagestr," %6ldD_%02ld:%02ld:%02ld ", days, hrs, mins, secs);

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
	    printf(" %14lu%c  ", objectinfotable[foundidx].size, bytesunitchar);
	}
	printf("%s\n", objectinfotable[foundidx].name);
    }

    if (numtargets == 0 && verbosity > 1) {
	fprintf(stderr, "Warning: no targets were specified on the command line!\n");
    }
}


/*******************************************************************************
Time resolution is 1 second, so non-integer 's' (second) time values generate a
warning. Non-integer time values used with units 'M' (months) or 'Y' (years) also
produce a warning because months and years vary in size. E.g., '0.5M' does NOT
always equate to the same number of seconds. (But 0.25h, 0.5m, 0.1W, etc. do.)
*******************************************************************************/
void check_integer(char *targetagestr) {
    char	*chptr;

    for (chptr=targetagestr; chptr<targetagestr+strlen(targetagestr)-1; chptr++) {
	if (!isdigit(*chptr) && *chptr != '-' && *chptr != '+' ) {
	    fprintf(stderr, "Warning: non-integer character '%c' in '%s'!\n",
							    *chptr, targetagestr);
	}
    }
}


/*******************************************************************************
Set the target time by subtracting the (relative) "age" command line argument,
e.g., "15D" from starttime. Note: targettime will always be less than starttime.
I.e., "-30s" and "[+]30s" both result in targettime = starttime-30, but the
minus sign is used to set/clear newerthantargetflag. This function is called for
both last access time and last modification time.
*******************************************************************************/
time_t set_target_time_by_relative_age(char *targetagestr, char c) {
    char	timeunitchar;
    time_t	targettime;
    struct tm	*brkdwntimeptr;
    char	datestr[MAXDATESTRLENGTH];

    if (c == RELMODAGECHAR) {
	accesstimeflag = 0;
    } else {
	accesstimeflag = 1;
    }

    if (*targetagestr == '-' ) {
	/* eg, "-m -15D" find objects modified <= 15 days ago (newer than) */
	newerthantargetflag = 1;
	targetagestr++;
    } else {
	/* eg, "-m [+]15D" find objects modified >= 15 days ago (older than) */
	newerthantargetflag = 0;
    }

    timeunitchar = *(targetagestr+strlen(targetagestr+1));
    brkdwntimeptr = localtime(&starttime);
    if (verbosity > 1) {
	strftime(datestr, MAXDATESTRLENGTH, "%c", brkdwntimeptr);
	fprintf(stderr, "i: start time:  %14lds = %s\n", starttime, datestr);
    }

    switch (timeunitchar) {
	case 's': check_integer(targetagestr);
	    brkdwntimeptr->tm_sec -= atoi(targetagestr);
	    break;
	case 'm': brkdwntimeptr->tm_sec -= atof(targetagestr)*SECONDSPERMINUTE;
	    break;
	case 'h': brkdwntimeptr->tm_sec -= atof(targetagestr)*SECONDSPERHOUR;
	    break;
	case 'D': brkdwntimeptr->tm_sec -= atof(targetagestr)*SECONDSPERDAY;
	    break;
	case 'W': brkdwntimeptr->tm_sec -= atof(targetagestr)*SECONDSPERWEEK;
	    break;
	case 'M': check_integer(targetagestr);
	    brkdwntimeptr->tm_mon -= atoi(targetagestr);
	    break;
	case 'Y': check_integer(targetagestr);
	    brkdwntimeptr->tm_year -= atoi(targetagestr);
	    break;
	default:  fprintf(stderr, "Illegal time unit '%c'\n", timeunitchar);
	    exit(1);
    }

    targettime = mktime(brkdwntimeptr);
    if (verbosity > 1) {
	strftime(datestr, MAXDATESTRLENGTH, "%c", brkdwntimeptr);
	fprintf(stderr, "i: target time: %14lds = %s\n", targettime, datestr);

	fprintf(stderr, "i: %13.5fD = %10lds last %s %s target time\n",
		(float)(starttime-targettime)/SECONDSPERDAY,
		starttime-targettime,
		accesstimeflag ? "accessed" : "modified",
		newerthantargetflag ? "after (newer than)" : "before (older than)");
    }
    return targettime;
}


/*******************************************************************************
Set targettime to be the same as that of the reference object's last modification
or last access time, as required. 
*******************************************************************************/
time_t set_target_time_by_object_time(char *targetobjectstr, char c) {
    time_t	 targettime;
    struct stat	 statinfo;

    newerthantargetflag = 0;
    /* eg, "-M [+]foo" find objects last modified BEFORE foo was (OLDER than) */
    if (*targetobjectstr == '-') {
	/* eg, "-M -foo" find objects last modified AFTER foo was (NEWER than) */
	newerthantargetflag = 1;
	targetobjectstr++;
    } else if (*targetobjectstr == '+') {
	targetobjectstr++;
    }
    if (verbosity > 1) {
	fprintf(stderr, "i: last %s %s than '%s'\n",
		accesstimeflag ? "accessed" : "modified",
		newerthantargetflag ? "after (newer than)" : "before (older than)",
		targetobjectstr);
    }

    if (*targetobjectstr && (lstat(targetobjectstr, &statinfo) != -1)) {
	if (c == REFMODAGECHAR) {
	    accesstimeflag = 0;
	    targettime = statinfo.st_mtime;
	} else {
	    accesstimeflag = 1;
	    targettime = statinfo.st_mtime;
	}
    } else {
	fprintf(stderr, "Cannot access '%s'\n", targetobjectstr);
	exit(1);
    }
    if (newerthantargetflag) {
	return targettime+1;	/* +1s for NEWER than (NOT the same age!) */
    } else {
	return targettime-1;	/* -1s for OLDER than (NOT the same age!) */
    }
}


/*******************************************************************************
Set the extended regular expression (pattern) to be used to match the object names.
*******************************************************************************/
void set_extended_regular_expression(char *extregexpstr, regex_t *extregexpptr) {
    int		cflags;

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
Replace (overwrite!) a long format command line option (argv[c]) with its short
format equivalent. E.g., replace '--files' with '-f' and '--pattern=foo' with
'-pfoo'. Note: getopt_long doesn't process arguments in left-to-right order!
Below, --longopt only requires enough of the first part (e.g., --long) to be
uninque (à la getopt_long).
*******************************************************************************/
void command_line_long_to_short(char *longopt) {
    char	*equalptr;
    unsigned	optiontableidx;
    int		optionfoundflag = 0;
    char	*optiontblcharptr, *longoptcharptr;

    typedef struct {
	char *shortform;
	char *longform;
	int  minuniqlen;
    } Optiontype;

    Optiontype optiontable[] = {
	{ "-a", "--acc-age"    , 7 },
	{ "-A", "--acc-ref"    , 7 },
	{ "-d", "--directories", 3 },
	{ "-f", "--files"      , 3 },
	{ "-h", "--help"       , 3 },
	{ "-i", "--ignore-case", 3 },
	{ "-m", "--mod-age"    , 7 },
	{ "-M", "--mod-ref"    , 7 },
	{ "-o", "--others"     , 3 },
	{ "-p", "--pattern"    , 3 },
	{ "-r", "--recursive"  , 5 },
	{ "-R", "--reverse"    , 5 },
	{ "-s", "--seconds"    , 3 },
	{ "-t", "--target"     , 3 },
	{ "-u", "--units"      , 3 },
	{ "-v", "--verbose"    , 3 },
    };

    for (optiontableidx=0; optiontableidx<sizeof(optiontable)/sizeof(Optiontype); optiontableidx++) {

	/* if '--longopt' with or without something following (eg, '--longopt=<param>' */
	if (!strncmp(longopt, optiontable[optiontableidx].longform,
							optiontable[optiontableidx].minuniqlen)) {

	    /* check for invalid characters in (possibly less than the full) --longopt */
	    optiontblcharptr = optiontable[optiontableidx].longform;
	    longoptcharptr = longopt;
	    while (*optiontblcharptr && *longoptcharptr && *optiontblcharptr == *longoptcharptr
								    && *longoptcharptr != '=') {
		optiontblcharptr++;
		longoptcharptr++;
	    }
	    if (*longoptcharptr != '\0' && *longoptcharptr != '=') {
		fprintf(stderr, "Bad command line option '%s', aborting\n", longopt);
		exit(1);
	    }

	    equalptr = strchr(longopt, '=');
	    /* if --longopt has no equal sign following it */
	    if (equalptr == NULL) {
		memmove(longopt, optiontable[optiontableidx].shortform, 3);
		optionfoundflag = 1;
	    /* if exactly '--longopt=<param>', and NOT exactly '--longopt=' (<param> missing) */
	    } else if (*(equalptr+1) != '\0') {
		*(longopt+1) = *(optiontable[optiontableidx].shortform+1);
		memmove(longopt+2, equalptr+1, strlen(equalptr+1)+1);
		optionfoundflag = 1;
	    }
	}
    }

    /* if '--bogus_option' or '--valid_option=' was found */
    if (!optionfoundflag) {
	fprintf(stderr, "Illegal command line option '%s', aborting\n", longopt);
	exit(1);
    }
}


/*******************************************************************************
Parse the command line arguments left to right, processing them in order.  See
the usage message.
*******************************************************************************/
int main(int argc, char *argv[]) {
    extern char *optarg;
    extern int	 optind, optopt, opterr;
    int		 optchar, optidx;
    regex_t	 extregexp;
    time_t	 targettime = 0;	/* default */

    setlocale(LC_ALL, getenv("LANG"));

    if (argc <= 1) {
	display_usage_message(argv[0]);
	exit(1);
    }

    if ((objectinfotable=(Objectinfo*)calloc(INITMAXNUMOBJS, sizeof(Objectinfo))) == NULL) {
	perror("Could not calloc initial object info table");
	exit(1);
    }

    /* replace any --longarg(s) with the equivalent -l (short argument(s)) */
    for (optidx=1; optidx<argc; optidx++) {
	if (!strncmp(argv[optidx], "--", 2)) {
	    command_line_long_to_short(argv[optidx]);
	}
    }

    starttime = time(NULL);	/* the current time - in seconds */
    targettime = DEFAULTAGE;	/* set default, 0 seconds - find files of all ages */

    set_extended_regular_expression(DEFAULTERE, &extregexp);	/* set default */

    /* Both while loops and the if (below) are required because command line options
    and arguments can be interspersed and are processed in (left-to-right) order */
    while (optind < argc) {
	while ((optchar = getopt(argc, argv, GETOPTSTR)) != -1) {
	    switch(optchar) {
		case 'd': directoryflag    = !directoryflag;			break;
		case 'f': regularfileflag  = !regularfileflag;			break;
		case 'o': otherobjectflag  = !otherobjectflag;			break;
		case 'r': recursiveflag    = !recursiveflag;			break;
		case 'i': ignorecaseflag   = !ignorecaseflag;			break;
		case 'a': targettime =
		    set_target_time_by_relative_age(optarg, optchar);		break;
		case 'm': targettime =
		    set_target_time_by_relative_age(optarg, optchar);		break;
		case 'p': set_extended_regular_expression(optarg, &extregexp);	break;
		case 'A': targettime =
		    set_target_time_by_object_time(optarg, optchar);		break;
		case 'M': targettime =
		    set_target_time_by_object_time(optarg, optchar);		break;
		case 'h': display_usage_message(argv[0]); exit(0);		break;
		case 's': displaysecondsflag  = 1;				break;
		case 'u': secondsunitchar = SECONDSUNITCHAR;
			  bytesunitchar   = BYTESUNITCHAR;			break;
		case 'v': verbosity++;						break;
		case 'R': sortmultiplier  = -1;					break;
		case 't': process_path(optarg, &extregexp, targettime, 1);
		    numtargets++;						break;
		break;
	    }
	}


	if (optind < argc) {	/* See above comment. Yes, this is required! */
	    process_path(argv[optind], &extregexp, targettime, 1);
	    numtargets++;
	    optind++;
	}
    }

    list_objects();
    return returncode;
}
