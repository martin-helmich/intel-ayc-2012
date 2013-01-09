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

#ifdef OMA_OPENMP
#include <omp.h>
#endif

#include "types.h"
#include "methods.h"

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/tick_count.h"
#include "tbb/mutex.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_do.h"

#include "oma/loop_bodies.h"
#include "oma/tasks.h"

using namespace std;
using namespace tbb;
using namespace oma;

concurrent_hash_map<string, Location> *location_map;
concurrent_hash_map<string, bool> alliance_map;
concurrent_hash_map<int, time_t> times;

/// Solves BOTH the "Work Hard" AND the "Play Hard" problem.
/** This function solves both the "work hard" AND the "play hard" problem.
 *  It works in two phases:
 *
 *  1. In the first phase, all partial routes (i.e. all intermediary routes
 *     with just one origin and destination -- e.g "home to vacation", "home to
 *     conference", etc). All these partial routes are computed in parallel
 *     using task parallelism.
 *
 *     Especially the "conference to home" and "home to conference" routes are
 *     needed to solve both the "work hard" and "play hard" problems. However,
 *     since these routes are completely independent from any vacation target,
 *     they need to be computed ONLY ONCE.
 *
 *  2. In the second phase, all routes from the first phase are merged into
 *     possible solutions for the "work hard" and each of the "play hard"
 *     solutions. Then the cheapest of each set of possible solutions is
 *     computed. Just like in the first phase, this is parallelized using tasks.
 *
 *  @param parameters The parameters.
 *  @param alliances  The alliances between companies.
 *  @return           A solution object containing the "Work Hard" solution and
 *                    a list of "Play Hard" solutions (one for each vacation
 *                    destination). */
Solution play_and_work_hard(Parameters& parameters, Alliances *alliances)
{
	int n = parameters.airports_of_interest.size();
	Solution solution(n);
	vector<Travel> results, home_to_conference, conference_to_home, home_to_vacation[n],
			vacation_to_conference[n], conference_to_vacation[n], vacation_to_home[n];

	// Compute the "conference to home" and "home to conference" routes. There routes are
	// needed to solve both the "work hard" and "play hard" problems. However, since these routes
	// are completely independent from any vacation target, they need to be computed ONLY ONCE.

	#pragma omp parallel default(shared)
	{
		#pragma omp single
		{
			// Conference to Home
			#pragma omp task default(shared)
			find_path_task(parameters.to, parameters.from, parameters.ar_time_min, parameters.ar_time_max,
					&parameters, &conference_to_home, alliances);

			// Home to Conference
			#pragma omp task default(shared)
			find_path_task(parameters.from, parameters.to, parameters.dep_time_min, parameters.dep_time_max,
					&parameters, &home_to_conference, alliances);

			for (int i = 0; i < n; i++)
			{
				string current_airport_of_interest = parameters.airports_of_interest[i];
				concurrent_hash_map<string, Location>::const_accessor a;

				// Small optimization: If no route from or to vacation location exist, do not
				// bother to find routes, since it would be impossible to find any, anyhow.
				if (!location_map->find(a, current_airport_of_interest)
						|| a->second.outgoing_flights.size() == 0
						|| a->second.incoming_flights.size() == 0)
				{
					a.release();
					continue;
				}
				a.release();

				#pragma omp task default(shared) firstprivate(i,current_airport_of_interest)
				{
					Travels *htv = &(home_to_vacation[i]);
					find_path_task(parameters.from, current_airport_of_interest,
							parameters.dep_time_min - parameters.vacation_time_max,
							parameters.dep_time_min - parameters.vacation_time_min, &parameters,
							htv, alliances);
				}

				// Vacation[i] to Conference
				#pragma omp task default(shared) firstprivate(i,current_airport_of_interest)
				{
					Travels *vtc = &(vacation_to_conference[i]);
					find_path_task(current_airport_of_interest, parameters.to,
								parameters.dep_time_min, parameters.dep_time_max, &parameters,
								vtc, alliances);
				}

				// Conference to Vacation[i]
				#pragma omp task default(shared) firstprivate(i,current_airport_of_interest)
				{
					Travels *ctv = &(conference_to_vacation[i]);
					find_path_task(parameters.to, current_airport_of_interest, parameters.ar_time_min,
							parameters.ar_time_max, &parameters, ctv, alliances);
				}

				// Vacation[i] to Home
				#pragma omp task default(shared) firstprivate(i,current_airport_of_interest)
				{
					Travels *vth = &(vacation_to_home[i]);
					find_path_task(current_airport_of_interest, parameters.from,
							parameters.ar_time_max + parameters.vacation_time_min,
							parameters.ar_time_max + parameters.vacation_time_max, &parameters,
							vth, alliances);
				}
			}

			#pragma omp taskwait

			// Merge the computed paths.
			// Solve the "work hard" problem in one task, and each "play hard" problem in another
			// seperate task.
			// These merge/reduce tasks merge the partial routes computed in the parallel step before
			// and search for the cheapest of the merged routes.

			Travels *cth = &conference_to_home, *htc = &home_to_conference;
			Solution *sp = &solution;

			#pragma omp task default(shared)
			work_hard_task(htc, cth, sp, alliances);

			for (unsigned int i = 0; i < parameters.airports_of_interest.size(); i++)
			{
				string current_airport_of_interest = parameters.airports_of_interest[i];
				concurrent_hash_map<string, Location>::const_accessor a;

				// Small optimization: If no route from or to vacation location exist, do not
				// bother to find routes, since it would be impossible to find any, anyhow.
				if (!location_map->find(a, current_airport_of_interest)
						|| a->second.outgoing_flights.size() == 0
						|| a->second.incoming_flights.size() == 0)
				{
					Travel t;
					a.release();
					solution.add_play_hard(i, t);
				}
				else
				{
					a.release();

					#pragma omp task default(shared) firstprivate(i)
					{
						Travels *htv = &(home_to_vacation[i]), *vtc = &(vacation_to_conference[i]), *ctv =
								&(conference_to_vacation[i]), *vth = &(vacation_to_home[i]);
						play_hard_task(htv, vtc, cth, htc, vth, ctv, sp, i, alliances);
					}
				}
			}

			// Complete all mergereduce tasks. Each task is handed a pointer to the solution object.
			// Since each task knows exactly where to modify the solution object, special access synchronization
			// is not required (apart from some tiny spinlock implemented in the "Solution" class).
			// So ideally, the "solution" variable should be completely filled when all the tasks have run.
			#pragma omp taskwait

		}
	}

	return solution;
}

