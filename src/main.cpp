/*!
 * \file main.cpp
 * \brief This file contains source code that solves the Work Hard - Play Hard problem for the Acceler8 contest
 */
#include <iostream>
#include <cstdlib>
#include <string>
#include <list>
#include <vector>
#include <fstream>
#include <time.h>
#include <cstdio>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <limits>

#define DEBUG 1

#include "types.h"
#include "methods.h"

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/tick_count.h"
#include "tbb/mutex.h"
#include "oma/loop_bodies.h"
#include "oma/tasks.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_do.h"

using namespace std;
using namespace tbb;
using namespace oma;

time_t convert_to_timestamp(int day, int month, int year, int hour, int minute,
		int seconde);
time_t convert_string_to_timestamp(string s);
void print_params(Parameters &parameters);
void print_flight(Flight& flight, float discount, ofstream& output);
void read_parameters(Parameters& parameters, int argc, char **argv);
void split_string(vector<string>& result, string line, char separator);
void parse_flight(vector<Flight>& flights, string& line);
void parse_flights(vector<Flight>& flights, string filename);
void parse_alliance(vector<string> &alliance, string line);
void parse_alliances(vector<vector<string> > &alliances, string filename);
bool company_are_in_a_common_alliance(const string& c1, const string& c2,
		vector<vector<string> > *alliances);
bool has_just_traveled_with_company(Flight& flight_before, Flight& current_flight);
bool has_just_traveled_with_alliance(Flight& flight_before, Flight& current_flight,
		vector<vector<string> > *alliances);
void apply_discount(Travel *travel, vector<vector<string> >*alliances);
float compute_cost(Travel *travel, vector<vector<string> >*alliances);
void print_alliances(vector<vector<string> > &alliances);
void print_flights(vector<Flight>& flights, vector<float> discounts, ofstream& output);
void print_travel(Travel& travel, vector<vector<string> >&alliances);
//Travel work_hard(vector<Flight>& flights, Parameters& parameters,
//		vector<vector<string> >& alliances);
Solution play_and_work_hard(vector<Flight>& flights, Parameters& parameters,
		vector<vector<string> >& alliances);
//void output_play_hard(vector<Flight>& flights, Parameters& parameters,
//		vector<vector<string> >& alliances);
//void output_work_hard(vector<Flight>& flights, Parameters& parameters,
//		vector<vector<string> >& alliances);
time_t timegm(struct tm *tm);
void print_cities();

concurrent_hash_map<string, Location> location_map;
concurrent_hash_map<string, bool> alliance_map;

/*
 *  TravelComparator Class
 */
class TravelComparator
{
public:
	Travel *cheapest_travel;
	float cheapest_cost;
	vector<Travel> *travels;
	vector<vector<string> > *alliances;

	TravelComparator(vector<Travel> *t, vector<vector<string> > *a)
	{
		cheapest_travel = NULL;
		cheapest_cost = -1;
		travels = t;
		alliances = a;
	}

	TravelComparator(TravelComparator &cc, split) :
			cheapest_cost(cc.cheapest_cost), travels(cc.travels), alliances(cc.alliances)
	{
		cheapest_travel = NULL;
	}

	void operator()(blocked_range<unsigned int> range)
	{
		float c;
		for (unsigned int i = range.begin(); i != range.end(); ++i)
		{
			c = compute_cost(&travels->at(i), alliances);
			if (cheapest_cost == -1 || c < cheapest_cost)
			{
				cheapest_cost = c;
				cheapest_travel = &travels->at(i);
			}
		}
	}

	void join(TravelComparator &tc)
	{
		if (tc.cheapest_cost != -1 && tc.cheapest_cost < cheapest_cost)
		{
			cheapest_cost = tc.cheapest_cost;
			cheapest_travel = tc.cheapest_travel;
		}
	}
};

/**
 * Solves BOTH the "Work Hard" AND the "Play Hard" problem.
 *
 * This function works in two phases:
 *
 * 1. In the first phase, all partial routes (i.e. all intermediary routes with just one
 *    origin and destination -- e.g "home to vacation", "home to conference", etc). All
 *    these partial routes are computed in parallel using task parallelism.
 *
 *    Especially the "conference to home" and "home to conference" routes are needed to solve
 *    both the "work hard" and "play hard" problems. However, since these routes are completely
 *    independent from any vacation target, they need to be computed ONLY ONCE.
 *
 * 2. In the second phase, all routes from the first phase are merged into possible solutions
 *    for the "work hard" and each of the "play hard" solutions. Then the cheapest of each
 *    set of possible solutions is computed. Just like in the first phase, this is parallelized
 *    using tasks.
 *
 * @param flights    The list of available flights.
 * @param parameters The parameters.
 * @param alliances  The alliances between companies.
 * @return           A solution object containing the "Work Hard" solution and
 *                   a list of "Play Hard" solutions (one for each vacation
 *                   destination).
 */
