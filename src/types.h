/*
 * types.h
 *
 *  Created on: 01.11.2012
 *      Author: mhelmich
 */

#ifndef TYPES_H_
#define TYPES_H_


#include <string>
#include <vector>
#include <list>

using namespace std;


/**
 * \struct Parameters
 * \brief Store the program's parameters.
 * This structure don't need to be modified but feel free to change it if you want.
 */
struct Parameters{
	string from;/*!< The city where the travel begins */
	string to;/*!< The city where the conference takes place */
	unsigned long dep_time_min;/*!< The minimum departure time for the conference (epoch). No flight towards the conference's city must be scheduled before this time. */
	unsigned long dep_time_max;/*!< The maximum departure time for the conference (epoch). No flight towards the conference's city must be scheduled after this time.  */
	unsigned long ar_time_min;/*!< The minimum arrival time after the conference (epoch). No flight from the conference's city must be scheduled before this time.  */
	unsigned long ar_time_max;/*!< The maximum arrival time after the conference (epoch). No flight from the conference's city must be scheduled after this time.  */
	unsigned long max_layover_time;/*!< You don't want to wait more than this amount of time at the airport between 2 flights (in seconds) */
	unsigned long vacation_time_min;/*!< Your minimum vacation time (in seconds). You can't be in a plane during this time. */
	unsigned long vacation_time_max;/*!< Your maximum vacation time (in seconds). You can't be in a plane during this time. */
	list<string> airports_of_interest;/*!< The list of cities you are interested in. */
	string flights_file;/*!< The name of the file containing the flights. */
	string alliances_file;/*!< The name of the file containing the company alliances. */
	string work_hard_file;/*!< The file used to output the work hard result. */
	string play_hard_file;/*!< The file used to output the play hard result. */
	int nb_threads;/*!< The maximum number of worker threads */
};

/**
 * \struct Flight
 * \brief Store a single flight data.
 *This structure don't need to be modified but feel free to change it if you want.
 */
struct Flight{
	string id;/*!< Unique id of the flight. */
	string from;/*!< City where you take off. */
	string to;/*!< City where you land. */
	unsigned long take_off_time;/*!< Take off time (epoch). */
	unsigned long land_time;/*!< Land time (epoch). */
	string company;/*!< The company's name. */
	float cost;/*!< The cost of the flight. */
	float discout;/*!< The discount applied to the cost. */
};

/**
 * \struct Travel
 * \brief Store a travel.
 * This structure don't need to be modified but feel free to change it if you want.
 */
struct Travel{
	vector<Flight> flights;/*!< A travel is just a list of Flight(s). */
	float total_cost; /* Total costs of this travel (sum of flight costs minus possible discounts). */
};

struct Location {
	string name;
	vector<Flight> outgoing_flights;
	vector<Flight> incoming_flights;
};


#endif /* TYPES_H_ */
