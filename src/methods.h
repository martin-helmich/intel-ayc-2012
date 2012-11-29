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
		CostRange *min_range, string destination_point, Alliances *alliances);
void compute_path(vector<Flight>& flights, string to, vector<Travel> *travels,
		unsigned long t_min, unsigned long t_max, Parameters parameters,
		vector<Travel> *final_travels, CostRange *min_range, Alliances *alliances);
void merge_path(Travels *results, Travels *travels1, Travels *travels2, Alliances *alliances);
Travel find_cheapest(vector<Travel> *travels, vector<vector<string> > *alliances);
bool company_are_in_a_common_alliance(const string& c1, const string& c2,
		vector<vector<string> > *alliances);
bool has_just_traveled_with_company(Flight *flight_before, Flight *current_flight);
bool has_just_traveled_with_alliance(Flight *flight_before, Flight *current_flight,
		vector<vector<string> > *alliances);

#endif /* METHODS_H_ */