Solution play_and_work_hard(vector<Flight>& flights, Parameters& parameters,
		vector<vector<string> >& alliances)
{

	Solution solution;
	int n = parameters.airports_of_interest.size();
	vector<Travel> results, home_to_conference, conference_to_home, home_to_vacation[n],
			vacation_to_conference[n], conference_to_vacation[n], vacation_to_home[n];

	task_list tasks, mergereduce_tasks;

	// Compute the "conference to home" and "home to conference" routes. There routes are
	// needed to solve both the "work hard" and "play hard" problems. However, since these routes
	// are completely independent from any vacation target, they need to be computed ONLY ONCE.

	// Conference to Home
	tasks.push_back(
			*new (task::allocate_root()) FindPathTask(parameters.to, parameters.from,
					parameters.ar_time_min, parameters.ar_time_max, &parameters, &flights,
					&conference_to_home));
	// Home to Conference
	tasks.push_back(
			*new (task::allocate_root()) FindPathTask(parameters.from, parameters.to,
					parameters.dep_time_min, parameters.dep_time_max, &parameters,
					&flights, &home_to_conference));

	for (int i = 0; i < n; i++)
	{
		string current_airport_of_interest = parameters.airports_of_interest[i];

		// Home to Vacation[i]
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(parameters.from,
						current_airport_of_interest,
						parameters.dep_time_min - parameters.vacation_time_max,
						parameters.dep_time_min - parameters.vacation_time_min,
						&parameters, &flights, &home_to_vacation[i]));

		// Vacation[i] to Conference
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(current_airport_of_interest,
						parameters.to, parameters.dep_time_min, parameters.dep_time_max,
						&parameters, &flights, &vacation_to_conference[i]));

		// Conference to Vacation[i]
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(parameters.to,
						current_airport_of_interest, parameters.ar_time_min,
						parameters.ar_time_max, &parameters, &flights,
						&conference_to_vacation[i]));

		// Vacation[i] to Home
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(current_airport_of_interest,
						parameters.from,
						parameters.ar_time_max + parameters.vacation_time_min,
						parameters.ar_time_max + parameters.vacation_time_max,
						&parameters, &flights, &vacation_to_home[i]));
	}

	task::spawn_root_and_wait(tasks);

	// Merge the computed paths.
	// Solve the "work hard" problem in one task, and each "play hard" problem in another
	// seperate task.
	// These merge/reduce tasks merge the partial routes computed in the parallel step before
	// and search for the cheapest of the merged routes.
	mergereduce_tasks.push_back(
			*new (task::allocate_root()) WorkHardTask(&home_to_conference,
					&conference_to_home, &solution, &alliances));

	for (unsigned int i = 0; i < parameters.airports_of_interest.size(); i++)
	{
		mergereduce_tasks.push_back(
				*new (task::allocate_root()) PlayHardTask(&home_to_vacation[i],
						&vacation_to_conference[i], &conference_to_home,
						&home_to_conference, &vacation_to_home[i],
						&conference_to_vacation[i], &solution, i, &alliances));
	}

	// Complete all mergereduce tasks. Each task is handed a pointer to the solution object.
	// Since each task knows exactly where to modify the solution object, special access synchronization
	// is not required (apart from some tiny spinlock implementen in the "Solution" struct).
	// So ideally, the "solution" variable should be completely filled when all the tasks have run.
	task::spawn_root_and_wait(mergereduce_tasks);

	return solution;
}

/**
 * \fn void apply_discount(Travel & travel, vector<vector<string> >&alliances)
 * \brief Apply a discount when possible to the flights of a travel.
 * \param travel A travel (it will be modified to apply the discounts).
 * \param alliances The alliances.
 */
void apply_discount(Travel *travel, vector<vector<string> >*alliances)
{
	if (travel->flights.size() > 1)
	{
		for (unsigned int i = 1; i < travel->flights.size(); i++)
		{
			Flight& flight_before = travel->flights[i - 1];
			Flight& current_flight = travel->flights[i];
			if (has_just_traveled_with_company(flight_before, current_flight))
			{
				travel->discounts[i] = 0.7;
				travel->discounts[i - 1] = 0.7;
			}
			else if (has_just_traveled_with_alliance(flight_before, current_flight,
					alliances))
			{
				if (travel->discounts[i - 1] > 0.8) travel->discounts[i - 1] = 0.8;
				travel->discounts[i] = 0.8;
			}
		}
	}
}

/**
 * \fn float compute_cost(Travel & travel, vector<vector<string> >&alliances)
 * \brief Compute the cost of a travel and uses the discounts when possible.
 * \param travel The travel.
 * \param alliances The alliances.
 */
float compute_cost(Travel *travel, vector<vector<string> >*alliances)
{
	apply_discount(travel, alliances);

	// Parallelism does not make much sense here... Due to various optimizations, most
	// travels are only 3 to 6 flights in length. Scheduling overhead becomes pretty obvious
	// here...
	travel->total_cost = 0;
	for(unsigned int i=0; i < travel->flights.size(); i ++)
	{
		travel->total_cost += travel->discounts[i] * travel->flights[i].cost;
	}

	return travel->total_cost;

	/*CostComputer cc(&travel);
	parallel_reduce(blocked_range<unsigned int>(0, travel.flights.size()), cc);
	travel.total_cost = cc.costs;*/

//	return cc.costs;
}

class ParallelPathComputer
{
private:
	vector<Travel> *final_travels;
	mutex *final_travels_lock;
	Parameters parameters;
	string to;
	unsigned long t_min, t_max;
	CostRange *min_range;

public:
	ParallelPathComputer(vector<Travel> *ft, mutex *ftl, Parameters &p, string t,
			unsigned long tmi, unsigned long tma, CostRange *r)
	{
		final_travels = ft;
		final_travels_lock = ftl;
		parameters = p;
		t_min = tmi;
		t_max = tma;
		to = t;
		min_range = r;
	}

