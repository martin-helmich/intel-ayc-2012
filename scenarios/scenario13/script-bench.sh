#!/bin/bash

IMP="OMP"

for I in {1..24} ; do
	for J in {0..9} ; do
		echo -n "$I / $P ... "
		{ time -p ../../run -nb_threads $I -from C1 -to C100 -departure_time_min 00000000100000 -departure_time_max 00000000450000 -arrival_time_min 00000000550000 -arrival_time_max 00000000900000 -max_layover 1000 -vacation_time_min 50000 -vacation_time_max 200000 -flights flights.txt -alliances alliances.txt -work_hard_file work_hard.txt -play_hard_file play_hard.txt -vacation_airports C2 C3 C4 C5 C6 C7 C8 C9 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26 C27 C28 C29 C30 C31 C32 C33 C34 C35 C36 C37 C38 C39 C40 C41 C42 C43 C44 C45 C46 C47 C48 C49 C50 C51  ; } 2>&1 | awk "BEGIN { WALL=\"\" ; USER=\"\"; } { if(\$1 == \"real\") WALL=\$2 ; if(\$1 == \"user\") USER=\$2 } END { print \"$IMP;$I;\" WALL \";\" USER \";\" (USER/WALL) }" >> ~/intel-benchmark-13.csv
		echo "DONE"
	done
done
