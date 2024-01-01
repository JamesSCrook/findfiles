#/bin/sh

################################################################################
# findfiles_regression_test: test findfiles C program (for versions >= 2.0.0)
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
# 2023/03/14 - for findfiles version 3.1.1
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

EXE1=$1
EXE2=$2

if [ \! -x $EXE1 ]; then
    echo "$EXE1 is not an executable file, aborting"
    exit 1
fi

if [ \! -x $EXE2 ]; then
    echo "$EXE2 is not an executable file, aborting"
    exit 1
fi

export FF_STARTTIME=$(date +%Y%m%d_%H%M%S).5

STDOUTFILE1=/tmp/ff_rt_1_$$.out
STDERRFILE1=/tmp/ff_rt_1_$$.err
STDOUTFILE2=/tmp/ff_rt_2_$$.out
STDERRFILE2=/tmp/ff_rt_2_$$.err
STDOUTDIFFS=/tmp/ff_rt_stdout_$$.dif
STDERRDIFFS=/tmp/ff_rt_stderr_$$.dif

################################################################################
# Call the both versions of findfiles with the same arguments asyncrhonously.
# EXE1, EXE2 and ARGS are global variables, so compare has no arguments.
################################################################################
function compare {
    eval $EXE1 $ARGS > $STDOUTFILE1 2> $STDERRFILE1 &
    eval $EXE2 $ARGS > $STDOUTFILE2 2> $STDERRFILE2 &
    wait
    
    diff $STDOUTFILE1 $STDOUTFILE2 > $STDOUTDIFFS
    STDOUTDIFFRETVAL=$?
    STDOUTNUMLNS1=$(cat $STDOUTFILE1 | wc -l)
    STDOUTNUMLNS2=$(cat $STDOUTFILE2 | wc -l)

    diff $STDERRFILE1 $STDERRFILE2 > $STDERRDIFFS
    STDERRDIFFRETVAL=$?
    STDERRNUMLNS1=$(cat $STDERRFILE1 | wc -l)
    STDERRNUMLNS2=$(cat $STDERRFILE2 | wc -l)

    # handle all 4 cases of stdouts and stderr matching or not
    if [ $STDOUTDIFFRETVAL -eq 0 ]; then	# do the stdouts match?
	if [ $STDERRDIFFRETVAL -eq 0 ]; then	# the stdouts match, do two stderrs match too?
	    printf "[SAME] %5d/%5d stdout/stderr identilcal lines: %-40s\n" $STDOUTNUMLNS1 $STDERRNUMLNS1 "[$ARGS]"
	else					# stdouts match, stderrs differ - display those details
	    DIFFCOUNT=$((DIFFCOUNT+1))
	    echo "=================== stderr is different ====================== $DIFFCOUNT"
	    printf "[DIFFERENT] %5d != %5d stderr lines: %-40s\n" $STDERRNUMLNS1 $STDERRNUMLNS2 "[$ARGS]"
	    echo "------------------- stderr diffs ---------------------"
	    cat $STDERRDIFFS
	fi
    else
	DIFFCOUNT=$((DIFFCOUNT+1))		# the stdouts differ, display those details
	echo "=================== stdout is different ====================== $DIFFCOUNT"
	printf "[DIFFERENT] %5d != %5d stdout lines: %-40s\n" $STDOUTNUMLNS1 $STDOUTNUMLNS2 "[$ARGS]"
	echo "------------------- stdout diffs ---------------------"
	cat $STDOUTDIFFS
	if [ $STDERRDIFFRETVAL -ne 0 ]; then	# if the stderr also differs, display those details
	    echo "------------------- stderr diffs ---------------------"
	    cat $STDERRDIFFS
	fi
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
    "-fp '^[ghp]' -X '^group$' -X '^passwd-$' /etc" \
    "-fp '^tty' /dev -p ^passwd /etc" \
\
    "-vfh /etc" \
    "-vfH /etc" \
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
\
    "" \

do
    compare	# Note: EXE1 EXE2 and ARGS are global variables!
done

echo "==============================================="
echo "$DIFFCOUNT differences/problems/errors"
echo "==============================================="

rm -f $STDOUTFILE1 $STDERRFILE1 $STDOUTFILE2 $STDERRFILE2 $STDOUTDIFFS $STDERRDIFFS

exit 0