	void operator()(Travel travel, parallel_do_feeder<Travel>& f) const
	{
		Flight *current_city = &(travel.flights.back());

		//First, if a direct flight exist, it must be in the final travels
		if (current_city->to == to)
		{
			min_range->from_travel(&travel);
			final_travels->push_back(travel);
		}
		else
		{
			concurrent_hash_map<string, Location>::const_accessor a;
			if (!location_map.find(a, current_city->to))
			{
				cerr << "Fehler: Stadt " << current_city->to << " ist nicht bekannt."
						<< endl;
				exit(100);
			}

			const Location *from = &(a->second);

			/*PathComputingInnerLoop loop(travels, final_travels,
			 &from.outgoing_flights, &travels_lock, &final_travels_lock,
			 t_min, t_max, parameters, &current_city, &travel, to, &best);
			 parallel_for(
			 blocked_range<unsigned int>(0,
			 from.outgoing_flights.size()), loop);*/

			unsigned int s = from->outgoing_flights.size();
			for (unsigned int i = 0; i < s; i++)
			{
				Flight *flight = (Flight*) &(from->outgoing_flights[i]);
				if (flight->take_off_time >= t_min && flight->land_time <= t_max
						&& (flight->take_off_time > current_city->land_time)
						&& flight->take_off_time - current_city->land_time
								<= parameters.max_layover_time
						&& nerver_traveled_to(travel, flight->to)
						&& flight->cost * 0.7 + travel.min_cost <= min_range->max)
				{

					Travel newTravel = travel;
					newTravel.add_flight(*flight);

					if (flight->to == to)
					{
						final_travels_lock->lock();
						final_travels->push_back(newTravel);
						min_range->from_travel(&newTravel);
						final_travels_lock->unlock();
					}
					else
					{
						f.add(newTravel);
					}
				}
			}
		}
	}
};

/**
 * \fn void compute_path(vector<Flight>& flights, string to, vector<Travel>& travels, unsigned long t_min, unsigned long t_max, Parameters parameters)
 * \brief Computes a path from a point A to a point B. The flights must be scheduled between t_min and t_max. It is also important to take the layover in consideration.
 * 	You should try to improve and parallelize this function. A lot of stuff can probably done here. You can do almost what you want but the program output must not be modified.
 * \param flights All the flights that are available.
 * \param to The destination.
 * \param travels The list of possible travels that we are building.
 * \param t_min You must not be in a plane before this value (epoch)
 * \param t_max You must not be in a plane after this value (epoch)
 * \param parameters The program parameters
 */
void compute_path(vector<Flight>& flights, string to, vector<Travel> *travels,
		unsigned long t_min, unsigned long t_max, Parameters parameters,
		vector<Travel> *final_travels, CostRange *min_range)
{
	mutex final_travels_lock;
	ParallelPathComputer ppc(final_travels, &final_travels_lock, parameters, to, t_min, t_max, min_range);

	parallel_do(travels->begin(), travels->end(), ppc);

	return;

	//mutex final_travels_lock, travels_lock;
	//CostRange min_range = { numeric_limits<float>::max(), numeric_limits<float>::max() };
/*
// TODO: Parallele Queue?
	while (travels->size() > 0)
	{
		Travel travel = travels->back();
		Flight current_city = travel.flights.back();
		travels->pop_back();
		//First, if a direct flight exist, it must be in the final travels
		if (current_city.to == to)
		{
//			if (travel.max_cost < min_range->min)
//			{
			min_range->from_travel(&travel);
//			}
			final_travels->push_back(travel);
		}
		else
		{
			concurrent_hash_map<string, Location>::const_accessor a;
			if (!location_map.find(a, current_city.to))
			{
				cerr << "Fehler: Stadt " << current_city.to << " ist nicht bekannt."
						<< endl;
				exit(100);
			}

			Location from = a->second;

			for (unsigned int i = 0; i < from.outgoing_flights.size(); i++)
			{
				Flight flight = from.outgoing_flights[i];
				if (flight.take_off_time >= t_min && flight.land_time <= t_max
						&& (flight.take_off_time > current_city.land_time)
						&& flight.take_off_time - current_city.land_time
								<= parameters.max_layover_time
						&& nerver_traveled_to(travel, flight.to)
						&& flight.cost * 0.7 + travel.min_cost <= min_range->max)
				{
					Travel newTravel = travel;
					newTravel.add_flight(flight);

					if (flight.to == to)
					{
						final_travels->push_back(newTravel);
						min_range->from_travel(&newTravel);
					}
					else
					{
						travels->push_back(newTravel);
					}
				}
			}
		}
	}*/
}

/**
 * \fn Travel find_cheapest(vector<Travel>& travels, vector<vector<string> >&alliances)
 * \brief Finds the cheapest travel amongst the travels's vector.
 * \param travels A vector of acceptable travels
 * \param alliances The alliances
 * \return The cheapest travel found.
 */
