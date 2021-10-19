/*******************************************************************************
********************************************************************************

findfiles: find files based on various selection criteria
Copyright (C) 2016-2021 James S. Crook

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
 3. (optional) selection by last modification age(s) or timestamp(s)
 4. (optional) selection by last access age(s) or timestamp(s)
 5. (optional) selection by object name pattern matching using ERE(s)
See the usage/help message for additional options.

There are 3 main "times": "starttime", "targettime" and "objecttime"
Like the OS, findfiles stores each of these in two separate variables:
 "starttime"  is actually  starttime_s and  starttime_ns
 "targettime" is actually targettime_s and targettime_ns
 "objecttime" is actually objecttime_s and objecttime_ns
Thes are the number of seconds (s) & nanoseconds (ns) since "the epoch"
(1970-01-01 00:00:00.000000000).

findfiles sets "starttime" to the current system time when it starts.
targettime is calculated as either:
1. Relative (to start time): Both of the optional age of last modification ('-m')
   and age of last access ('-a') calculate a "targettime" relative to "startime".
   Note that "targettime" is never later (larger than) "starttime".
2. Absolute: e.g. YYYYMMDD_HHMMSS[.fraction_of_a_second]

Here is a timeline with time increasing to the right:

                                 "targettime"                   "starttime"
                                 v                              v
-------------olderthanttargettimeInewerthanttargettime---------------> -m & -a
------------olderthanttargettime) (newerthanttargettime--------------> -M & -A

For example:
 "-fm -10m" : find files modified <= 10 mins ago (modified after "targettime")
 "-fm  10m" : find files modified >= 10 mins ago (modified before "targettime")
Note that in both cases, "targettime" is 10 minutes _before_ "starttime"! So,
the numerical value and unit ("10m", in both cases above) sets "targettime" to
10 minutes before "starttime", and "-" causes findfiles to list objects last
modified/accessed more recently ("newer") than "targettime".

The optional last modificaton reference object ("-M") and last access reference
object ("-A") use the minus sign ("-") in the same way as "-m" and "-a". I.e.,
 "-fA -ref_file" : find files accessed after ref_file was (after "targettime")
 "-fA  ref_file" : find files accessed before ref_file was (before "targettime")

Note that "-m" and "-a" use <= and/or >=, but "-M" and "-A" use < and/or >!

It is assumed that, in general, the cases of file system objects having future
last access and/or last modification times are both rare and uninteresting.
*******************************************************************************/
#define PROGRAMVERSIONSTRING	"2.2.1"

#define _GNU_SOURCE		/* required for strptime */

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

#define MAX(x,y)		((x)>(y)?(x):(y))

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
#define NANOSECONDSPERSECOND	1000000000

#define MAXRECURSIONDEPTH	 10000		/* int value - should match the... */
#define MAXRECURSIONDEPTHSTR	"10000"		/* text value */
#define MAXDATESTRLENGTH	64
#define MAXPATHLENGTH		2048
#define INITMAXNUMOBJS		(  8*1024)	/* Allocate the object table to hold up to this many entries. */
#define MAXNUMOBJSMLTFCT	2		/* Dynamically increase the object table size by this factor... */
#define MAXNUMOBJSMLTLIM	(512*1024)	/* up to this number. After that, ... */
#define MAXNUMOBJSINCVAL	( 64*1024)	/* increment the size by this value. */
#define PATHDELIMITERCHAR	'/'
#define RELMODAGECHAR		'm'
#define REFMODAGECHAR		'M'
#define SECONDSUNITCHAR		's'
#define BYTESUNITCHAR		'B'
#define DEFAULTAGE		0
#define DEFAULTERE		".*"
#define NOW			"Now"
#define SECONDSFORMATSTR	"%S"

#define GETOPTSTR		"+dforia:m:p:t:A:D:M:hnsuRv"

typedef struct {
    char	*name;
    char	*defaultvalue;
    char	**valueptr;
} Envvar;

char	*starttimestr;
char	*datetimeformatstr;
char	*ageformatstr;
char	*infodatetimeformatstr;
char	*timestampformatstr;

Envvar envvartable[] = {
    { "FF_STARTTIME",		NOW,				&starttimestr },
    { "FF_DATETIMEFORMAT",	"%04d%02d%02d_%02d%02d%02d",	&datetimeformatstr },
    { "FF_AGEFORMAT",		"%7ldD_%02ld:%02ld:%02ld",	&ageformatstr },
    { "FF_INFODATETIMEFORMAT",	"%c",				&infodatetimeformatstr },
    { "FF_TIMESTAMPFORMAT",	"%Y%m%d_%H%M%S",		&timestampformatstr },
};

typedef struct {	/* each object's name, modification XOR access time & size */
    char	*name;
    time_t	time_s;
    time_t	time_ns;
    off_t	size;
} Objectinfo;

Objectinfo	*objectinfotable;

time_t	starttime_s;
time_t	starttime_ns;
time_t	targettime_s	= DEFAULTAGE;	/* set default, 0 s, and */
time_t	targettime_ns	= DEFAULTAGE;	/* 0 ns - find files of all ages */

int	maxnumberobjects	= INITMAXNUMOBJS;
int	numobjsfound		= 0;
int	numtargets		= 0;
int	returncode		= 0;