/// Compute the cost of a travel and uses the discounts when possible.
/** @param travel The travel.
 *  @param alliances The alliances. */
float compute_cost(Travel *travel, Alliances*alliances)
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

/// Computes a path from a point A to a point B.
/** The flights must be scheduled between t_min and t_max. It is also important
 *  to take the layover in consideration.
 *
 *  Optimization: This method now uses task-based parallelism in order to
 *  compute possible paths in parallel.
 *
 *  @param to            The destination.
 *  @param travels       The list of possible travels that we are building.
 *  @param t_min         You must not be in a plane before this value (epoch)
 *  @param t_max         You must not be in a plane after this value (epoch)
 *  @param parameters    The program parameters
 *  @param final_travels The output vector.
 *  @param min_range     The minimum price range in which all found routes must fit.
 *  @param alliances     The global alliance vector. */
void compute_path(string to, vector<Travel> *travels, unsigned long t_min,
		unsigned long t_max, Parameters parameters, vector<Travel> *final_travels,
		CostRange *min_range, Alliances *alliances)
{
	mutex final_travels_lock;

	unsigned int s = travels->size();
	if (s == 0) return;

	for (unsigned int i = 0; i < s; i++)
	{
		#pragma omp task default(shared) firstprivate(i)
		{
			compute_path_task(&(travels->at(i)), to,
							final_travels, &final_travels_lock, t_min, t_max, &parameters,
							alliances, min_range, location_map, 0);
		}
	}

	#pragma omp taskwait

	return;
}

/// Fills the travels's vector with flights that take off from the starting_point.
/** @param travels           A vector of travels under construction
 *  @param final_travels     Output vector for found routes to destination.
 *  @param starting_point    The starting point.
 *  @param travels           The list of possible travels that we are building.
 *  @param t_min             You must not be in a plane before this value (epoch).
 *  @param t_max             You must not be in a plane after this value (epoch).
 *  @param min_range         The minimum price range in which all found routes must fit.
 *  @param alliances         The global alliance vector.
 *  @param destination_point The travel destination point. Direct routes between start
 *                           and destination are not further processed. */