Travel find_cheapest(Travels *travels, Alliances *alliances)
{
	int s0 = travels->size();

	OUT("Find cheapest of " << s0 << " travels.");

	if (s0 == 0)
	{
		cerr << "Travel list is empty!" << endl;
		exit(110);
	}

	TravelComparator tc(travels, alliances);
	parallel_reduce(blocked_range<unsigned int>(0, travels->size()), tc);

	return *tc.cheapest_travel;
}

/**
 * \fn void fill_travel(vector<Travel>& travels, vector<Flight>& flights, string starting_point, unsigned long t_min, unsigned long t_max)
 * \brief Fills the travels's vector with flights that take off from the starting_point.
 * This function might probably be improved.
 * \param travels A vector of travels under construction
 * \param flights All the flights that are available.
 * \param starting_point The starting point.
 * \param travels The list of possible travels that we are building.
 * \param t_min You must not be in a plane before this value (epoch).
 * \param t_max You must not be in a plane after this value (epoch).
 */
void fill_travel(Travels *travels, Travels *final_travels, vector<Flight>& flights,
		string starting_point, unsigned long t_min, unsigned long t_max,
		CostRange *min_range, string destination_point)
{
	Location l;
	concurrent_hash_map<string, Location>::const_accessor a;
	Travels temp;

	if (!location_map.find(a, starting_point))
	{
		cerr << "Location " << starting_point << " is unknown!";
		exit(120);
	}

	l = a->second;

	unsigned int s = l.outgoing_flights.size();
	for (unsigned int i = 0; i < s; i++)
	{
		if (l.outgoing_flights[i].take_off_time >= t_min
				&& l.outgoing_flights[i].land_time <= t_max
				&& l.outgoing_flights[i].cost * 0.7 <= min_range->max)
		{
			Travel t;
			t.add_flight(l.outgoing_flights[i]);

			if (l.outgoing_flights[i].to == destination_point)
			{
				min_range->from_travel(&t);
				final_travels->push_back(t);
			}
			else
			{
				temp.push_back(t);
			}
		}
	}

	s = temp.size();
	for (unsigned int i = 0; i < s; i++)
	{
		if (temp[i].min_cost <= min_range->max)
		{
			travels->push_back(temp[i]);
		}
	}
}

/**
 * \fn void merge_path(vector<Travel>& travel1, vector<Travel>& travel2)
 * \brief Merge the travel1 with the travel2 and put the result in the travel1.
 * \param travel1 The first part of the trip.
 * \param travel2 The second part of the trip.
 */
void merge_path(vector<Travel>& travel1, vector<Travel>& travel2)
{
	vector<Travel> result;
	unsigned int s1 = travel1.size(), s2;
	for (unsigned int i = 0; i < s1; i++)
	{
		Travel *t1 = &travel1[i];
		s2 = travel2.size();
		for (unsigned j = 0; j < s2; j++)
		{
			Travel *t2 = &travel2[j];
			Flight *last_flight_t1 = &t1->flights.back();
			Flight *first_flight_t2 = &t2->flights[0];
			if (last_flight_t1->land_time < first_flight_t2->take_off_time)
			{
				Travel new_travel = *t1;
				new_travel.flights.insert(new_travel.flights.end(), t2->flights.begin(),
						t2->flights.end());
				new_travel.discounts.insert(new_travel.discounts.end(),
						t2->discounts.begin(), t2->discounts.end());
				new_travel.max_cost += t2->max_cost;
				new_travel.min_cost += t2->min_cost;
				result.push_back(new_travel);
			}
		}
	}
	travel1 = result;
}

/**
 * \fn time_t convert_to_timestamp(int day, int month, int year, int hour, int minute, int seconde)
 * \brief Convert a date to timestamp
 * Parameter's names are self-sufficient. You shouldn't modify this part of the code unless you know what you are doing.
 * \return a timestamp (epoch) corresponding to the given parameters.
 */
time_t convert_to_timestamp(int day, int month, int year, int hour, int minute,
		int seconde)
{
	tm time;
	time.tm_year = year - 1900;
	time.tm_mon = month - 1;
	time.tm_mday = day;
	time.tm_hour = hour;
	time.tm_min = minute;
	time.tm_sec = seconde;
	return timegm(&time);
}

concurrent_hash_map<int, time_t> times;
mutex ol;

/**
 * \fn time_t timegm(struct tm *tm)
 * \brief Convert a tm structure into a timestamp.
 * \return a timestamp (epoch) corresponding to the given parameter.
 */
time_t timegm(struct tm *tm)
{
	char *tz;

	// Create a simple hash from the year and month.
	int year = tm->tm_year * 100 + tm->tm_mon;
	time_t month_ts;

	// On Linux, mktime() apparently requires exclusive access to some system
	// resource that is ensured by using futex locks. This makes it very hard to
	// parallelize.
	// In order to minimize the amount of these expensive mktime() calls, we only
	// compute "base dates" for each month/year combination which are subsequently
	// stored into a concurrent hash map. The actual timestamps are then computed
	// manually using the "base timestamp" plus the amount of seconds for the
	// day/hour/minute.
	concurrent_hash_map<int, time_t>::const_accessor a;
	if (times.find(a, year))
	{
		month_ts = a->second;
		a.release();
	}
	else
	{
		concurrent_hash_map<int, time_t>::accessor b;
		if (times.insert(b, year))
		{
			mutex::scoped_lock lk(ol);
			struct tm time;
			time.tm_year = tm->tm_year;
			time.tm_mon = tm->tm_mon;
			time.tm_mday = 1;
			time.tm_hour = 0;
			time.tm_min = 0;
			time.tm_sec = 0;

			tz = getenv("TZ");
			setenv("TZ", "", 1);
			tzset();

			month_ts = mktime(&time);
			b->second = month_ts;

			if (tz) setenv("TZ", tz, 1);
			else unsetenv("TZ");
			tzset();
		}
		else
		{
			month_ts = b->second;
		}
	}

	month_ts += (tm->tm_mday - 1) * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60
			+ tm->tm_sec;
	return month_ts;
}

