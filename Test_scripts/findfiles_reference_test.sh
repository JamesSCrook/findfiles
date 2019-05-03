#/bin/sh

################################################################################
# findfiles_test: test findfiles C program (for versions >= 2.0.0)
################################################################################

################################################################################
# Copyright (C) 2016-2019 James S. Crook
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
################################################################################

################################################################################
# Version 1.0	2019/04/23
################################################################################

################################################################################
################################################################################
if [ $# -ne 1 ]; then
    echo "usage: $0 findfiles_exectutable"
    exit 1
fi

FINDFILES=$1

################################################################################
# Initialize
################################################################################
TESTDIR=/tmp/FF_Test_Dir_DELETEME
rm -rf $TESTDIR
mkdir  $TESTDIR
cd     $TESTDIR

export TZ=UTC
export LANG=en_AU.UTF-8	# Display times in 24 hour format
unset FF_DATETIMEFORMAT
unset FF_AGEFORMAT

################################################################################
# Loop through - from first to last, by increment - for hour, then minute, then
# second and then ns (args 1-12). For each HMSns, set the TOUCHDDATE (date time)
# and FILENAME (date_time) environment variables from the 12 function arguments
# and create an empty file (with touch) named $TESTDIR/$FILENAME with a create
# timestamp of TOUCHDATE.
################################################################################
function create_ts_files_HMSns {
      BEGHOUR=${1};    ENDHOUR=${2};    INCHOUR=${3}
    BEGMINUTE=${4};  ENDMINUTE=${5};  INCMINUTE=${6}
    BEGSECOND=${7};  ENDSECOND=${8};  INCSECOND=${9}
       BEGNS=${10};     ENDNS=${11};     INCNS=${12}	# nanoseconds

    HOUR=$BEGHOUR
    while [ $HOUR -le $ENDHOUR ]; do
		MINUTE=$BEGMINUTE
		while [ $MINUTE -le $ENDMINUTE ]; do
			SECOND=$BEGSECOND
			while [ $SECOND -le $ENDSECOND ]; do
				NS=$BEGNS
				while [ $NS -le $ENDNS ]; do
					TIME=$(printf "%02d:%02d:%02d.%09d" $HOUR $MINUTE $SECOND $NS)
					# echo "DATE=$DATE, TIME=$TIME"
					TOUCHDATE="${DATE} ${TIME}"
					 FILENAME="${DATE}_${TIME}"
					CMD="touch -d '$TOUCHDATE' $TESTDIR/$FILENAME"
					eval "$CMD"
					NS=$((NS+INCNS))
				done
				SECOND=$((SECOND+INCSECOND))
			done
			MINUTE=$((MINUTE+INCMINUTE))
		done
		HOUR=$((HOUR+INCHOUR))
    done
}

################################################################################
# Loop through - from first to last, by increment - for year, then month, then
# day (args 1-9). For each YMD, call create_ts_files_HMSns with the 4 HMSns
# triples (args 10-21).
################################################################################
function create_ts_files_YMDHMSns {
     BEGYEAR=${1};  ENDYEAR=${2};  INCYEAR=${3}
    BEGMONTH=${4}; ENDMONTH=${5}; INCMONTH=${6}
      BEGDAY=${7};   ENDDAY=${8};   INCDAY=${9}

    rm -f $TESTDIR/*
    YEAR=$BEGYEAR
    while [ $YEAR -le $ENDYEAR ]; do
		MONTH=$BEGMONTH
		while [ $MONTH -le $ENDMONTH ]; do
			DAY=$BEGDAY
			while [ $DAY -le $ENDDAY ]; do
				DATE=$(printf "%04d-%02d-%02d" $YEAR $MONTH $DAY)
				create_ts_files_HMSns	${10} ${11} ${12}  ${13} ${14} ${15} \
										${16} ${17} ${18}  ${19} ${20} ${21}
				DAY=$((DAY+INCDAY))
			done
			MONTH=$((MONTH+INCMONTH))
		done
		YEAR=$((YEAR+INCYEAR))
    done
}

################################################################################
# Create the test files with specific timestamps and set the test start time
# environment variable FF_STARTTIME to a time very nearly at the start of "the
# epoch". Then call findfiles with various test arguments.
################################################################################
create_ts_files_YMDHMSns 1970 1970 1   1 1 1   1 1 1 \
    0 0 1   1 1 1   40 40 1   0 999999999 100000000

(
    SETSTARTTIME="1970-01-01 00:16:41"; FRACSECS=".3"
    export FF_STARTTIME=$(date +%s --date="$SETSTARTTIME")$FRACSECS
    for FINDFILESARGS in \
		"-vvvvfn             ." \
		" -vvvfn             ." \
		"  -vvfn -m  900.60s ." \
		"  -vvfn -m -900.60s ." \
		"  -vvfn -m  900.60s ." \
		"  -vvfn -m -900.60s ." \
		"  -vvfn -m   15.01m ." \
		"  -vvfn -m  -15.01m ." \
		"  -vvfn -m   15.01m ." \
		"  -vvfn -m  -15.01m ." \

    do
		echo "========== findfiles $FINDFILESARGS (${SETSTARTTIME}$FRACSECS/$FF_STARTTIME) =========="
		eval $FINDFILES $FINDFILESARGS 2>&1
    done
)

################################################################################
# Create the test files with relatively recent timestamps and set the test start
# time environment variable FF_STARTTIME to a value around that time. Then call
# findfiles with various test arguments. 
################################################################################
create_ts_files_YMDHMSns 2018 2018 1   1 1 1   9 11 1 \
    0 0 1   0 0 1   0 0 1   0 999999999 250000000

(
    SETSTARTTIME="2018-01-30 00:00:00"; FRACSECS=".0"
    export FF_STARTTIME=$(date +%s --date="$SETSTARTTIME")$FRACSECS
    REFFILE="2018-01-10_00:00:00.750000000"
    for FINDFILESARGS in \
		"-vvvvf          ." \
		"-vvvvfn         ." \
		"  -vvf  -m -20D ." \
		"  -vvfn -m -20D ." \
		"  -vvf  -a -20D ." \
		"  -vvfn -a -20D ." \
		"  -vvf  -m  20D ." \
		"  -vvfn -m  20D ." \
		"  -vvf  -a  20D ." \
		"  -vvfn -a  20D ." \
		"  -vvf  -M -$REFFILE ." \
		"  -vvfn -M -$REFFILE ." \
		"  -vvf  -A -$REFFILE ." \
		"  -vvfn -A -$REFFILE ." \
		"  -vvf  -M  $REFFILE ." \
		"  -vvfn -M  $REFFILE ." \
		"  -vvf  -A  $REFFILE ." \
		"  -vvfn -A  $REFFILE ." \
		"  -vvfn -m  1728000.25s ." \
		"  -vvfn -m  20.25D ." \

    do
		echo "========== findfiles $FINDFILESARGS (${SETSTARTTIME}$FRACSECS/$FF_STARTTIME) =========="
		eval $FINDFILES $FINDFILESARGS 2>&1
    done

	############ Check different locales ############
	FINDFILESARGS="-vvf -m 20.0025D ."
    for LANG in \
		en_US.UTF-8 \
		es_ES.UTF-8 \
		de_DE.UTF-8 \
		fr_FR.UTF-8 \

    do
		echo "========== export LANG=$LANG; findfiles $FINDFILESARGS (${SETSTARTTIME}$FRACSECS/$FF_STARTTIME) =========="
		eval export LANG=$LANG; $FINDFILES $FINDFILESARGS 2>&1
    done
)

################################################################################
# Create the test files with relatively recent timestamps and set the test start
# time environment variable FF_STARTTIME to a value around that time. Then call
# findfiles with various test arguments to test timezones.
################################################################################
create_ts_files_YMDHMSns 2017 2017 1   1 12 1   1 1 1 \
    0 0 1   0 0 1   0 0 1   0 0 1000000000

(
    SETSTARTTIME="2018-01-02 00:00:00"; FRACSECS=".5"
    export FF_STARTTIME=$(date +%s --date="$SETSTARTTIME")$FRACSECS
    for TIMEZONE in \
		UTC \
		Australia/Sydney \
		America/Los_Angeles \

	do
		for FINDFILESARGS in \
			"-vvf  -m -6M . -a 2Y ." \
			"-vvfn -m -6M . -a 2Y ." \

		do
			echo "========== export TZ=$TIMEZONE; findfiles $FINDFILESARGS (${SETSTARTTIME}$FRACSECS/$FF_STARTTIME) =========="
			eval export TZ=$TIMEZONE; $FINDFILES $FINDFILESARGS 2>&1
		done
	done
)

################################################################################
# Use the files from the previous test(s), produce output using the user-defined
# date and age format environment variables: FF_DATETIMEFORMAT and FF_AGEFORMAT
################################################################################
(
    SETSTARTTIME="2017-12-31 00:00:00"; FRACSECS=".5"
    export FF_STARTTIME=$(date +%s --date="$SETSTARTTIME")$FRACSECS
    export FF_DATETIMEFORMAT="%04d-%02d-%02dT%02d:%02d:%02dZ"
    export FF_AGEFORMAT="%7ldDays;%02ldh,%02ldm,%02lds"
    for FINDFILESARGS in \
		"-vvvvf  -m -6M ." \
		"-vvvvfn -m -6M ." \
		"  -vvf  -m -6M ." \
		"  -vvfn -m -6M ." \

    do
		echo "========== findfiles $FINDFILESARGS (${SETSTARTTIME}$FRACSECS/$FF_STARTTIME) =========="
		eval $FINDFILES $FINDFILESARGS 2>&1
    done
)

################################################################################
# Cleanup and exit
################################################################################
rm -rf $TESTDIR
