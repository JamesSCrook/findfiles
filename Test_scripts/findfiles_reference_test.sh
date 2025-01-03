#/bin/sh

################################################################################
# findfiles_test: test findfiles C program (for versions >= 2.2.0)
################################################################################

################################################################################
# Copyright (C) 2016-2022 James S. Crook
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
# Version 2.0	2021/11/06
################################################################################

################################################################################
################################################################################
if [ $# -ne 1 ]; then
    echo "usage: $0 findfiles_exectutable"
    exit 1
fi

FF=$1

################################################################################
# Initialize
################################################################################
TESTBASEDIR=/tmp/FF_Test_Dir_DELETEME
rm -rf $TESTBASEDIR
mkdir  $TESTBASEDIR
cd     $TESTBASEDIR
TESTDIR=$TESTBASEDIR

export TZ=UTC
export LANG=C
export LC_TIME=C
unset FF_DATETIMEFORMAT
unset FF_AGEFORMAT

################################################################################
# Loop through - from first to last, by increment - for hour, then minute, then
# second and then ns (args 1-12). For each HMSns, set the TOUCHDDATE (date time)
# and FILENAME (date_time) environment variables from the 12 function arguments
# and create an empty file (with touch) named $TESTBASEDIR/$FILENAME with a
# create timestamp of TOUCHDATE.
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
echo ======================================================================================================
echo "Test with a very early startime (close to 'the Epoch' in 1970)"
echo ======================================================================================================
rm -rf $TESTBASEDIR/*
create_ts_files_YMDHMSns 1970 1970 1   1 1 1   1 1 1 \
    0 0 1   1 1 1   40 40 1   0 999999999 100000000

(
    export FF_STARTTIME=19700101_001641.3
    for CMD in \
	"$FF -vvvvfn             ." \
	"$FF  -vvvfn             ." \
	"$FF   -vvfn -m  900.60s ." \
	"$FF   -vvfn -m -900.60s ." \
	"$FF   -vvfn -m  900.60s ." \
	"$FF   -vvfn -m -900.60s ." \
	"$FF   -vvfn -m   15.01m ." \
	"$FF   -vvfn -m  -15.01m ." \
	"$FF   -vvfn -m   15.01m ." \
	"$FF   -vvfn -m  -15.01m ." \

    do
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval "$CMD" 2>&1
	echo
    done
)


################################################################################
# Create the test files with relatively recent timestamps and set the test start
# time environment variable FF_STARTTIME to a value around that time. Then call
# findfiles with various test arguments. 
################################################################################
echo ======================================================================================================
echo "Test with start and target times around 2018."
echo ======================================================================================================
rm -rf $TESTBASEDIR/*
create_ts_files_YMDHMSns 2018 2018 1   1 1 1   9 11 1 \
    0 0 1   0 0 1   0 0 1   0 999999999 250000000

(
    export FF_STARTTIME=20180130_000000
    REFFILE="2018-01-10_00:00:00.750000000"
    for CMD in \
	"$FF -vvvvf          ." \
	"$FF -vvvvfn         ." \
	"$FF   -vvf  -m -20D ." \
	"$FF   -vvfn -m -20D ." \
	"$FF   -vvf  -a -20D ." \
	"$FF   -vvfn -a -20D ." \
	"$FF   -vvf  -m  20D ." \
	"$FF   -vvfn -m  20D ." \
	"$FF   -vvf  -a  20D ." \
	"$FF   -vvfn -a  20D ." \
	"$FF   -vvf  -M  $REFFILE ." \
	"$FF   -vvfn -M  $REFFILE ." \
	"$FF   -vvf  -A  $REFFILE ." \
	"$FF   -vvfn -A  $REFFILE ." \
	"$FF   -vvf  -M -$REFFILE ." \
	"$FF   -vvfn -M -$REFFILE ." \
	"$FF   -vvf  -A -$REFFILE ." \
	"$FF   -vvfn -A -$REFFILE ." \
	"$FF   -vvfn -m  1728000.25s ." \
	"$FF   -vvfn -m  20.25D ." \
	"$FF   -vf   -T ." \

    do
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval "$CMD" 2>&1
	echo
    done
)


echo ======================================================================================================
echo "Test LANG set to different locales"
echo ======================================================================================================
(
    export FF_STARTTIME=20180130_000000
    for LANGLOCALE in en_US.UTF-8 es_ES.UTF-8 de_DE.UTF-8 fr_FR.UTF-8;  do
	CMD="LANG=$LANGLOCALE $FF -vvf -m 20.0025D ."
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval "$CMD" 2>&1
	echo
    done

)


################################################################################
# Create the test files with relatively recent timestamps and set the test start
# time environment variable FF_STARTTIME to a value around that time. Then call
# findfiles with various test arguments to test timezones.
################################################################################
echo ======================================================================================================
echo "Test different timezones"
echo ======================================================================================================
rm -rf $TESTBASEDIR/*
create_ts_files_YMDHMSns 2017 2017 1   1 12 1   1 1 1 \
    0 0 1   0 0 1   0 0 1   0 0 1000000000

(
    export FF_STARTTIME=20180102_000000.5
    for TIMEZONE in \
	UTC \
	Australia/Sydney \
	America/Los_Angeles \

    do
	for CMD in \
	    "TZ=$TIMEZONE $FF -vvf  -m -6M . -a 2Y ." \
	    "TZ=$TIMEZONE $FF -vvfn -m -6M . -a 2Y ." \

	do
	    echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	    eval "$CMD" 2>&1
	    echo
	done
    done
)


################################################################################
# Use the files from the previous test(s), produce output using the user-defined
# date and age format environment variables: FF_DATETIMEFORMAT and FF_AGEFORMAT
################################################################################
echo ======================================================================================================
echo "Test test changing output formats"
echo ======================================================================================================
(
    export FF_STARTTIME=20171231_000000.5
    export FF_DATETIMEFORMAT="%04d-%02d-%02dT%02d:%02d:%02dZ"
    export FF_AGEFORMAT="%7ldDays;%02ldh,%02ldm,%02lds"
    for CMD in \
	"$FF -vvvvf  -m -6M ." \
	"$FF -vvvvfn -m -6M ." \
	"$FF   -vvf  -m -6M ." \
	"$FF   -vvfn -m -6M ." \

    do
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval $CMD 2>&1
	echo
    done
)


################################################################################
# Create some directories, symbolic links and files for the tests below.
################################################################################
rm -rf $TESTBASEDIR/*
DIRL1=DirL1
mkdir ${DIRL1}
ln -s ${DIRL1} ${DIRL1}_symlink
TESTDIR=${DIRL1}
create_ts_files_YMDHMSns 2021 2021 1   1 12 6   1 1 1 \
    0 0 1   0 0 1   0 0 1   0 0 1000000000

DIRL2=DirL2
mkdir ${DIRL1}/${DIRL2}
ln -s ${DIRL2} ${DIRL1}/${DIRL2}_symlink
TESTDIR=${DIRL1}/${DIRL2}
create_ts_files_YMDHMSns 2021 2021 1   6 6 1   15 16 1 \
    0 0 1   0 0 1   0 0 1   0 0 1000000000


echo ======================================================================================================
echo "Test that symbolic links are processed correctly"
echo ======================================================================================================
(
    STARTTIME=20211231_120000.0
    for CMD in \
	"$FF -V FF_STARTTIME=$STARTTIME -vf     ${DIRL1}_symlink" \
	"$FF -V FF_STARTTIME=$STARTTIME -vf     ${DIRL1}" \
	"$FF -V FF_STARTTIME=$STARTTIME -vf     ${DIRL1}_symlink/" \
	"$FF -V FF_STARTTIME=$STARTTIME -vf  -L ${DIRL1}_symlink" \
	"$FF -V FF_STARTTIME=$STARTTIME -vfr    ${DIRL1}_symlink" \
	"$FF -V FF_STARTTIME=$STARTTIME -vfr    ${DIRL1}" \
	"$FF -V FF_STARTTIME=$STARTTIME -vfr    ${DIRL1}_symlink/" \
	"$FF -V FF_STARTTIME=$STARTTIME -vfr -L ${DIRL1}_symlink" \
	"$FF -V FF_STARTTIME=$STARTTIME -vfr    ${DIRL1} ${DIRL1}_symlink" \
	"$FF -V FF_STARTTIME=$STARTTIME -vfr -L ${DIRL1} ${DIRL1}_symlink" \

    do
	echo "========== [$CMD] =========="
	eval "$CMD" 2>&1
	echo
    done
)


echo ======================================================================================================
echo "Test all the configurable parameters as both environment variables and command line arguments."
echo ======================================================================================================
(
    export TZ=UTC
    export FF_STARTTIME=20220301_000000.1
    TIMESTAMPFORMAT="'%Y%m%d-%H%M%S'"
    TIMESTAMP=-20220301-000000
    AGEFORMAT="'%7ld(Day), %02ld%02ld%02ld'"
    DATETIMEFORMAT="'%04d%02d%02dT%02d%02d%02d'"
    INFODATETIMEFORMAT="'%c %Z'"
    for CMD in \
	"       FF_TIMESTAMPFORMAT=$TIMESTAMPFORMAT    FF_AGEFORMAT=$AGEFORMAT    FF_DATETIMEFORMAT=$DATETIMEFORMAT   $FF -vvvvf -m $TIMESTAMP ${DIRL1}" \
	"$FF -V FF_TIMESTAMPFORMAT=$TIMESTAMPFORMAT -V FF_AGEFORMAT=$AGEFORMAT -V FF_DATETIMEFORMAT=$DATETIMEFORMAT       -vvvvf -m $TIMESTAMP ${DIRL1}" \
	"       FF_TIMESTAMPFORMAT=$TIMESTAMPFORMAT    FF_INFODATETIMEFORMAT=$INFODATETIMEFORMAT                      $FF -vvvvf -m $TIMESTAMP ${DIRL1}" \
	"$FF -V FF_TIMESTAMPFORMAT=$TIMESTAMPFORMAT -V FF_INFODATETIMEFORMAT=$INFODATETIMEFORMAT                          -vvvvf -m $TIMESTAMP ${DIRL1}" \
	\
	"$FF -V FF_STARTTIME=20220301_000000.3 -vvvvf -m -$FF_STARTTIME ${DIRL1} -V FF_STARTTIME=20220301_000000.5" \
	"$FF -V FF_STARTTIME=20220301_000000.3 -vvvvf                   ${DIRL1} -V FF_STARTTIME=20220301_000000.5" \

    do
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval "$CMD" 2>&1
	echo
    done
)


echo ======================================================================================================
echo "Test handling absolute times"
echo ======================================================================================================
(
    for CMD in \
	"TZ=UTC FF_STARTTIME=19700101_002000.5 $FF -vvvvf -m 19700101_001000.3 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700101_002000.5 $FF -vvvvf -m 19700101_001000.7 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700101_001000.5 $FF -vvvvf -m 19700101_002000.3 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700101_001000.5 $FF -vvvvf -m 19700101_002000.7 ${DIRL1}" \
\
	"TZ=UTC FF_STARTTIME=19700101_000000.5 $FF -vvvvf -m 19700102_000000.3 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700101_000000.5 $FF -vvvvf -m 19700102_000000.7 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700102_000000.5 $FF -vvvvf -m 19700101_000000.3 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700102_000000.5 $FF -vvvvf -m 19700101_000000.7 ${DIRL1}" \
\
	"TZ=UTC FF_STARTTIME=19700101_000000.4 $FF -vvvvf -m 19700102_000000.3 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700101_000000.4 $FF -vvvvf -m 19700102_000000.7 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700102_000000.4 $FF -vvvvf -m 19700101_000000.3 ${DIRL1}" \
	"TZ=UTC FF_STARTTIME=19700102_000000.4 $FF -vvvvf -m 19700101_000000.7 ${DIRL1}" \

    do
	echo "========== [$CMD] =========="
	eval "$CMD" 2>&1
	echo
    done
)


echo ======================================================================================================
echo "Test handling relative times with lots of significant ns digits"
echo ======================================================================================================
(
    export TZ=UTC
    export FF_STARTTIME=20220301_000010.5
    for CMD in \
	"$FF -vvvvf -m 1s                    ${DIRL1}" \
	"$FF -vvvvf -m 1.s                   ${DIRL1}" \
	"$FF -vvvvf -m 1.0s                  ${DIRL1}" \
	"$FF -vvvvf -m 1.123s                ${DIRL1}" \
	"$FF -vvvvf -m 1.123456s             ${DIRL1}" \
	"$FF -vvvvf -m 1.123456789s          ${DIRL1}" \
	"$FF -vvvvf -m 1.123456789012s       ${DIRL1}" \
\
	"$FF -vvvvf -m 1.000123456789s       ${DIRL1}" \
	"$FF -vvvvf -m 1.000000123456789s    ${DIRL1}" \
	"$FF -vvvvf -m 1.000000000123456789s ${DIRL1}" \
\
	"$FF -vvvvf -m 1.1234.56789s         ${DIRL1}" \
	"$FF -vvvvf -m 1.1234x56789s         ${DIRL1}" \

    do
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval "$CMD" 2>&1
	echo
    done
)


echo ======================================================================================================
echo "Test handling of all the object types: Fil, Dir, Sln, Blk, Chr, FIF, Soc. There should be no 'Oth'"
echo ======================================================================================================
(
    nc -lkU ff_socket > /dev/null &	# must be in the background
    sleep 1				# wait so the next objects are garanteed to have later timestamps
    mknod ff_fifo p
    sudo mknod ff_char_device c 1 7
    sudo mknod ff_block_device b 1 7
    for CMD in \
	"$FF -fdorR -T ." \

    do
	echo "========== [$CMD] (FF_STARTTIME=$FF_STARTTIME) =========="
	eval "$CMD" 2>&1
	echo
    done
)


################################################################################
# Cleanup and exit
################################################################################
rm -rf $TESTBASEDIR
exit 0