/**
 * \fn time_t convert_string_to_timestamp(string s)
 * \brief Parses the string s and returns a timestamp (epoch)
 * \param s A string that represents a date with the following format MMDDYYYYhhmmss with
 * M = Month number
 * D = Day number
 * Y = Year number
 * h = hour number
 * m = minute number
 * s = second number
 * You shouldn't modify this part of the code unless you know what you are doing.
 * \return a timestamp (epoch) corresponding to the given parameters.
 */
time_t convert_string_to_timestamp(string s)
{
	if (s.size() != 14)
	{
		cerr << "The given string is not a valid timestamp" << endl;
		exit(0);
	}
	else
	{
		int day, month, year, hour, minute, seconde;
		day = atoi(s.substr(2, 2).c_str());
		month = atoi(s.substr(0, 2).c_str());
		year = atoi(s.substr(4, 4).c_str());
		hour = atoi(s.substr(8, 2).c_str());
		minute = atoi(s.substr(10, 2).c_str());
		seconde = atoi(s.substr(12, 2).c_str());
		return convert_to_timestamp(day, month, year, hour, minute, seconde);
	}
}

/**
 * \fn void print_params(Parameters &parameters)
 * \brief You can use this function to display the parameters
 */
void print_params(Parameters &parameters)
{
	cout << "From : " << parameters.from << endl;
	cout << "To : " << parameters.to << endl;
	cout << "dep_time_min : " << parameters.dep_time_min << endl;
	cout << "dep_time_max : " << parameters.dep_time_max << endl;
	cout << "ar_time_min : " << parameters.ar_time_min << endl;
	cout << "ar_time_max : " << parameters.ar_time_max << endl;
	cout << "max_layover_time : " << parameters.max_layover_time << endl;
	cout << "vacation_time_min : " << parameters.vacation_time_min << endl;
	cout << "vacation_time_max : " << parameters.vacation_time_max << endl;
	cout << "flights_file : " << parameters.flights_file << endl;
	cout << "alliances_file : " << parameters.alliances_file << endl;
	cout << "work_hard_file : " << parameters.work_hard_file << endl;
	cout << "play_hard_file : " << parameters.play_hard_file << endl;
//	list<string>::iterator it = parameters.airports_of_interest.begin();
//	for (; it != parameters.airports_of_interest.end(); it++)
//		cout << "airports_of_interest : " << *it << endl;
	cout << "flights : " << parameters.flights_file << endl;
	cout << "alliances : " << parameters.alliances_file << endl;
	cout << "nb_threads : " << parameters.nb_threads << endl;
}

/**
 * \fn void print_flight(Flight& flight)
 * \brief You can use this function to display a flight
 */
void print_flight(Flight& flight, float discount, ofstream& output)
{
	struct tm * take_off_t, *land_t;
	take_off_t = gmtime(((const time_t*) &(flight.take_off_time)));
	output << flight.company << "-";
	output << "" << flight.id << "-";
	output << flight.from << " (" << (take_off_t->tm_mon + 1) << "/"
			<< take_off_t->tm_mday << " " << take_off_t->tm_hour << "h"
			<< take_off_t->tm_min << "min" << ")" << "/";
	land_t = gmtime(((const time_t*) &(flight.land_time)));
	output << flight.to << " (" << (land_t->tm_mon + 1) << "/" << land_t->tm_mday << " "
			<< land_t->tm_hour << "h" << land_t->tm_min << "min" << ")-";
	output << flight.cost << "$" << "-" << discount * 100 << "%" << endl;

}

/**
 * \fn void read_parameters(Parameters& parameters, int argc, char **argv)
 * \brief This function is used to read the parameters
 * \param parameters Represents the structure that will be filled with the parameters.
 */
