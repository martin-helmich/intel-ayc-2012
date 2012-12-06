/*!
 * \file main.cpp
 * \brief This file contains source code that solves the Work Hard - Play Hard problem for the Acceler8 contest
 */
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

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

concurrent_hash_map<string, Location> *location_map;
concurrent_hash_map<string, bool> alliance_map;
concurrent_hash_map<int, time_t> times;

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
Solution play_and_work_hard(vector<Flight> *flights, Parameters& parameters,
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
					parameters.ar_time_min, parameters.ar_time_max, &parameters, flights,
					&conference_to_home, &alliances));
	// Home to Conference
	tasks.push_back(
			*new (task::allocate_root()) FindPathTask(parameters.from, parameters.to,
					parameters.dep_time_min, parameters.dep_time_max, &parameters,
					flights, &home_to_conference, &alliances));

	for (int i = 0; i < n; i++)
	{
		string current_airport_of_interest = parameters.airports_of_interest[i];
		concurrent_hash_map<string, Location>::const_accessor a;

		// Small optimization: If no route from or to vacation location exist, do not
		// bother to find routes, since it would be impossible to find any, anyhow.
		if (! location_map->find(a, current_airport_of_interest)
				|| a->second.outgoing_flights.size() == 0 || a->second.incoming_flights.size() == 0)
		{
			a.release();
			continue;
		}
		a.release();

		// Home to Vacation[i]
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(parameters.from,
						current_airport_of_interest,
						parameters.dep_time_min - parameters.vacation_time_max,
						parameters.dep_time_min - parameters.vacation_time_min,
						&parameters, flights, &home_to_vacation[i], &alliances));

		// Vacation[i] to Conference
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(current_airport_of_interest,
						parameters.to, parameters.dep_time_min, parameters.dep_time_max,
						&parameters, flights, &vacation_to_conference[i], &alliances));

		// Conference to Vacation[i]
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(parameters.to,
						current_airport_of_interest, parameters.ar_time_min,
						parameters.ar_time_max, &parameters, flights,
						&conference_to_vacation[i], &alliances));

		// Vacation[i] to Home
		tasks.push_back(
				*new (task::allocate_root()) FindPathTask(current_airport_of_interest,
						parameters.from,
						parameters.ar_time_max + parameters.vacation_time_min,
						parameters.ar_time_max + parameters.vacation_time_max,
						&parameters, flights, &vacation_to_home[i], &alliances));
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
		string current_airport_of_interest = parameters.airports_of_interest[i];
		concurrent_hash_map<string, Location>::const_accessor a;

		// Small optimization: If no route from or to vacation location exist, do not
		// bother to find routes, since it would be impossible to find any, anyhow.
		if (! location_map->find(a, current_airport_of_interest)
				|| a->second.outgoing_flights.size() == 0 || a->second.incoming_flights.size() == 0)
		{
			Travel t;
			a.release();
			solution.add_play_hard(i, t);
			continue;
		}
		a.release();

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
 * \fn float compute_cost(Travel & travel, vector<vector<string> >&alliances)
 * \brief Compute the cost of a travel and uses the discounts when possible.
 * \param travel The travel.
 * \param alliances The alliances.
 */
