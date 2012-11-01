#!/bin/bash

#7 days is equal to 604800 seconds
#5 days is equal to 432000 seconds

#Tests the output when 2 vacation destinations are setted
../../run -nb_threads 2 -from Paris -to Los\ Angeles -departure_time_min 10302012000000 -departure_time_max 11022012000000 -arrival_time_min 11082012000000 -arrival_time_max 11112012000000 -max_layover 14400 -vacation_time_min 432000 -vacation_time_max 604800 -vacation_airports Rio Madrid -flights flights.txt -alliances alliances.txt -work_hard_file work_hard.txt -play_hard_file play_hard.txt