void read_parameters(Parameters& parameters, int argc, char **argv)
{
	for (int i = 0; i < argc; i++)
	{
		string current_parameter = argv[i];
		if (current_parameter == "-from")
		{
			parameters.from = argv[++i];
		}
		else if (current_parameter == "-arrival_time_min")
		{
			parameters.ar_time_min = convert_string_to_timestamp(argv[++i]);
		}
		else if (current_parameter == "-arrival_time_max")
		{
			parameters.ar_time_max = convert_string_to_timestamp(argv[++i]);
		}
		else if (current_parameter == "-to")
		{
			parameters.to = argv[++i];
		}
		else if (current_parameter == "-departure_time_min")
		{
			parameters.dep_time_min = convert_string_to_timestamp(argv[++i]);
		}
		else if (current_parameter == "-departure_time_max")
		{
			parameters.dep_time_max = convert_string_to_timestamp(argv[++i]);
		}
		else if (current_parameter == "-max_layover")
		{
			parameters.max_layover_time = atol(argv[++i]);
		}
		else if (current_parameter == "-vacation_time_min")
		{
			parameters.vacation_time_min = atol(argv[++i]);
		}
		else if (current_parameter == "-vacation_time_max")
		{
			parameters.vacation_time_max = atol(argv[++i]);
		}
		else if (current_parameter == "-vacation_airports")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				parameters.airports_of_interest.push_back(argv[++i]);
			}
		}
		else if (current_parameter == "-flights")
		{
			parameters.flights_file = argv[++i];
		}
		else if (current_parameter == "-alliances")
		{
			parameters.alliances_file = argv[++i];
		}
		else if (current_parameter == "-work_hard_file")
		{
			parameters.work_hard_file = argv[++i];
		}
		else if (current_parameter == "-play_hard_file")
		{
			parameters.play_hard_file = argv[++i];
		}
		else if (current_parameter == "-nb_threads")
		{
			parameters.nb_threads = atoi(argv[++i]);
		}

	}
}

/**
 * \fn void split_string(vector<string>& result, string line, char separator)
 * \brief This function split a string into a vector of strings regarding the separator.
 * \param result The vector of separated strings
 * \param line The line that must be split.
 * \param separator The separator character.
 */
void split_string(vector<string>& result, string line, char separator)
{
	while (line.find(separator) != string::npos)
	{
		size_t pos = line.find(separator);
		result.push_back(line.substr(0, pos));
		line = line.substr(pos + 1);
	}
	result.push_back(line);
}

/**
 * \fn void parse_flight(vector<Flight>& flights, string& line)
 * \brief This function parses a line containing a flight description.
 * \param flights The vector of flights.
 * \param line The line that must be parsed.
 */
void parse_flight(vector<Flight> *flights, string& line)
{
	vector<string> splittedLine;
	split_string(splittedLine, line, ';');
	if (splittedLine.size() == 7)
	{
		Flight flight;
		flight.id = splittedLine[0];
		flight.from = splittedLine[1];
		flight.take_off_time = convert_string_to_timestamp(splittedLine[2].c_str());
		flight.to = splittedLine[3];
		flight.land_time = convert_string_to_timestamp(splittedLine[4].c_str());
		flight.cost = atof(splittedLine[5].c_str());
		flight.company = splittedLine[6];
		flight.discout = 1.0;
		flights->push_back(flight);

		// Build a big graph from all locations and the flights connecting them.
		// We store all known locations (i.e. targets and origins of our flights)
		// in a hash map for fast access and all incoming and outgoing flights from
		// or to these locations in adjacency lists.
		concurrent_hash_map<string, Location>::accessor a;
		if (!location_map.insert(a, flight.from))
		{
			a->second.outgoing_flights.push_back(flight);
		}
		else
		{
			Location new_loc;

			new_loc.name = flight.from;
			new_loc.outgoing_flights.push_back(flight);
			a->second = new_loc;
		}
		a.release();

		concurrent_hash_map<string, Location>::accessor b;
		if (!location_map.insert(b, flight.to))
		{
			b->second.incoming_flights.push_back(flight);
		}
		else
		{
			Location new_loc;

			new_loc.name = flight.to;
			new_loc.incoming_flights.push_back(flight);

			b->second = new_loc;
		}
		b.release();
	}
}

class FlightParser
{
private:
	vector<Flight> *flights;
	char *input;
	vector<int>* lfs;

public:
	FlightParser(char* i, vector<int>* l) :
			input(i), lfs(l)
	{
		flights = new vector<Flight>;
	}
	FlightParser(FlightParser &fp, split) :
			input(fp.input), lfs(fp.lfs)
	{
		flights = new vector<Flight>;
	}

	void setFlights(vector<Flight> *f)
	{
		flights = f;
	}

	void operator()(const blocked_range<int> range)
	{
		for (int i = range.begin(); i != range.end(); ++i)
		{
			// Determine the length of the line by computing the difference between the
			// current LF character and the previous (does always work, since the first element
			// in the "lfs" vector is a "-1").
			// Then, allocate an appropriate amount of memory (plus 1 byte for the trailing 0-byte).
			char* b = (char*) malloc(lfs->at(i) - lfs->at(i - 1) + 1);

			// Copy the line into the previously allocated line buffer.
			strncpy(b, (input + lfs->at(i - 1) + 1), lfs->at(i) - lfs->at(i - 1) - 1);

			// Add 0-byte to mark string end.
			b[lfs->at(i) - lfs->at(i - 1) - 1] = 0x00;

			// Create string from character array and parse line.
			string s(b);
			parse_flight(flights, s);
		}
	}

	void join(FlightParser &fp)
	{
		// Merge flight lists.
		flights->insert(flights->end(), fp.flights->begin(), fp.flights->end());
	}
};

mutex rfl;

/**
 * \fn void parse_flights(vector<Flight>& flights, string filename)
 * \brief This function parses the flights from a file.
 * \param flights The vector of flights.
 * \param filename The name of the file containing the flights.
 */
