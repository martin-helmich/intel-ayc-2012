#!/bin/bash

D=$(pwd)
for S in scenarios/scenario[0-9] ; do
	cd $S
	./script.sh > /dev/null

	echo -n "$S: "

	if [ "$(md5 -q work_hard.txt)" = "$(md5 -q work_hard_ref.txt)" ] ; then
		echo -n "  OK "
	else
		echo -n "FAIL "
	fi

	if [ "$(md5 -q play_hard.txt)" = "$(md5 -q play_hard_ref.txt)" ] ; then
		echo -n "  OK "
	else
		echo -n "FAIL "
	fi

	echo ""
	cd $D
done
