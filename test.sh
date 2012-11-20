#!/bin/bash

function getmd5()
{
	if [ -x /sbin/md5 ] ; then
		md5 -q "$1"
	else
		md5sum "$1" | awk '{print $1}'
	fi
}

D=$(pwd)
for S in scenarios/scenario* ; do
	cd $S
	./script.sh > /dev/null

	echo -n "$S: "

	if [ "$(getmd5 work_hard.txt)" = "$(getmd5 work_hard_ref.txt)" ] ; then
		echo -n "  OK "
	else
		echo -n "FAIL "
	fi

	if [ "$(getmd5 play_hard.txt)" = "$(getmd5 play_hard_ref.txt)" ] ; then
		echo -n "  OK "
	else
		echo -n "FAIL "
	fi

	echo ""
	cd $D
done