void parse_flights(vector<Flight>& flights, string filename)
{
	char *m = NULL;
	struct stat stat;
	int fd;
	off_t l;

	// Try to open the input file and do a stat() syscall on it.
	// Exit with an error message if either operation fails.
	fd = open(filename.c_str(), O_RDONLY);
	if (fd && fstat(fd, &stat) != 0)
	{
		cerr << "Could not open or stat " << filename << endl;
		exit(130);
	}

	// Get file size from stat call and map the entire file into memory.
	l = stat.st_size;
	m = (char*) mmap((void*) m, l, PROT_READ, MAP_PRIVATE, fd, 0);

	// Iterate through the entire file and search for line feeds.
	// Store all linefeeds into a vector.
	vector<int> lfs;
	lfs.push_back(-1);
	for (off_t i = 38; i < l; i++)
	{
		// 10 = LF
		if (i < l && m[i] == 10)
		{
			lfs.push_back(i);

			// Each line is at least 39 characters long (assuming 14 character
			// datetime values and at least 1 character for every other value),
			// so it is safe to assume that no linefeed will occur for the next
			// 39 characters.
			i += 39;
		}
	}

	// Iterate over all found linefeeds and parse each line in parallel.
	FlightParser fp(m, &lfs);
	fp.setFlights(&flights);

//	parallel_reduce(blocked_range<int>(1, lfs.size()), fp);
	fp(blocked_range<int>(1, lfs.size()));

	// Unmap file from memory and close file handle.
	munmap(m, stat.st_size);
	close(fd);
}

/**
 * \fn void parse_alliance(vector<string> &alliance, string line)
 * \brief This function parses a line containing alliances between companies.
 * \param alliance A vector of companies sharing a same alliance.
 * \param line A line that contains the name of companies in the same alliance.
 */
void parse_alliance(vector<string> &alliance, string line)
{
	vector<string> splittedLine;
	split_string(splittedLine, line, ';');
	for (unsigned int i = 0; i < splittedLine.size(); i++)
	{
		alliance.push_back(splittedLine[i]);
	}
}
/**
 * \fn void parse_alliances(vector<vector<string> > &alliances, string filename)
 * \brief This function parses a line containing alliances between companies.
 * \param alliances A 2D vector representing the alliances. Companies on the same line are in the same alliance.
 * \param filename The name of the file containing the alliances description.
 */
void parse_alliances(vector<vector<string> > &alliances, string filename)
{
	string line = "";
	ifstream file;

	file.open(filename.c_str());
	if (!file.is_open())
	{
		cerr << "Problem while opening the file " << filename << endl;
		exit(0);
	}
	while (!file.eof())
	{
		vector<string> alliance;
		getline(file, line);
		parse_alliance(alliance, line);
		alliances.push_back(alliance);
	}
}

/**
 * \fn bool company_are_in_a_common_alliance(const string& c1, const string& c2, vector<vector<string> >& alliances)
 * \brief Check if 2 companies are in the same alliance.
 * \param c1 The first company's name.
 * \param c2 The second company's name.
 * \param alliances A 2D vector representing the alliances. Companies on the same line are in the same alliance.
 */
bool company_are_in_a_common_alliance(const string& c1, const string& c2,
		vector<vector<string> > *alliances)
{
	concurrent_hash_map<string, bool>::accessor a;
	if (!alliance_map.insert(a, c1 + c2))
	{
		for (unsigned int i = 0; i < alliances->size(); i++)
		{
			bool c1_found = false, c2_found = false;
			for (unsigned int j = 0; j < alliances->at(i).size(); j++)
			{
				if (alliances->at(i)[j] == c1) c1_found = true;
				if (alliances->at(i)[j] == c2) c2_found = true;
			}
			if (c1_found && c2_found)
			{
				a->second = true;
				return true;
			}
		}
		a->second = false;
		return false;
	}
	else
	{
		return a->second;
	}
}

/**
 * \fn bool has_just_traveled_with_company(vector<Flight>& flights_before, Flight& current_flight)
 * \brief The 2 last flights are with the same company.
 * \param flight_before The first flight.
 * \param current_flight The second flight.
 * \return The 2 flights are with the same company
 */
bool has_just_traveled_with_company(Flight& flight_before, Flight& current_flight)
{
	return flight_before.company == current_flight.company;
}

/**
 * \fn bool has_just_traveled_with_alliance(Flight& flight_before, Flight& current_flight, vector<vector<string> >& alliances)
 * \brief The 2 last flights are with the same alliance.
 * \param flight_before The first flight.
 * \param current_flight The second flight.
 * \param alliances The alliances.
 * \return The 2 flights are with the same alliance.
 */
bool has_just_traveled_with_alliance(Flight& flight_before, Flight& current_flight,
		vector<vector<string> > *alliances)
{
	return company_are_in_a_common_alliance(current_flight.company, flight_before.company,
			alliances);
}

/**
 * \fn void print_alliances(vector<vector<string> > &alliances)
 * \brief Display the alliances on the standard output.
 * \param alliances The alliances.
 */
void print_alliances(vector<vector<string> > &alliances)
{
	for (unsigned int i = 0; i < alliances.size(); i++)
	{
		cout << "Alliance " << i << " : ";
		for (unsigned int j = 0; j < alliances[i].size(); j++)
		{
			cout << "**" << alliances[i][j] << "**; ";
		}
		cout << endl;
	}
}

/**
 * \fn void print_flights(vector<Flight>& flights)
 * \brief Display the flights on the standard output.
 * \param flights The flights.
 */
