#!/bin/bash

IMP="TBB"

for I in {1..24} ; do
	for J in {0..9} ; do
		echo -n "$I / $P ... "
		valgrind --tool=massif --depth=1 --heap=yes --stacks=yes --massif-out-file=massif.out ../../run -nb_threads $I -from BOSTON -to MUNICH -departure_time_min 04102012000000 -departure_time_max 04142012000000 -arrival_time_min 05102012000000 -arrival_time_max 05142012000000 -max_layover 10000 -vacation_time_min 500000 -vacation_time_max 700000 -flights flights.txt -alliances alliances.txt -work_hard_file work_hard..txt -play_hard_file play_hard..txt -vacation_airports ATLANTA LONDON\ HEATHROW PHILADELPHIA RIO\ DE\ JANEIRO TORONTO
		MAXMEM=$(grep mem_heap_B= massif.out -A2 | grep '[0-9]\+' -o | tr "\n" " " | python getmaxmem.py)

		echo "$IMP;$I;$MAXMEM" >> ~/intel-benchmark-valgrindmem.csv
		echo "DONE"
	done
done
