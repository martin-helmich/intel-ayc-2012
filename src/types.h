/*!
 * @file types.h
 * @brief This file contains class declarations for model classes.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#ifndef TYPES_H_
#define TYPES_H_

#ifdef DEBUG
#define OUT(a) cout << a << endl;
#else
#define OUT(a)
#endif

#include <string>
#include <vector>
#include <list>
#include <limits>
#include <cmath>

#include "tbb/spin_mutex.h"

using namespace std;

/**
 * @brief Store the program's parameters.
 */
struct Parameters
{
	string from;/*!< The city where the travel begins */
	string to;/*!< The city where the conference takes place */
	unsigned long dep_time_min;/*!< The minimum departure time for the conference (epoch). No flight towards the conference's city must be scheduled before this time. */
	unsigned long dep_time_max;/*!< The maximum departure time for the conference (epoch). No flight towards the conference's city must be scheduled after this time.  */
	unsigned long ar_time_min;/*!< The minimum arrival time after the conference (epoch). No flight from the conference's city must be scheduled before this time.  */
	unsigned long ar_time_max;/*!< The maximum arrival time after the conference (epoch). No flight from the conference's city must be scheduled after this time.  */
	unsigned long max_layover_time;/*!< You don't want to wait more than this amount of time at the airport between 2 flights (in seconds) */
	unsigned long vacation_time_min;/*!< Your minimum vacation time (in seconds). You can't be in a plane during this time. */
	unsigned long vacation_time_max;/*!< Your maximum vacation time (in seconds). You can't be in a plane during this time. */
	vector<string> airports_of_interest;/*!< The list of cities you are interested in. */
	string flights_file;/*!< The name of the file containing the flights. */
	string alliances_file;/*!< The name of the file containing the company alliances. */
	string work_hard_file;/*!< The file used to output the work hard result. */
	string play_hard_file;/*!< The file used to output the play hard result. */
	int nb_threads;/*!< The maximum number of worker threads */
};

/**
 * @brief Store a single flight data.
 */
struct Flight
{
	string id;/*!< Unique id of the flight. */
	string from;/*!< City where you take off. */
	string to;/*!< City where you land. */
	unsigned long take_off_time;/*!< Take off time (epoch). */
	unsigned long land_time;/*!< Land time (epoch). */
	string company;/*!< The company's name. */
	float cost;/*!< The cost of the flight. */
	float discout;/*!< The discount applied to the cost. */
};

// Yes, we are lazy and don't want to type "vector<vector<string> >" too often... ;)
typedef vector<vector<string> > Alliances;

/// Models a travel and associated application logic.
class Travel
{
public:
	/// Flights contained in this travel.
	/** A travel is (not anymore!) just a list of Flight(s). */
	vector<Flight> flights;

	/// Discounts applied to each flight.
	/** Discounts applied to each flight. Due to parallel processing,
	 *  we cannot store the discount directly in the Flight objects. */
	vector<float> discounts;

	/// Total costs of this travel.
	/** The total cost of this travel. This is the sum of flight costs
	 *  minus all possible discounts. The actual total costs of a travel
	 *  can only be computed, when it is guaranteed that no flights are
	 *  added any more (otherwise the costs can change due to discounts). */
	float total_cost;

	/// Minimal costs of this travel.
	/** The minimal costs is the sum of all travel costs with highest
	 *  possible discount. */
	float min_cost;

	/// Maximal costs of this travel.
	/** The maximal costs is the sum of this travel (i.e. all travel costs
	 *  with lowest possible discount). */
	float max_cost;

	/// Helper storing the size of the travel.
	int size;

	/// Creates a new travel.
	Travel() :
			total_cost(0), min_cost(0), max_cost(0), size(0)
	{
	}

	/// Adds a new flight to this travel.
	/** @param f The flight to be added.
	 *  @param a A list of allicances. Is needed, because this function takes discouts
	 *           into account. */
	void add_flight(Flight &f, Alliances *a);

	/// Merges two travels into one.
	/** @param t The travel to be merged.
	 *  @param a The list of allicances. Is needed, because this function takes discouts
	 *           into account. */
	void merge_travel(Travel *t, Alliances *a);

	/// Prints a textual representation of this travel to STDOUT.
	void print();
};

typedef vector<Travel> Travels;

/// Models a location and associated application logic.
/** This class models a single location (i.e. a possible flight origin
 *  or destination). In our flight graph, the locations are nodes, flights
 *  are edges.
 *
 *  Each location stores a list of all incoming and outgoing flights for
 *  quick access. */
class Location
{
public:
	/// The location's name.
	string name;

	/// Outgoing flights.
	vector<Flight> outgoing_flights;

	/// Incoming flights.
	vector<Flight> incoming_flights;
};

/// Models the program's solution.
/** This class models the program's solution. It contains one travel as
 *  solution for the "work hard" problems and a list of n travels as solution
 *  for the "play hard" problems. */
class Solution
{
private:
	tbb::spin_mutex lock;
public:
	/// The "play hard" solutions.
	Travel *play_hard;
	/// The "work hard" solutions.
	Travel work_hard;

	/// Creates a new solution object.
	/** @param s Number of "play hard" solutions. */
	Solution(unsigned int s);

	/// Adds a new "play hard" solution.
	/** @param i The index of the "play hard" solution.
	 *  @param t The travel to be added as the solution. */
	void add_play_hard(unsigned int i, Travel &t);
};

/// Models a dynamic cost range.
/** This class models a dynamic cost range. This is necessary due to the
 *  uncertainty in travel prices originating from possible discounts.
 *
 *  This class stores a minimum and a maximum price. */
class CostRange
{
private:
	/// Spinlock to protect against concurrent access.
	tbb::spin_mutex lock;
public:
	/// Minimum price.
	int min;
	/// Maximum price.
	int max;

	/// Creates a new price range.
	CostRange();

	/// Sets min and max prices from an existing travel object.
	/** @param t The travel from which the price range is to be set. */
	void from_travel(Travel *t);

};

#endif /* TYPES_H_ */