/* Command line option flags - all set to false */
int	maxrecursiondepth	= MAXRECURSIONDEPTH;
int	recursiveflag		= 0;
int	ignorecaseflag		= 0;
int	regularfileflag		= 0;
int	directoryflag		= 0;
int	otherobjectflag		= 0;
int	verbosity		= 0;
int	displaysecondsflag	= 0;
int	displaynsecflag		= 0;
int	accesstimeflag		= 0;
int	newerthantargetflag	= 0;
int	sortmultiplier		= 1;
char	secondsunitchar		= ' ';
char	bytesunitchar		= ' ';

void process_directory(char *, regex_t *, int);


/*******************************************************************************
Display the usage (help) message.
*******************************************************************************/
void display_usage_message(char *progname) {
    size_t	lineidx;

    typedef struct {
	char	*formatstr;
	char	*strvarptr;
    } Usagelineobject;

    Usagelineobject usagelinetable[] = {
	{"usage (version %s):",											PROGRAMVERSIONSTRING},
	{"%s [OPTION]... [target|-t target]... [OPTION]... [target|-t target]...",				progname},
	{" Some OPTIONs require arguments. These are:",									NULL},
	{"  age    : a +/- relative age value followed by a time unit",							NULL},
	{"  ERE    : a POSIX-style Extended Regular Expression (pattern)",						NULL},
	{"  path   : the pathname of a reference object (file, directory, etc.)",					NULL},
	{"  target : the pathname of an object (file, directory, etc.) to search",					NULL},
	{" OPTIONs - can be toggled on/off (parsed left to right):",							NULL},
	{"  -d|--directories : directories   (default off)",								NULL},
	{"  -f|--files       : regular files (default off)",								NULL},
	{"  -o|--others      : other files   (default off)",								NULL},
	{"  -r|--recursive   : recursive - traverse file trees (default off)",						NULL},
	{"  -i|--ignore-case : case insensitive pattern match - invoke before -p option (default off)",			NULL},
	{" OPTIONs requiring an argument (parsed left to right):",							NULL},
	{"  -a|--acc-info [-|+]access_age        : - for newer/=, [+] for older/= ages (no default), or",		NULL},
	{"    |--acc-info [-|+]access_time       : - for newer/=, [+] for older/= times (no default)",			NULL},
	{"  -m|--mod-info [-|+]modification_age  : - for newer/=, [+] for older/= ages (default 0s: any time), or",	NULL},
	{"    |--mod-info [-|+]modification_time : - for newer/=, [+] for older/= times (no default",			NULL},
	{"  -p|--pattern ERE                     : POSIX-style Extended Regular Expression (pattern) (default '.*')",	NULL},
	{"  -t|--target target_path              : target path (no default)",						NULL},
	{"  -A|--acc-ref [-|+]acc_ref_path       : - for newer, [+] for older ages (no default)",			NULL},
	{"  -D|--depth maximum_recursion_depth   : maximum recursion traversal depth/level (default %s)",	MAXRECURSIONDEPTHSTR},
	{"  -M|--mod-ref [-|+]mod_ref_path       : - for newer, [+] for older ages (no default)",			NULL},
	{" Flags - are 'global' options (and can NOT be toggled by setting multiple times):",				NULL},
	{"  -h|--help        : display this help message",								NULL},
	{"  -n|--nanoseconds : in verbose mode, display the maximum resolution of the OS/FS - up to ns",		NULL},
	{"  -s|--seconds     : display file ages in seconds (default D_hh:mm:ss)",					NULL},
	{"  -u|--units       : display units: s for seconds, B for Bytes (default off)",				NULL},
	{"  -R|--reverse     : Reverse the (time) order of the output (default off)",					NULL},
	{" Verbosity: (May be specified more than once for additional information.)",					NULL},
	{"  -v|--verbose : also display modification time, age & size(B) (default 0[off])",				NULL},
	{" Time units:",												NULL},
	{"  Y: Years    M: Months     W: Weeks      D: Days",								NULL},
	{"  h: hours    m: minutes    s: seconds",									NULL},
	{"  Note: Specify Y & M with integer values. W, D, h, m & s can also take floating point values.",		NULL},
	{" Examples of command line arguments (parsed left to right):",							NULL},
	{"  -f /tmp                      # files in /tmp of any age, including future dates!",				NULL},
	{"  -vfn -m -1M /tmp             # files in /tmp modified <= 1 month, verbose output with ns",			NULL},
	{"  -f -m 1D -p '\\.ant$' /tmp    # files in /tmp ending in '.ant' modified >= 1 day ago",			NULL},
	{"  -fip a /tmp -ip b /var       # files named /tmp/*a*, /tmp/*A* or /var/*b*",					NULL},
	{"  -rfa -3h src                 # files in the src tree accessed <= 3 hours ago",				NULL},
	{"  -dRp pat junk                # directories named junk/*pat* - reverse sort",				NULL},
	{"  -rfM -/etc/hosts /lib        # files in the /lib tree modified after /etc/hosts was",			NULL},
	{"  -vfm -3h / /tmp -fda 1h /var # files in / or /tmp modified <= 3 hours, and dirs (but",			NULL},
	{"                               # NOT files) in /var accessed >= 1h, verbose output",				NULL},
	{"  -vf -m -20201231_010203 /tmp # files in /tmp modified after 20201231_010203",				NULL},
	{"",														NULL},
	{"findfiles Copyright (C) 2016-2021 James S. Crook",								NULL},
	{"This program comes with ABSOLUTELY NO WARRANTY.",								NULL},
	{"This is free software, and you are welcome to redistribute it under certain conditions.",			NULL},
	{"This program is licensed under the terms of the GNU General Public License as published",			NULL},
	{"by the Free Software Foundation, either version 3 of the License, or (at your option) any",			NULL},
	{"later version (see <http://www.gnu.org/licenses/>).",								NULL},
    };

    for (lineidx=0; lineidx<sizeof(usagelinetable)/sizeof(Usagelineobject); lineidx++) {
	if (usagelinetable[lineidx].strvarptr == NULL) {
	    printf("%s\n", usagelinetable[lineidx].formatstr);
	} else {
	    printf(usagelinetable[lineidx].formatstr, usagelinetable[lineidx].strvarptr);
	    printf("\n");
	}
    }
}