void fill_travel(Travels *travels, Travels *final_travels, string starting_point,
		unsigned long t_min, unsigned long t_max, CostRange *min_range,
		string destination_point, Alliances *alliances)
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

	#pragma omp parallel for shared(temp, travels)
	for (unsigned int i = 0; i < temp.size(); i++)
	{
		if ((&(temp.at(i)))->min_cost <= min_range->max)
		{
			travels->push_back(temp.at(i));
		}
	}
}

/// Convert a date to timestamp
/** @return a timestamp (epoch) corresponding to the given parameters. */
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

/// Convert a tm structure into a timestamp.
/** This function was completely rewritten in favor of a more performant and
 *  scalable implementation.
 *
 *  On Linux, mktime() apparently requires exclusive access to some system
 *  resource that is ensured by using futex locks. This makes it very hard to
 *  parallelize.
 *
 *  In order to minimize the amount of these expensive mktime() calls, we only
 *  compute "base dates" for each month/year combination which are subsequently
 *  stored into a concurrent hash map. The actual timestamps are then computed
 *  manually using the "base timestamp" plus the amount of seconds for the
 *  day/hour/minute.
 *
 *  EDIT: Apparently, for larger input files (> 200000 flights), mktime is still
 *  making trouble (by mixing up timezones on occasion). In order to avoid these
 *  problems altogether, we compute the timestamp COMPLETELY manually (yes, this is
 *  pain; luckily we can at least ignore the timezone problem -- it's all UTC,
 *  after all).
 *
 *  @return a timestamp (epoch) corresponding to the given parameter. */
