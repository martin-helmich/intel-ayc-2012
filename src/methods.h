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
bool company_are_in_a_common_alliance(const string& c1, const string& c2,
		vector<vector<string> > *alliances);
bool has_just_traveled_with_company(Flight *flight_before, Flight *current_flight);
bool has_just_traveled_with_alliance(Flight *flight_before, Flight *current_flight,
		vector<vector<string> > *alliances);
time_t convert_to_timestamp(int day, int month, int year, int hour, int minute,
		int seconde);
time_t convert_string_to_timestamp(string s);
void print_params(Parameters &parameters);
void print_flight(Flight& flight, float discount, ofstream& output);
void read_parameters(Parameters& parameters, int argc, char **argv);
void split_string(vector<string>& result, string line, char separator);
void parse_flight(vector<Flight> *flights, string& line);
void parse_flights(vector<Flight> *flights, string filename);
void parse_alliance(vector<string> &alliance, string line);
void parse_alliances(vector<vector<string> > &alliances, string filename);
float compute_cost(Travel *travel, vector<vector<string> >*alliances);
void print_alliances(vector<vector<string> > &alliances);
void print_flights(vector<Flight>& flights, vector<float> discounts, ofstream& output);
void print_travel(Travel& travel, vector<vector<string> >&alliances);
Solution play_and_work_hard(vector<Flight> *flights, Parameters& parameters,
		vector<vector<string> >& alliances);
time_t timegm(struct tm *tm);
void print_cities();

#endif /* METHODS_H_ */
