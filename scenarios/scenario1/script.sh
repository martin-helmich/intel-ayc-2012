#!/bin/bash

../../run -nb_threads 2 -from Paris -to Los\ Angeles -departure_time_min 10302012000000 -departure_time_max 11022012000000 -arrival_time_min 11082012000000 -arrival_time_max 11112012000000 -max_layover 14400 -vacation_time_min 432000 -vacation_time_max 604800 -flights flights.txt -alliances alliances.txt -work_hard_file work_hard.txt -play_hard_file play_hard.txt