float compute_cost(Travel *travel, vector<vector<string> >*alliances)
{
	// Parallelism does not make much sense here... Due to various optimizations, most
	// travels are only 3 to 6 flights in length. Scheduling overhead becomes pretty obvious
	// here...
	travel->total_cost = 0;
	for (unsigned int i = 0; i < travel->flights.size(); i++)
	{
		travel->total_cost += travel->discounts[i] * travel->flights[i].cost;
	}

	return travel->total_cost;
}

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
		vector<Travel> *final_travels, CostRange *min_range, Alliances *alliances)
{
	mutex final_travels_lock;
	ComputePathOuterLoop cpol(final_travels, &final_travels_lock, parameters, to, t_min,
			t_max, min_range, alliances, location_map);

	parallel_do(travels->begin(), travels->end(), cpol);

	return;
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
		CostRange *min_range, string destination_point, Alliances *alliances)
{
	const Location *l;
	concurrent_hash_map<string, Location>::const_accessor a;
	Travels temp;

	if (!location_map->find(a, starting_point))
	{
		cerr << "Location " << starting_point << " is unknown!";
		return;
	}

	l = &(a->second);

	unsigned int s = l->outgoing_flights.size();
	for (unsigned int i = 0; i < s; i++)
	{
		Flight *f = (Flight*) &(l->outgoing_flights[i]);
		if (f->take_off_time >= t_min && f->land_time <= t_max
				&& f->cost * 0.7 <= min_range->max)
		{
			Travel t;
			t.add_flight(*f, alliances);

			if (f->to == destination_point)
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

	FilterPathsLoop fpl(&temp, travels, min_range);

	if (temp.size() > 500) parallel_reduce(blocked_range<unsigned int>(0, temp.size()),
			fpl);
	else fpl(blocked_range<unsigned int>(0, temp.size()));
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

/**
 * \fn time_t timegm(struct tm *tm)
 * \brief Convert a tm structure into a timestamp.
 * \return a timestamp (epoch) corresponding to the given parameter.
 */
time_t timegm(struct tm *tm)
{
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
	// EDIT: Apparently, for larger input files (> 200000 flights), mktime is still
	// making trouble (by mixing up timezones on occasion). In order to avoid these
	// problems altogether, we compute the timestamp COMPLETELY manually (yes, this is
	// pain; luckily we can at least ignore the timezone problem -- it's all UTC,
	// after all).
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
			int years = tm->tm_year > 100 ? tm->tm_year - 70 : tm->tm_year;

			// Build offset for year (not yet considering leap years).
			unsigned long offset = years * 31536000; // = 365 * 24 * 60 * 60

			// Add additional days to offset for each passed leap year.
			for (int y = years; y >= 0; y--)
			{
				if (((y + 1970) % 4 == 0) && ((y + 1970) % 100 != 0))
				{
					offset += 86400;
				}
			}

			// Add additional days to offset for each passed month. Have to consider
			// different month lengths and leap years.
			for (int m = 0; m < tm->tm_mon; m++)
			{
				bool is_leap_year = (tm->tm_year + 1900 % 4 == 0)
						&& (tm->tm_year + 1900 % 100 != 0);

				if (m == 1) offset += (is_leap_year ? 29 : 28) * 86400;
				else if ((m < 7 && m % 2 == 0) || (m >= 7 && m % 2 == 1)) offset += 31
						* 86400;
				else offset += 30 * 86400;
			}

			if (tm->tm_mon != 1) offset += 86400;

			b->second = offset;
			month_ts = offset;
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
	vector<string>::iterator it = parameters.airports_of_interest.begin();
	for (; it != parameters.airports_of_interest.end(); it++)
		cout << "airports_of_interest : " << *it << endl;
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
		if (!location_map->insert(a, flight.from))
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
		if (!location_map->insert(b, flight.to))
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

/**
 * \fn void parse_flights(vector<Flight>& flights, string filename)
 * \brief This function parses the flights from a file.
 * \param flights The vector of flights.
 * \param filename The name of the file containing the flights.
 */
void parse_flights(vector<Flight> *flights, string filename)
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
	ParseFlightsLoop pfl(m, &lfs, flights);
	parallel_reduce(blocked_range<int>(1, lfs.size()), pfl);

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
 * Check if 2 companies are in the same alliance.
 *
 * Based on the assumption that there are relatively few possible combinations
 * of airlines, this function used a cache based on a "tbb::concurrent_hash_map<string,bool>"
 * in which each combination of airlines is stored. Each entry is created with the
 * first call with a certain company combination.
 *
 * @param c1        The first company's name.
 * @param c2        The second company's name.
 * @param alliances A 2D vector representing the alliances. Companies on the
 *                  same line are in the same alliance.
 */
bool company_are_in_a_common_alliance(const string& c1, const string& c2,
		vector<vector<string> > *alliances)
{
	concurrent_hash_map<string, bool>::accessor a;
	if (alliance_map.insert(a, c1 < c2 ? c1 + c2 : c2 + c1))
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
bool has_just_traveled_with_company(Flight *flight_before, Flight *current_flight)
{
	return flight_before->company == current_flight->company;
}

/**
 * \fn bool has_just_traveled_with_alliance(Flight& flight_before, Flight& current_flight, vector<vector<string> >& alliances)
 * \brief The 2 last flights are with the same alliance.
 * \param flight_before The first flight.
 * \param current_flight The second flight.
 * \param alliances The alliances.
 * \return The 2 flights are with the same alliance.
 */
bool has_just_traveled_with_alliance(Flight *flight_before, Flight *current_flight,
		vector<vector<string> > *alliances)
{
	return company_are_in_a_common_alliance(current_flight->company,
			flight_before->company, alliances);
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
	output << "Price : " << compute_cost(&travel, &alliances)/*ravel.max_cost*/<< endl;
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
void output_solutions(vector<Flight> *flights, Parameters& parameters,
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
 * Dumps the flight graph.
 *
 * This function dumps the entire flight graph, grouped by cities.
 */
void print_cities()
{
	concurrent_hash_map<string, Location>::iterator i;

	for (i = location_map->begin(); i != location_map->end(); ++i)
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

	vector<Flight> *flights = new vector<Flight>;
	location_map = new concurrent_hash_map<string, Location>;

	parse_flights(flights, parameters.flights_file);
	cout << "Read " << flights->size() << " flights." << endl;
	parse_alliances(alliances, parameters.alliances_file);
	cout << "Read " << alliances.size() << " alliances." << endl;
	tick_count t0 = tick_count::now();

	output_solutions(flights, parameters, alliances);

	tick_count t1 = tick_count::now();

	cout << "Duration: " << (t1 - t0).seconds() * 1000 << endl;
}

//./run -from Paris -to Los\ Angeles -departure_time_min 11152012000000 -departure_time_max 11172012000000 -arrival_time_min 11222012000000 -arrival_time_max 11252012000000 -max_layover 100000 -vacation_time_min 432000 -vacation_time_max 604800 -vacation_airports Rio London Chicago -flights flights.txt -alliances alliances.txt