/*******************************************************************************
Process a (file system) object - eg, a regular file, directory, symbolic
link, fifo, special file, etc. If the object's attributes satisfy the command
line arguments (i.e., the name matches the extended regular expression (pattern),
the access xor modification time, etc. then, this object is appended to the
objectinfotable. If objectinfotable is full, its size is dynamically doubled.
*******************************************************************************/
void process_object(char *pathname, regex_t *extregexpptr) {
    struct	stat statinfo;
    char	objectname[MAXPATHLENGTH], *chptr;
    time_t	objecttime_s, objecttime_ns;

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
	    objecttime_s = statinfo.st_atime;
	    objecttime_ns = statinfo.st_atim.tv_nsec;
	} else {
	    objecttime_s = statinfo.st_mtime;
	    objecttime_ns = statinfo.st_mtim.tv_nsec;
	}

	if ((targettime_s == DEFAULTAGE && targettime_ns == DEFAULTAGE) ||
	    ( newerthantargetflag && (
		objecttime_s > targettime_s ||
		(objecttime_s == targettime_s && objecttime_ns >= targettime_ns))
	    ) ||
	    (!newerthantargetflag && (
		objecttime_s < targettime_s ||
		(objecttime_s == targettime_s && objecttime_ns <= targettime_ns))
	    )
	) {
	    if (numobjsfound >= maxnumberobjects) {
		if (maxnumberobjects <= MAXNUMOBJSMLTLIM) {
		    maxnumberobjects *= MAXNUMOBJSMLTFCT;
		} else {
		    maxnumberobjects += MAXNUMOBJSINCVAL;
		}
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
	    objectinfotable[numobjsfound].size = statinfo.st_size;
	    objectinfotable[numobjsfound].time_s = objecttime_s;
	    objectinfotable[numobjsfound].time_ns = objecttime_ns;
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
    if (len > 1) {		/* handle the special case of '/' correctly */
	chptr = pathname+len-1;
	while (chptr >= pathname && *chptr == PATHDELIMITERCHAR) {
	    *chptr-- = '\0';
	}
    }
}


/*******************************************************************************
Process a (file system) pathname (a file, directory or "other").
*******************************************************************************/
void process_path(char *pathname, regex_t *extregexpptr, int recursiondepth) {
    struct stat	statinfo;

    if (recursiondepth == 0) {
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
	    process_object(pathname, extregexpptr);
	}
    } else if (S_ISDIR(statinfo.st_mode)) {
	if (directoryflag) {
	    process_object(pathname, extregexpptr);
	}
	if (recursiondepth == 0 || (recursiveflag && recursiondepth <= maxrecursiondepth)) {
	    process_directory(pathname, extregexpptr, recursiondepth);
	}
    } else if (otherobjectflag) {
	process_object(pathname, extregexpptr);
    }
}


/*******************************************************************************
Process a directory. Open it, read all it's entries (objects) and call
process_path for each one (EXCEPT '.' and '..') and close it.
*******************************************************************************/
void process_directory(char *pathname, regex_t *extregexpptr, int recursiondepth) {
    DIR			*dirptr;
    struct dirent	*direntptr;
    char		newpathname[MAXPATHLENGTH];
    char		dirpath[MAXPATHLENGTH];

    if (strcmp(pathname, "/")) {
	sprintf(dirpath, "%s%c", pathname, PATHDELIMITERCHAR);		/* not "/" */
    } else {
	sprintf(dirpath, "%c", PATHDELIMITERCHAR);			/* pathname is "/" */
    }

    if ((dirptr=opendir(pathname)) == (DIR*)NULL) {
	fprintf(stderr, "opendir error");
	perror(pathname);
	returncode = 1;
	return;
    }

    while ((direntptr=readdir(dirptr)) != (struct dirent *)NULL) {
	if (strcmp(direntptr->d_name, ".") && strcmp(direntptr->d_name, "..")) {
	    /* create newpathname from pathname/objectname. (sprintf generates a gcc warning) */
	    strcpy(newpathname, dirpath);
	    strcat(newpathname, direntptr->d_name);
	    process_path(newpathname, extregexpptr, recursiondepth+1);
	}
    }

    if (closedir(dirptr)) {
	perror(pathname);
	returncode = 1;
    }
}


