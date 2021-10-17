#/bin/sh

################################################################################
# findfiles_regression_test: test findfiles C program (for versions >= 2.0.0)
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
#
# This script tests two versions of findfiles to detect differences in the
# output they produce when they are called "at the same time" (see below) and
# with the same command line arguments. The expectation is that this script will
# be called with the old and new versions of findfiles - and, if all goes well,
# the output should be identical.
#
# The test/debug environment variable FF_STARTTIME is used to ensure the two
# invocations of findfiles behave as if though they had been started at exactly
# the same time (to the nanosecond).
################################################################################


################################################################################
# Check arguments, set up the environment variables and call the findfile tests.
################################################################################
if [ $# -ne 2 ]; then
    echo "$0 exe#1 exe#2"
    exit
fi

if [ \! -x $1 ]; then
    echo "$1 is not an executable file, aborting"
    exit 1
fi

if [ \! -x $2 ]; then
    echo "$2 is not an executable file, aborting"
    exit 1
fi

export FF_STARTTIME=$(date +%s).5
#echo  $FF_STARTTIME

STDOUTFILE1=/tmp/ff_rt_1.out
STDERRFILE1=/tmp/ff_rt_1.err
STDOUTFILE2=/tmp/ff_rt_2.out
STDERRFILE2=/tmp/ff_rt_2.err
STDOUTDIFFS=/tmp/ff_rt_diffs

################################################################################
# Call the both versions of findfiles with the same arguments asyncrhonously
# so 
################################################################################
function compare {
    $1 $ARGS > $STDOUTFILE1 2> $STDERRFILE1 &
    $2 $ARGS > $STDOUTFILE2 2> $STDERRFILE2 &
    wait

    diff $STDOUTFILE1 $STDOUTFILE2 > $STDOUTDIFFS
    RETVAL=$?
    if [ $RETVAL -eq 0 ]; then
	printf "OK! %5d lines : %-40s\n" "$(cat /tmp/ff_rt_1.out | wc -l)" "$ARGS"
    else
	NLINES1=$(cat $STDOUTFILE1 | wc -l)
	NLINES2=$(cat $STDOUTFILE2 | wc -l)
	echo "======= $ARGS [stdout: $NLINES1 vs $NLINES2 lines] ========="
	if [ $NLINES1 -ne $NLINES2 ]; then
	    echo "****** ERROR ****** : Number of lines is different!!!!"
	fi
	cat /tmp/ff_rt_diffs
	if [ -s $STDERRFILE1 ]; then
	    echo "--- stderr 1 ---"
	    cat $STDERRFILE1
	fi
	if [ -s $STDERRFILE2 ]; then
	    echo "--- stderr 2 ---"
	    cat $STDERRFILE2
	fi
	DIFFCOUNT=$((DIFFCOUNT+1))
    fi
}

################################################################################
# Loop through all of the following findfiles sets of findfiles arguments and
# call the compare function (which calls both of the findfiles executables).
###############################################################################
DIFFCOUNT=0
for ARGS in \
    "-fv /etc" \
    "-fv -m   30D /etc" \
    "-fv -m  -30D /etc" \
    "-fv -a   30D /etc" \
    "-fv -a  -30D /etc" \
    "-fv -M  /etc/vimrc /etc" \
    "-fv -M -/etc/vimrc /etc" \
    "-fv -A  /etc/vimrc /etc" \
    "-fv -A -/etc/vimrc /etc" \
\
    "-fvsu  /etc" \
    "-fvsu  -m   30D /etc" \
    "-fvsu  -m  -30D /etc" \
    "-fvsu  -a   30D /etc" \
    "-fvsu  -a  -30D /etc" \
    "-fvsu  -M  /etc/vimrc /etc" \
    "-fvsu  -M -/etc/vimrc /etc" \
    "-fvsu  -A  /etc/vimrc /etc" \
    "-fvsu  -A -/etc/vimrc /etc" \
\
    "-fodv  /etc" \
    "-fodv  -m   30D /etc" \
    "-fodv  -m  -30D /etc" \
    "-fodv  -a   30D /etc" \
    "-fodv  -a  -30D /etc" \
    "-fodv  -M  /etc/vimrc /etc" \
    "-fodv  -M -/etc/vimrc /etc" \
    "-fodv  -A  /etc/vimrc /etc" \
    "-fodv  -A -/etc/vimrc /etc" \
\
    "-fvR /etc" \
    "-fvR -m   30D /etc" \
    "-fvR -m  -30D /etc" \
    "-fvR -a   30D /etc" \
    "-fvR -a  -30D /etc" \
    "-fvR -M  /etc/vimrc /etc" \
    "-fvR -M -/etc/vimrc /etc" \
    "-fvR -A  /etc/vimrc /etc" \
    "-fvR -A -/etc/vimrc /etc" \
\
    "-fvrn -m -3D /etc" \
\
    "-fd /etc" \
    "-fdr -D 0 /etc" \
    "-fdr -D 1 /etc" \
    "-fdr -D 2 /etc" \
\
    "-dfv -p s /" \
    "-dfvr -ip e -t /etc/X11" \
\
\
    "--files --verbose /etc" \
    "--files --verbose --mod-info  30D /etc" \
    "--files --verbose --mod-info -30D /etc" \
    "--files --verbose --acc-info  30D /etc" \
    "--files --verbose --acc-info -30D /etc" \
    "--files --verbose --mod-ref  /etc/vimrc /etc" \
    "--files --verbose --mod-ref -/etc/vimrc /etc" \
    "--files --verbose --acc-ref  /etc/vimrc /etc" \
    "--files --verbose --acc-ref -/etc/vimrc /etc" \
\
    "--files --verbose --seconds --units /etc" \
    "--files --verbose --seconds --units --mod-info  30D /etc" \
    "--files --verbose --seconds --units --mod-info -30D /etc" \
    "--files --verbose --seconds --units --acc-info  30D /etc" \
    "--files --verbose --seconds --units --acc-info -30D /etc" \
    "--files --verbose --seconds --units --mod-ref  /etc/vimrc /etc" \
    "--files --verbose --seconds --units --mod-ref -/etc/vimrc /etc" \
    "--files --verbose --seconds --units --acc-ref  /etc/vimrc /etc" \
    "--files --verbose --seconds --units --acc-ref -/etc/vimrc /etc" \
\
    "--files --others --directories --verbose /etc" \
    "--files --others --directories --verbose --mod-info  30D /etc" \
    "--files --others --directories --verbose --mod-info -30D /etc" \
    "--files --others --directories --verbose --acc-info  30D /etc" \
    "--files --others --directories --verbose --acc-info -30D /etc" \
    "--files --others --directories --verbose --mod-ref  /etc/vimrc /etc" \
    "--files --others --directories --verbose --mod-ref -/etc/vimrc /etc" \
    "--files --others --directories --verbose --acc-ref  /etc/vimrc /etc" \
    "--files --others --directories --verbose --acc-ref -/etc/vimrc /etc" \
\
    "--files --verbose --reverse /etc" \
    "--files --verbose --reverse --mod-info  30D /etc" \
    "--files --verbose --reverse --mod-info -30D /etc" \
    "--files --verbose --reverse --acc-info  30D /etc" \
    "--files --verbose --reverse --acc-info -30D /etc" \
    "--files --verbose --reverse --mod-ref  /etc/vimrc /etc" \
    "--files --verbose --reverse --mod-ref -/etc/vimrc /etc" \
    "--files --verbose --reverse --acc-ref  /etc/vimrc /etc" \
    "--files --verbose --reverse --acc-ref -/etc/vimrc /etc" \
\
    "--files --verbose --recursive --nanoseconds --mod-info -3D /etc" \
\
    "--files --directories /etc" \
    "--files --directories --recursive --depth 0 /etc" \
    "--files --directories --recursive --depth 1 /etc" \
    "--files --directories --recursive --depth 2 /etc" \
\
    "--directories --files --verbose --pattern s /" \
    "--directories --files --verbose --recursive --ignore-case --pattern e --target /etc/X11" \

do
    compare $1 $2 $ARGS
done

echo "==============================================="
echo "$DIFFCOUNT differences/problems/errors"
echo "==============================================="

################################################################################
################################################################################
for ARGS in \
    "-h" \
    "--help" \

do
    compare $1 $2 $ARGS
done
