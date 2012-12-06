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
printf "\033[1;33m%-20s\033[0m : \033[1;33mWORK\033[0m \033[1;33mPLAY\033[0m\n" "SCENARIO"

for S in scenarios/scenario{1..12} ; do
	cd $S

	rm play_hard.txt work_hard.txt

	./script.sh &> /dev/null

	printf "%-20s : " $S
	#echo -n "$S: "

	if [ "$(getmd5 work_hard.txt)" = "$(getmd5 work_hard_ref.txt)" ] ; then
		echo -ne "  \033[1;32mOK \033[0m"
	else
		echo -e "\033[1;31mFAIL \033[0m"
		diff -u work_hard_ref.txt work_hard.txt
	fi

	if [ "$(getmd5 play_hard.txt)" = "$(getmd5 play_hard_ref.txt)" ] ; then
		echo -ne "  \033[1;32mOK \033[0m"
	else
		echo -e "\033[1;31mFAIL \033[0m"
		diff -u play_hard_ref.txt play_hard.txt
	fi

	echo ""
	cd $D
done