time_t timegm(struct tm *tm)
{
	// Create a simple hash from the year and month.
	int year = tm->tm_year * 100 + tm->tm_mon;
	time_t month_ts;

	// Try to look up the month/year hash in the time map. Use the existing
	// base timestamp if it exists, otherwise compute it.
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

/// Parses the string s and returns a timestamp (epoch)
/** @param s A string that represents a date with the following format MMDDYYYYhhmmss with
 *      M = Month number
 *      D = Day number
 *      Y = Year number
 *      h = hour number
 *      m = minute number
 *      s = second number
 * @return a timestamp (epoch) corresponding to the given parameters.
 *
 * You shouldn't modify this part of the code unless you know what you are doing.
 * (Yep, I'm not really sure I know what I'm doing, but it works nonetheless).
 *
 * This function was completely rewritten in favour of a more performant approach.
 * Instead of working with substrings and numerous "atoi" calls, we perform the
 * "string -> integer" conversion manually.
 */
time_t convert_string_to_timestamp(char *s)
{
	int day, month, year, hour, minute, seconde;
	day = ((int) (s[2] - 48) * 10) + ((int) s[3] - 48);
	month = ((int) (s[0] - 48) * 10) + ((int) s[1] - 48);
	year = ((int) (s[4] - 48) * 1000) + ((int) (s[5] - 48) * 100)
			+ ((int) (s[6] - 48) * 10) + ((int) (s[7] - 48));
	hour = ((int) (s[8] - 48) * 10) + ((int) s[9] - 48);
	minute = ((int) (s[10] - 48) * 10) + ((int) s[11] - 48);
	seconde = ((int) (s[12] - 48) * 10) + ((int) s[13] - 48);
	return convert_to_timestamp(day, month, year, hour, minute, seconde);
}

/// You can use this function to display the parameters
/** @param parameters The parameter object. */
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

/// You can use this function to display a flight
/** @param flight   The flight.
 *  @param discount The discount.
 *  @param output   The output stream. */
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

/// This function is used to read the parameters
/** @param parameters Represents the structure that will be filled with the parameters.
 *  @param argc Count of command line parameters.
 *  @param argv Command line parameters. */
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

/// This function split a string into a vector of strings regarding the separator.
/** @param result The vector of separated strings
 *  @param line The line that must be split.
 *  @param separator The separator character. */
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

/// Parses a line containing a flight description.
/** This function parses a string containing a flight description.
 *
 *  It implements the following optimizations:
 *
 *    * (char*) instead of std::string saves unnecessary copying.
 *    * Flights that are clearly outside the specified time window are
 *      completely ignored, reducing the amount of data to be processed
 *      later.
 *    * A flight graph is constructed while parsing flights, allowing
 *      quick (and O(1)) access to outgoing and incoming flights from or
 *      to certain locations.
 *
 *  @param l       The line that must be parsed.
 *  @param param   A pointer to the input parameter object.*/
void parse_flight(char *l, Parameters *param)
{
	unsigned int p[7];

	int j = 0;
	for (int i = 0; l[i] != 0x00; i++)
	{
		if (l[i] == ';')
		{
			if (j >= 6) return;
			p[j] = i;
			l[i] = 0x00;

			if (j == 1 || j == 3) i += 13;
			j++;
		}
	}
	if (j < 6) return;

	const char* o = (const char*) l;

	Flight flight;
	flight.take_off_time = convert_string_to_timestamp(&(l[p[1] + 1]));
	flight.land_time = convert_string_to_timestamp(&(l[p[3] + 1]));

	// If the flight times are clearly outside of the specified time window, ignore
	// them completely. This saves quite a lot of useless computing time later.
	if (flight.land_time < param->dep_time_min - param->vacation_time_max) return;
	if (flight.take_off_time > param->ar_time_max + param->vacation_time_max) return;

	flight.id = string(&o[0]);
	flight.from = string(&(o[p[0] + 1]));
	flight.to = string(&(o[p[2] + 1]));
	flight.cost = atof(&(o[p[4] + 1]));
	flight.company = string(&(o[p[5] + 1]));
	flight.discout = 1.0;

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

/// This function parses the flights from a file.
/** @param filename   The name of the file containing the flights.
 *  @param parameters Input parameters. */
void parse_flights(string filename, Parameters *parameters)
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
	int s = lfs.size();

	#pragma omp parallel for
	for (int i = 1; i < s; i ++)
	{
		// Determine the length of the line by computing the difference between the
		// current LF character and the previous (does always work, since the first element
		// in the "lfs" vector is a "-1").
		// Then, allocate an appropriate amount of memory (plus 1 byte for the trailing 0-byte).
		char* b = (char*) malloc(lfs[i] - lfs[i - 1] + 1);

		// Copy the line into the previously allocated line buffer.
		strncpy(b, (m + lfs[i - 1] + 1), lfs[i] - lfs[i - 1] - 1);

		// Add 0-byte to mark string end.
		b[lfs[i] - lfs[i - 1] - 1] = 0x00;

		// Parse line.
		parse_flight(b, parameters);
	}

	// Unmap file from memory and close file handle.
	munmap(m, stat.st_size);
	close(fd);
}

/// This function parses a line containing alliances between companies.
/** @param alliance A vector of companies sharing a same alliance.
 *  @param line     A line that contains the name of companies in the same alliance. */
void parse_alliance(vector<string> &alliance, string line)
{
	vector<string> splittedLine;
	split_string(splittedLine, line, ';');
	for (unsigned int i = 0; i < splittedLine.size(); i++)
	{
		alliance.push_back(splittedLine[i]);
	}
}

/// This function parses a line containing alliances between companies.
/** @param alliances A 2D vector representing the alliances. Companies on the
 *                   same line are in the same alliance.
 *  @param filename  The name of the file containing the alliances description. */
void parse_alliances(Alliances *alliances, string filename)
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
		alliances->push_back(alliance);
	}
}

/// Check if 2 companies are in the same alliance.
/** Based on the assumption that there are relatively few possible combinations
 *  of airlines, this function used a cache based on a "tbb::concurrent_hash_map<string,bool>"
 *  in which each combination of airlines is stored. Each entry is created with the
 *  first call with a certain company combination.
 *
 *  @param c1        The first company's name.
 *  @param c2        The second company's name.
 *  @param alliances A 2D vector representing the alliances. Companies on the
 *                   same line are in the same alliance. */
bool company_are_in_a_common_alliance(const string& c1, const string& c2,
		Alliances *alliances)
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