/*******************************************************************************
Comparison function for sorting objectinfotable by time (with qsort). Sort is by
seconds, then nanoseconds, then filename.
*******************************************************************************/
int compare_object_info(const void *firstptr, const void *secondptr) {
    const Objectinfo	*firstobjinfoptr = firstptr;	/* to keep gcc happy */
    const Objectinfo	*secondobjinfoptr = secondptr;

    if (firstobjinfoptr->time_s != secondobjinfoptr->time_s) {
	return (secondobjinfoptr->time_s - firstobjinfoptr->time_s)*sortmultiplier;
    } else {
	if (firstobjinfoptr->time_ns != secondobjinfoptr->time_ns) {
	    return (secondobjinfoptr->time_ns - firstobjinfoptr->time_ns)*sortmultiplier;
	} else {
	    return (strcmp(firstobjinfoptr->name, secondobjinfoptr->name))*sortmultiplier;
	}
    }
}


/*******************************************************************************
Sort objectinfotable by time, and display the object's information - optionally,
the timestamp and age, and (always) the name.
Due to storing times in 2 variables (*_s and *_ns), it is necessary to add 1s to
the objectage_ns value and subtract 1s from the objectage_s value whenever
starttime_ns < the_object's_age_in_ns.
*******************************************************************************/
void list_objects() {
    struct tm	*localtimeinfoptr;
    char	objectagestr[MAXOBJAGESTRLEN], *chptr;
    int		foundidx, negativeageflag;
    time_t	objectage_s, absobjectage_s, days, hrs, mins, secs;
    time_t	objectage_ns = DEFAULTAGE;

    qsort((void*)objectinfotable, (size_t)numobjsfound, (size_t)sizeof(Objectinfo), compare_object_info);
    for (foundidx=0; foundidx<numobjsfound; foundidx++) {
	if (verbosity > 0) {
	    if (verbosity > 2) {		/* Test/debug: object time in s and ns */
		printf("%10ld.%09ld = ", objectinfotable[foundidx].time_s,
		    objectinfotable[foundidx].time_ns);
	    }

	    /* year, month day, hour, minute, second */
	    localtimeinfoptr = localtime(&objectinfotable[foundidx].time_s);
	    printf(datetimeformatstr, localtimeinfoptr->tm_year+TMBASEYEAR,
		localtimeinfoptr->tm_mon+TMBASEMONTH, localtimeinfoptr->tm_mday,
		localtimeinfoptr->tm_hour, localtimeinfoptr->tm_min, localtimeinfoptr->tm_sec);
	    if (displaynsecflag) {		/* ns */
		printf(".%09ld", objectinfotable[foundidx].time_ns);
	    }

	    if (starttime_s > objectinfotable[foundidx].time_s || /* starttime >= object's time */
					(starttime_s == objectinfotable[foundidx].time_s &&
					starttime_ns >= objectinfotable[foundidx].time_ns)) {
		objectage_s = starttime_s - objectinfotable[foundidx].time_s;
		if (starttime_ns >= objectinfotable[foundidx].time_ns) {
		    objectage_ns = starttime_ns - objectinfotable[foundidx].time_ns;
		} else {
		    objectage_ns = starttime_ns - objectinfotable[foundidx].time_ns + NANOSECONDSPERSECOND;
		    objectage_s--;
		}
		negativeageflag = 0;
	    } else {					/* object's time is after starttime - future! */
		objectage_s = starttime_s - objectinfotable[foundidx].time_s;
		if (starttime_ns <= objectinfotable[foundidx].time_ns) {
		    objectage_ns = objectinfotable[foundidx].time_ns - starttime_ns;
		} else {
		    objectage_ns = objectinfotable[foundidx].time_ns - starttime_ns + NANOSECONDSPERSECOND;
		    objectage_s++;
		}
		negativeageflag = 1;
	    }

	    if (verbosity > 2) {		/* Test/debug: object age in s and ns */
		printf(" %10ld.%09ld = ", objectage_s, objectage_ns);
	    }

	    if (displaysecondsflag) {	/* object age in seconds */
		printf("%16ld", objectage_s);
		if (displaynsecflag) {
		    printf(".%09ld", objectage_ns);
		}
		printf("%c ", secondsunitchar);
	    } else {				/* object age in days, hours, minutes and seconds */
		absobjectage_s = objectage_s >= 0 ? objectage_s : -objectage_s;	/* absolute value */
		days = absobjectage_s/SECONDSPERDAY;
		hrs = absobjectage_s/SECONDSPERHOUR - days*HOURSPERDAY;
		mins = absobjectage_s/SECONDSPERMINUTE - days*MINUTESPERDAY - hrs*MINUTESPERHOUR;
		secs = absobjectage_s % SECONDSPERMINUTE;
		sprintf(objectagestr, ageformatstr, days, hrs, mins, secs);
		/* if objectage_s is negative (future timestamp), display a - sign */
		if (negativeageflag) {
		    if ((chptr=strrchr(objectagestr, ' ')) != NULL) {
			*chptr = '-'; /* %07ld : OK for 999999 days - until the year 4707 */
		    } else {
			fprintf(stderr, "Error: Insuficient 'days' field width in '%s'\n", ageformatstr);
			exit(1);
		    }
		}
		printf("%s", objectagestr);

		if (displaynsecflag) {
		    printf(".%09ld", objectage_ns);
		}
		printf(" ");
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
Integer values must be used with units 'M' (months) and 'Y' (years) because
months and years vary in size. E.g., '0.5M' does NOT always equate to the same
amount of time, but 1.33s, 0.25h, 0.5m, 0.1W, etc., all do.
*******************************************************************************/
void check_integer(char *relativeagestr) {
    char	*chptr;

    for (chptr=relativeagestr; chptr<relativeagestr+strlen(relativeagestr)-1; chptr++) {
	if (!isdigit(*chptr) && *chptr != '-' && *chptr != '+' ) {
	    fprintf(stderr, "Warning: non-integer character '%c' in '%s'!\n", *chptr, relativeagestr);
	}
    }
}


/*******************************************************************************
Adjust the relative age of targettime. Parse the relativeagestr argument (a
string representing floating point number) into integer and (optional) fraction
parts. Update the breakdown time structure (brkdwntimeptr, see below) by the
calculated integer number of seconds, and return the calculated number of ns.
*******************************************************************************/
time_t adjust_relative_age(char *relativeagestr, int *timeunitptr, long secsperunit) {
    double	decimal_s, fraction_s;

    decimal_s = atof(relativeagestr) * secsperunit;
    fraction_s = decimal_s - (long)decimal_s;
    *timeunitptr -= (int)decimal_s;
    return (time_t)(fraction_s * NANOSECONDSPERSECOND);
}


/*******************************************************************************
*******************************************************************************/
void set_relative_targettime(char *timeinfostr, struct tm *brkdwntimeptr, char timeunitchar) {
    time_t	relage_ns = DEFAULTAGE;

    switch (timeunitchar) {		/* set targettime relative to now */
	case 's': relage_ns = adjust_relative_age(timeinfostr, &(brkdwntimeptr->tm_sec), 1);
	    break;
	case 'm': relage_ns = adjust_relative_age(timeinfostr, &(brkdwntimeptr->tm_sec), SECONDSPERMINUTE);
	    break;
	case 'h': relage_ns = adjust_relative_age(timeinfostr, &(brkdwntimeptr->tm_sec), SECONDSPERHOUR);
	    break;
	case 'D': relage_ns = adjust_relative_age(timeinfostr, &(brkdwntimeptr->tm_sec), SECONDSPERDAY);
	    break;
	case 'W': relage_ns = adjust_relative_age(timeinfostr, &(brkdwntimeptr->tm_sec), SECONDSPERWEEK);
	    break;
	case 'M': check_integer(timeinfostr);
	    brkdwntimeptr->tm_mon -= atoi(timeinfostr);
	    break;
	case 'Y': check_integer(timeinfostr);
	    brkdwntimeptr->tm_year -= atoi(timeinfostr);
	    break;
	default: fprintf(stderr, "Error: Illegal time unit '%c'\n", timeunitchar);
	    exit(1);
    }
    targettime_s = mktime(brkdwntimeptr);

    /* Due to storing times in 2 variables (*_s and *_ns), it is necessary to add 1s to
	the targettime_ns value and subtract 1s from the targettime_s value whenever
	starttime_ns < relage_ns. */
    if (starttime_ns >= relage_ns) {
	targettime_ns = starttime_ns - relage_ns;
    } else {
	targettime_ns = starttime_ns - relage_ns + NANOSECONDSPERSECOND;
	targettime_s--;
    }
}


/*******************************************************************************
Set the absolute time (for both modification and access) based on the required
format. The default is '%Y%m%d_%H%M%S', but this can be changed by by setting
environment variable FF_TIMESTAMPFORMAT. It is possible to specify a subset of
these. If not all of year to second are specified, the values of the start
time are used. For example, one could set FF_TIMESTAMPFORMAT to
'date:%m%d, hour:%H' and specify these on the command line: 'date:', month, day,
', hour:', and hour, e.g., 'date:1231, hour:23' - or '%d%m%H' and '311223').
See strptime for details.
*******************************************************************************/
void set_absolute_targettime(char *timeinfostr, struct tm *brkdwntimeptr) {
    size_t	timestampformatstrlen;
    char	*fractionstr, *fromchptr, *tochptr;
    char	nanosecondsstr[] = "000000000";

    targettime_ns = DEFAULTAGE;		/* set to zero - use if fraction of a second is specified */

    /* convert the command line timeinfostr (eg, YYYYMMDD_HHMMSS) to a breakdown time structure */
    fractionstr = strptime(timeinfostr, timestampformatstr, brkdwntimeptr);
    if (fractionstr == NULL) {
	fprintf(stderr, "Error: bad timestamp: '%s' must be in format '%s'\n", timeinfostr, timestampformatstr);
	exit(1);
    } else if (*fractionstr == '.') {	/* if a decimal point ('.') follows a valid timestamp */
	/* Ensure that the last characters of timestampformatstr (eg, %Y%m%d_%H%M%S) are SECONDSFORMATSTR ("%S") */
	timestampformatstrlen = strlen(timestampformatstr);
	if (timestampformatstrlen < sizeof(SECONDSFORMATSTR)-1 ||
		strcmp(timestampformatstr+timestampformatstrlen-(sizeof(SECONDSFORMATSTR)-1), SECONDSFORMATSTR)) {
	    fprintf(stderr, "Error: last two characters of '%s' must be '%s' when using fractions of seconds\n",
		timestampformatstr, SECONDSFORMATSTR);
	    exit(1);
	}

	/* Set targettime_ns to the fraction of a second contained in timeinfostr- eg, ".25" -> 250,000,000 ns */
	fromchptr = fractionstr+1;
	tochptr = nanosecondsstr;
	while (*fromchptr && *tochptr) {	/* copy digits from fractionstr to temp string nanosecondsstr */
	    if (isdigit(*fromchptr)) {
		*tochptr++ = *fromchptr++;
	    } else {
		fprintf(stderr, "Error: Bad timestamp fraction: '%s' in '%s'\n", fractionstr, timeinfostr);
		exit(1);
	    }
	}
	*fromchptr = '\0';	/* Only the first 9 digits of the fraction are used; any more are ignored */
	targettime_ns = atoi(nanosecondsstr);
    } else if (*fractionstr != '\0') {
	fprintf(stderr, "Error: Illegal timestamp character(s) starting at '%c' in timestamp '%s'\n",
	    *fractionstr, timeinfostr);
	exit(1);
    }
    if (fractionstr == NULL || *fractionstr != '\0' || verbosity > 1) {
	fprintf(stderr, "i: absolute timestamp: '%s'\n", timeinfostr);
    }
    targettime_s = mktime(brkdwntimeptr);
}


/*******************************************************************************
Set targettime from a command line argumet in one of two formats:
1. When the last character of timeinfostr is one of Y, M, D, h, m or s: by
   subtracting the relative) "age" command line argument, e.g., "15D" from
   starttime. Note: targettime will always be less than starttime. I.e., "-30s"
   and "[+]30s" both result in targettime = starttime-30s.
2. When the last character of timeinfostr is a digit: by using timeinfostr
   as a timestamp. timestampformatstr (default "%Y%m%d_%H%M%S", and configurable
   with FF_TIMESTAMPFORMAT) is used to parse the value. So timestamps must be
   entered in the format YYYYMMDD_HHMMSS[.secondfraction] unless the environment
   variable FF_TIMESTAMPFORMAT is changed.
In either case, a first character of '-' is used to set the newerthantargetflag.
This function is called for both last access time and last modification time.
*******************************************************************************/
void set_target_time_by_cmd_line_arg(char *timeinfostr, char c) {
    char	timeunitchar;
    struct tm	*brkdwntimeptr;
    char	datestr[MAXDATESTRLENGTH];
    time_t	relagenanoseconds;

    if (c == RELMODAGECHAR) {
	accesstimeflag = 0;
    } else {
	accesstimeflag = 1;
    }

    if (*timeinfostr == '-' ) {
	/* eg, "-m -15D" find objects modified <= 15 days ago (newer than) */
	newerthantargetflag = 1;
	timeinfostr++;
    } else {
	/* eg, "-m [+]15D" find objects modified >= 15 days ago (older than) */
	newerthantargetflag = 0;
    }

    timeunitchar = *(timeinfostr+strlen(timeinfostr+1));
    brkdwntimeptr = localtime(&starttime_s);

    if (isdigit(timeunitchar)) {	/* if the last character of timeinfostr is a digit */
	set_absolute_targettime(timeinfostr, brkdwntimeptr);
    } else {
	set_relative_targettime(timeinfostr, brkdwntimeptr, timeunitchar);
    }

    /* This will be a problem in 2262 */
    relagenanoseconds = (starttime_s-targettime_s)*NANOSECONDSPERSECOND + starttime_ns-targettime_ns;

    if (verbosity > 1) {
	strftime(datestr, MAXDATESTRLENGTH, infodatetimeformatstr, brkdwntimeptr);
	fprintf(stderr, "i: target time: %15ld.%09lds ~= %s\n", targettime_s, targettime_ns, datestr);
	fprintf(stderr, "i: %13.5fD ~= %10ld.%09lds last %s %s target time ('%s')\n",
	    (float)(starttime_s-targettime_s)/SECONDSPERDAY, relagenanoseconds/NANOSECONDSPERSECOND,
	    relagenanoseconds%NANOSECONDSPERSECOND, accesstimeflag ? "accessed" : "modified",
	    newerthantargetflag ? "after (newer than)" : "before (older than)", timeinfostr);
	fflush(stderr);
    }
}


/*******************************************************************************
Set targettime to be the same as that of the reference object's last modification
or last access time, as required.
*******************************************************************************/
void set_target_time_by_object_time(char *targetobjectstr, char c) {
    struct stat	statinfo;

    newerthantargetflag = 0;
    if (*targetobjectstr == '-') {
	/* eg, "-M -foo" find objects last modified AFTER foo was (NEWER than) */
	newerthantargetflag = 1;
	targetobjectstr++;
    } else if (*targetobjectstr == '+') {
	/* eg, "-M [+]foo" find objects last modified BEFORE foo was (OLDER than) */
	targetobjectstr++;
    }
    if (verbosity > 1) {
	fprintf(stderr, "i: last %s %s than '%s'\n", accesstimeflag ? "accessed" : "modified",
	    newerthantargetflag ? "after (newer than)" : "before (older than)", targetobjectstr);
	fflush(stderr);
    }

    if (*targetobjectstr && (lstat(targetobjectstr, &statinfo) != -1)) {
	if (c == REFMODAGECHAR) {
	    accesstimeflag = 0;
	    targettime_s = statinfo.st_mtime;
	    targettime_ns = statinfo.st_mtim.tv_nsec;
	} else {
	    accesstimeflag = 1;
	    targettime_s = statinfo.st_mtime;
	    targettime_ns = statinfo.st_atim.tv_nsec;
	}
    } else {
	fprintf(stderr, "Cannot access '%s'\n", targetobjectstr);
	exit(1);
    }
    if (newerthantargetflag) {
	targettime_ns += 1;	/* +1ns for NEWER than (NOT the same age!) */
    } else {
	targettime_ns -= 1 ;	/* -1ns for OLDER than (NOT the same age!) */
    }
}


/*******************************************************************************
Set the extended regular expression (pattern) to be used to match the object names.
*******************************************************************************/
#define MAXREGCOMPERRMSGLEN	64
void set_extended_regular_expression(char *extregexpstr, regex_t *extregexpptr) {
    char	regcomperrmsg[MAXREGCOMPERRMSGLEN];
    int		cflags;
    int		regcompretval;

    if (ignorecaseflag) {
	cflags = REG_EXTENDED|REG_ICASE;
    } else {
	cflags = REG_EXTENDED;
    }

    if ((regcompretval=regcomp(extregexpptr, extregexpstr, cflags)) != 0) {
	regerror(regcompretval, extregexpptr, regcomperrmsg, MAXREGCOMPERRMSGLEN);
	printf("Regular expression error for '%s': %s\n", extregexpstr, regcomperrmsg);
	exit(1);
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
	char	*shortform;
	char	*longform;
	int	minuniqlen;
    } Optiontype;

    Optiontype optiontable[] = {
	{ "-a", "--acc-info"	, 7 },
	{ "-A", "--acc-ref"	, 7 },
	{ "-D", "--depth"	, 4 },
	{ "-d", "--directories"	, 4 },
	{ "-f", "--files"	, 3 },
	{ "-h", "--help"	, 3 },
	{ "-i", "--ignore-case"	, 3 },
	{ "-m", "--mod-info"	, 7 },
	{ "-M", "--mod-ref"	, 7 },
	{ "-n", "--nanoseconds"	, 3 },
	{ "-o", "--others"	, 3 },
	{ "-p", "--pattern"	, 3 },
	{ "-r", "--recursive"	, 5 },
	{ "-R", "--reverse"	, 5 },
	{ "-s", "--seconds"	, 3 },
	{ "-t", "--target"	, 3 },
	{ "-u", "--units"	, 3 },
	{ "-v", "--verbose"	, 3 },
    };

    for (optiontableidx=0; optiontableidx<sizeof(optiontable)/sizeof(Optiontype); optiontableidx++) {
	/* if '--longopt' with or without something following (eg, '--longopt=<param>' */
	if (!strncmp(longopt, optiontable[optiontableidx].longform, optiontable[optiontableidx].minuniqlen)) {

	    /* check for invalid characters in (possibly less than the full) --longopt */
	    optiontblcharptr = optiontable[optiontableidx].longform;
	    longoptcharptr = longopt;
	    while (*optiontblcharptr && *longoptcharptr && *optiontblcharptr == *longoptcharptr &&
		*longoptcharptr != '=') {
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
Set starttime_s and starttime_ns with the current system time. (Unless environment
variable FF_STARTTIME is set, which is mainly useful for testing.)
*******************************************************************************/
void set_starttime() {
    struct timespec	currenttime;
    int			numdigits;
    char		*decimalptr;

    if (!strcmp(starttimestr, NOW)) {
	/* get the current time in s and ns */
	clock_gettime(CLOCK_REALTIME, &currenttime);
	starttime_s = currenttime.tv_sec;
	starttime_ns = currenttime.tv_nsec;
    } else {		/* generally, this is used for testing */
	starttime_s = atoi(starttimestr);
	if ((decimalptr=strchr(starttimestr, '.')) != NULL) {
	    starttime_ns = atoi(++decimalptr);
	    numdigits = strlen(decimalptr);
	    while (numdigits < 9) {		/* No integer power operator, Doh! */
		starttime_ns *= 10;
		numdigits++;
	    }
	    if (starttime_ns < 0 || starttime_ns >= NANOSECONDSPERSECOND) {
		fprintf(stderr, "Illegal value for starttime fraction: %ld ns\n", starttime_ns);
		exit(1);
	    }
	} else {
	    starttime_ns = 0;
	}
    }
}


/*******************************************************************************
Display the starttime in s.ns and human readable format.
*******************************************************************************/
void list_starttime() {
    struct tm	*brkdwntimeptr;
    char	datestr[MAXDATESTRLENGTH];

    brkdwntimeptr = localtime(&starttime_s);
    strftime(datestr, MAXDATESTRLENGTH, infodatetimeformatstr, brkdwntimeptr);
    fprintf(stderr, "i: start time:  %15ld.%09lds ~= %s\n", starttime_s, starttime_ns, datestr);
    fflush(stderr);
}


/*******************************************************************************
If any of the environment variables in envvartable have been set, overwrite the
default values of the relevant (string) variable with the contents.
*******************************************************************************/
void set_envvars() {
    unsigned int	idx;
    char		*envvarvaluestr;

    for (idx=0; idx<sizeof(envvartable)/sizeof(Envvar); idx++) {
	if ((envvarvaluestr=getenv(envvartable[idx].name)) != NULL) {
	    *envvartable[idx].valueptr = malloc(strlen(envvarvaluestr)+1);
	    strcpy(*envvartable[idx].valueptr, envvarvaluestr);
	} else {
	    *envvartable[idx].valueptr = malloc(strlen(envvartable[idx].defaultvalue)+1);
	    strcpy(*envvartable[idx].valueptr, envvartable[idx].defaultvalue);
	}
    }
}


/*******************************************************************************
List all the variables that can be set (all the entries of envvartable) - and
the value of each one. If that's not the default value, list that too.
*******************************************************************************/
#define MAXOUTPUTFMTMSGLEN	128
void list_envvartable() {
    unsigned int	idx;
    char		outputformatstr[MAXOUTPUTFMTMSGLEN];
    size_t		maxvarnamelen = 0, maxvarvaluelen = 0;

    /* Find the max lengths of the name and value strings (used for formatting) */
    for (idx=0; idx<sizeof(envvartable)/sizeof(Envvar); idx++) {
	maxvarnamelen = strlen(envvartable[idx].name) > maxvarnamelen ?
	    strlen(envvartable[idx].name) : maxvarnamelen;
	maxvarvaluelen = strlen(*envvartable[idx].valueptr) > maxvarvaluelen ?
	    strlen(*envvartable[idx].valueptr) : maxvarvaluelen;
    }

    for (idx=0; idx<sizeof(envvartable)/sizeof(Envvar); idx++) {
	sprintf(outputformatstr, "i: %%%lds='%%s'%%%lds", maxvarnamelen,
					maxvarvaluelen-strlen(*envvartable[idx].valueptr)+1);
	fprintf(stderr, outputformatstr, envvartable[idx].name, *envvartable[idx].valueptr, "");
	if (strcmp(*envvartable[idx].valueptr, envvartable[idx].defaultvalue)) {
	    fprintf(stderr, "# default='%s'\n", envvartable[idx].defaultvalue);
	} else {
	    fprintf(stderr, "# default\n");
	}
    }
    fflush(stderr);
}


/*******************************************************************************
Parse the command line arguments left to right, processing them in order. See
the usage message.
*******************************************************************************/
int main(int argc, char *argv[]) {
    extern char	*optarg;
    extern int	optind, optopt, opterr;
    int		optchar, optidx;
    regex_t	extregexp;

    setlocale(LC_ALL, getenv("LANG"));
    setlocale(LC_NUMERIC, "en_US.UTF-8");

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

    set_envvars();
    set_starttime();
    set_extended_regular_expression(DEFAULTERE, &extregexp);	/* set default */

    /* Both while loops and the if (below) are required because command line options
    and arguments can be interspersed and are processed in (left-to-right) order */
    while (optind < argc) {
	while ((optchar = getopt(argc, argv, GETOPTSTR)) != -1) {
	    switch(optchar) {
		case 'd': directoryflag		= !directoryflag;		break;
		case 'f': regularfileflag	= !regularfileflag;		break;
		case 'o': otherobjectflag	= !otherobjectflag;		break;
		case 'r': recursiveflag		= !recursiveflag;		break;
		case 'i': ignorecaseflag	= !ignorecaseflag;		break;
		case 'a': set_target_time_by_cmd_line_arg(optarg, optchar);	break;
		case 'm': set_target_time_by_cmd_line_arg(optarg, optchar);	break;
		case 'p': set_extended_regular_expression(optarg, &extregexp);	break;
		case 'A': set_target_time_by_object_time(optarg, optchar);	break;
		case 'D': maxrecursiondepth = MAX(0,atoi(optarg));		break;
		case 'M': set_target_time_by_object_time(optarg, optchar);	break;
		case 'h': display_usage_message(argv[0]); exit(0);		break;
		case 'n': displaynsecflag = 1;					break;
		case 's': displaysecondsflag = 1;				break;
		case 'u': secondsunitchar = SECONDSUNITCHAR;
			    bytesunitchar = BYTESUNITCHAR;			break;
		case 'v': verbosity++;						break;
		case 'R': sortmultiplier = -1;					break;
		case 't': process_path(optarg, &extregexp, 0);
			    numtargets++;					break;
	    }
	}

	if (optind < argc) {	/* See above comment. Yes, this is required! */
	    process_path(argv[optind], &extregexp, 0);
	    numtargets++;
	    optind++;
	}
    }

    if (verbosity > 1) {
	list_starttime();
    }
    list_objects();
    fflush(stdout);
    if (verbosity > 3) {
	list_envvartable();
    }
    return returncode;
}
