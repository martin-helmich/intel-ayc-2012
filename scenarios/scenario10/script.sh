#!/bin/bash

../../run -nb_threads 2 -from CHICAGO -to SEATTLE -departure_time_min 04182012000000 -departure_time_max 04302012000000 -arrival_time_min 12102012000000 -arrival_time_max 12292012000000 -max_layover 14400 -vacation_time_min 43200 -vacation_time_max 60480000 -flights flights.txt -alliances alliances.txt -work_hard_file work_hard.txt -play_hard_file play_hard.txt -vacation_airports DENVER
