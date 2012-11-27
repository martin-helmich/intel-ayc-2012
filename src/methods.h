/*
 * methods.h
 *
 *  Created on: 20.11.2012
 *      Author: oerxlebe
 */

#ifndef METHODS_H_
#define METHODS_H_

#include "types.h"

bool nerver_traveled_to(Travel travel, string city);
void fill_travel(Travels *travels, Travels *final_travels, vector<Flight>& flights,
		string starting_point, unsigned long t_min, unsigned long t_max,
		CostRange *min_range, string destination_point);
void compute_path(vector<Flight>& flights, string to, vector<Travel> *travels,
		unsigned long t_min, unsigned long t_max, Parameters parameters,
		vector<Travel> *final_travels, CostRange *min_range);
void merge_path(vector<Travel>& travel1, vector<Travel>& travel2);
Travel find_cheapest(vector<Travel> *travels, vector<vector<string> > *alliances);

#endif /* METHODS_H_ */