/// The 2 last flights are with the same company.
/** @param flight_before The first flight.
 *  @param current_flight The second flight.
 *  @return The 2 flights are with the same company. */
bool has_just_traveled_with_company(Flight *flight_before, Flight *current_flight)
{
	return flight_before->company == current_flight->company;
}

/// The 2 last flights are with the same alliance.
/** @param flight_before The first flight.
 *  @param current_flight The second flight.
 *  @param alliances The alliances.
 *  @return The 2 flights are with the same alliance.  */
bool has_just_traveled_with_alliance(Flight *flight_before, Flight *current_flight,
		Alliances *alliances)
{
	return company_are_in_a_common_alliance(current_flight->company,
			flight_before->company, alliances);
}

/// Display the alliances on the standard output.
/** @param alliances The alliances. */
void print_alliances(Alliances &alliances)
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

/// Display the flights on the standard output.
/** @param flights The flights.
 *  @param discounts Discounts for each flight.
 *  @param output The output stream to be used. */
void print_flights(vector<Flight>& flights, vector<float> discounts, ofstream& output)
{
	for (unsigned int i = 0; i < flights.size(); i++)
	{
		print_flight(flights[i], discounts[i], output);
	}
}

/// Indicates if the city has already been visited in the travel.
/** This function is used to avoid stupid loops.
 *
 *  @param travel The travels.
 *  @param city The city.
 *  @return The current travel has never visited the given city. */
bool nerver_traveled_to(Travel travel, string city)
{
	for (unsigned int i = 0; i < travel.flights.size(); i++)
	{
		if (travel.flights[i].from == city || travel.flights[i].to == city)
		{
			return false;
		}
	}
	return true;
}

/// Display a travel on an arbitrary output stream.
/** @param travel The travel.
 *  @param alliances The alliances (used to compute the price).
 *  @param output The output stream. */
void print_travel(Travel& travel, Alliances *alliances, ofstream& output)
{
	// Sometimes prices seem to differ one cents (possibly due to some float
	// arithmetics issues), so we compute the actual travel cost again for
	// output.

	output << "Price : " << compute_cost(&travel, alliances) << endl;
	print_flights(travel.flights, travel.discounts, output);
	output << endl;
}

/// Outputs solutions of both "work hard" and "play hard" problems.
/** This method first solves both the "work hard" and all "play hard" problems
 *  and then writes the solutions into the associated output files.
 *
 *  @param parameters The parameters.
 *  @param alliances The alliances between companies. */
void output_solutions(Parameters& parameters, Alliances *alliances)
{
	// Solve everything.
	Solution solution = play_and_work_hard(parameters, alliances);

	ofstream ph_out, wh_out;

	ph_out.open(parameters.play_hard_file.c_str());
	wh_out.open(parameters.work_hard_file.c_str());

	vector<string> cities = parameters.airports_of_interest;
	for (unsigned int i = 0; i < cities.size(); i++)
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

/// Dumps the flight graph.
/** This function dumps the entire flight graph, grouped by cities. */
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
	// Declare variables and read the args
	Parameters parameters;
	Alliances *alliances = new Alliances();
	read_parameters(parameters, argc, argv);

	// Respect nb_threads parameter.
#ifdef OMA_OPENMP
	omp_set_num_threads(parameters.nb_threads);
#endif

	// Initialize flight graph (important: needs to be allocated on heap, otherwise
	// everything will blow up on larger input datasets).
	location_map = new concurrent_hash_map<string, Location>;

	// Read flights and alliances.
	parse_flights(parameters.flights_file, &parameters);
	parse_alliances(alliances, parameters.alliances_file);

	tick_count t0 = tick_count::now();
	output_solutions(parameters, alliances);
	tick_count t1 = tick_count::now();

	cout << "Duration: " << (t1 - t0).seconds() * 1000 << endl;
}

//./run -from Paris -to Los\ Angeles -departure_time_min 11152012000000 -departure_time_max 11172012000000 -arrival_time_min 11222012000000 -arrival_time_max 11252012000000 -max_layover 100000 -vacation_time_min 432000 -vacation_time_max 604800 -vacation_airports Rio London Chicago -flights flights.txt -alliances alliances.txt