void print_flights(vector<Flight>& flights, vector<float> discounts, ofstream& output)
{
	for (unsigned int i = 0; i < flights.size(); i++)
		print_flight(flights[i], discounts[i], output);
}

/**
 * \fn bool nerver_traveled_to(Travel travel, string city)
 * \brief Indicates if the city has already been visited in the travel. This function is used to avoid stupid loops.
 * \param travel The travels.
 * \apram city The city.
 * \return The current travel has never visited the given city.
 */
bool nerver_traveled_to(Travel travel, string city)
{
	for (unsigned int i = 0; i < travel.flights.size(); i++)
		if (travel.flights[i].from == city || travel.flights[i].to == city) return false;
	return true;
}

/**
 * \fn void print_travel(Travel& travel, vector<vector<string> >&alliances)
 * \brief Display a travel on the standard output.
 * \param travel The travel.
 * \param alliances The alliances (used to compute the price).
 */
void print_travel(Travel& travel, vector<vector<string> >&alliances, ofstream& output)
{
	output << "Price : " << compute_cost(&travel, &alliances) << endl;
	print_flights(travel.flights, travel.discounts, output);
	output << endl;
}

/**
 * \fn void output_play_hard(vector<Flight>& flights, Parameters& parameters, vector<vector<string> >& alliances)
 * \brief Display the solution of the "Play Hard" problem by solving it first.
 * \param flights The list of available flights.
 * \param parameters The parameters.
 * \param alliances The alliances between companies.
 */
void output_solutions(vector<Flight>& flights, Parameters& parameters,
		vector<vector<string> >& alliances)
{
	Solution solution = play_and_work_hard(flights, parameters, alliances);

	ofstream ph_out, wh_out;

	ph_out.open(parameters.play_hard_file.c_str());
	wh_out.open(parameters.work_hard_file.c_str());

	vector<string> cities = parameters.airports_of_interest;
	for (unsigned int i = 0; i < solution.play_hard.size(); i++)
	{
		ph_out << "“Play Hard” Proposition " << (i + 1) << " : " << cities[i] << endl;
		print_travel(solution.play_hard[i], alliances, ph_out);
		ph_out << endl;
	}
	ph_out.close();

	wh_out << "“Work Hard” Proposition :" << endl;
	print_travel(solution.work_hard, alliances, wh_out);
	wh_out.close();
}

/**
 * \fn void output_work_hard(vector<Flight>& flights, Parameters& parameters, vector<vector<string> >& alliances)
 * \brief Display the solution of the "Work Hard" problem by solving it first.
 * \param flights The list of available flights.
 * \param parameters The parameters.
 * \param alliances The alliances between companies.
 */
//void output_work_hard(vector<Flight>& flights, Parameters& parameters,
//		vector<vector<string> >& alliances)
//{
//	ofstream output;
//	output.open(parameters.work_hard_file.c_str());
//	Travel travel = work_hard(flights, parameters, alliances);
//	output << "“Work Hard” Proposition :" << endl;
//	print_travel(travel, alliances, output);
//	output.close();
//}
/**
 * Dumps the flight graph.
 *
 * This function dumps the entire flight graph, grouped by cities.
 */
void print_cities()
{
	concurrent_hash_map<string, Location>::iterator i;

	for (i = location_map.begin(); i != location_map.end(); ++i)
	{
		cout << i->second.name << endl;

		cout << "    OUTGOING (" << i->second.outgoing_flights.size() << "):" << endl;
		for (uint j = 0; j < i->second.outgoing_flights.size(); j++)
		{
			cout << "        ";
			print_flight(i->second.outgoing_flights[j], 1.0, (ofstream&) cout);
		}

		cout << "    INCOMING:" << endl;
		for (uint j = 0; j < i->second.incoming_flights.size(); j++)
		{
			cout << "        ";
			print_flight(i->second.incoming_flights[j], 1.0, (ofstream&) cout);
		}
	}
}

int main(int argc, char **argv)
{
	//Declare variables and read the args
	Parameters parameters;
	vector<vector<string> > alliances;
	read_parameters(parameters, argc, argv);

	task_scheduler_init init(parameters.nb_threads);

//	cout<<"Printing parameters..."<<endl;
//	print_params(parameters);
	vector<Flight> flights;
	parse_flights(flights, parameters.flights_file);
//	print_cities();
//	cout<<"Printing flights..."<<endl;
	cout << "Read " << flights.size() << " flights." << endl;
//	print_flights(flights, (ofstream&) cout);
//	cout<<"flights printed "<<endl;
	parse_alliances(alliances, parameters.alliances_file);
	cout << "Read " << alliances.size() << " alliances." << endl;
//	cout<<"Printing alliances..."<<endl;
//	print_alliances(alliances);
	tick_count t0 = tick_count::now();

	output_solutions(flights, parameters, alliances);

	tick_count t1 = tick_count::now();

	cout << "Dauer: " << (t1 - t0).seconds() * 1000 << endl;
}

//./run -from Paris -to Los\ Angeles -departure_time_min 11152012000000 -departure_time_max 11172012000000 -arrival_time_min 11222012000000 -arrival_time_max 11252012000000 -max_layover 100000 -vacation_time_min 432000 -vacation_time_max 604800 -vacation_airports Rio London Chicago -flights flights.txt -alliances alliances.txt
