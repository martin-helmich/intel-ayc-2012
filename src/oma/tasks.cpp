/*!
 * @file tasks.cpp
 * @brief This file contains method bodies for custom tasks.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#include <iostream>
#include <limits>

#include "tbb/parallel_reduce.h"

#include "tasks.h"
#include "loop_bodies.h"
#include "../methods.h"

using namespace std;
using namespace oma;

oma::FindPathTask::FindPathTask(string f, string t, int tmi, int tma, Parameters *p,
		vector<Travel> *tr, Alliances *a)
{
	from = f;
	to = t;
	parameters = p;

	travels = tr;
	alliances = a;

	t_min = tmi;
	t_max = tma;
}

tbb::task* oma::FindPathTask::execute()
{
	Travels temp_travels, all_paths;
	CostRange min_range;

	fill_travel(&temp_travels, &all_paths, from, t_min, t_max, &min_range, to, alliances);

	compute_path(to, &temp_travels, t_min, t_max, *parameters, &all_paths, &min_range,
			alliances);

	FilterPathsLoop fpl(&all_paths, travels, &min_range);

	if (all_paths.size() > 500) parallel_reduce(
			blocked_range<unsigned int>(0, all_paths.size()), fpl);
	else fpl(blocked_range<unsigned int>(0, all_paths.size()));

	return NULL;
}

oma::WorkHardTask::WorkHardTask(Travels *htc, Travels *cth, Solution *s, Alliances *a)
{
	home_to_conference = htc;
	conference_to_home = cth;
	solution = s;
	alliances = a;
}

tbb::task* oma::WorkHardTask::execute()
{
	// Return empty travel when one of the two partial routes is empty.
	if (home_to_conference->size() == 0 || conference_to_home->size() == 0)
	{
		Travel empty;
		solution->work_hard = empty;

		return NULL;
	}

	PathMergingOuterLoop pmol(home_to_conference, conference_to_home, alliances);
	parallel_reduce(blocked_range<unsigned int>(0, home_to_conference->size()), pmol);

	if (pmol.get_cheapest() != NULL)
	{
		solution->work_hard = *pmol.get_cheapest();
	}

	return NULL;
}

oma::PlayHardTask::PlayHardTask(Travels *htv, Travels *vtc, Travels *cth, Travels *htc,
		Travels *vth, Travels *ctv, Solution *s, unsigned int si, Alliances *a)
{
	home_to_conference = htc;
	home_to_vacation = htv;
	vacation_to_conference = vtc;
	vacation_to_home = vth;
	conference_to_home = cth;
	conference_to_vacation = ctv;
	alliances = a;
	solution = s;
	solution_index = si;
}

tbb::task* oma::PlayHardTask::execute()
{
	vector<Travel> all_travels, home_to_vacation_to_conference;
	mutex rlock;
	task_list merge_paths;

	merge_paths.push_back(
			*new (task::allocate_child()) PlayHardMergeTripleTask(&all_travels, &rlock,
					home_to_vacation, vacation_to_conference, conference_to_home,
					alliances));
	merge_paths.push_back(
			*new (task::allocate_child()) PlayHardMergeTripleTask(&all_travels, &rlock,
					home_to_conference, conference_to_vacation, vacation_to_home,
					alliances));

	set_ref_count(3);
	task::spawn_and_wait_for_all(merge_paths);

	if (all_travels.size() == 0)
	{
		Travel empty;
		solution->add_play_hard(solution_index, empty);
	}
	else if (all_travels.size() == 1)
	{
		solution->add_play_hard(solution_index, all_travels[0]);
	}
	else
	{
		if (all_travels[0].max_cost < all_travels[1].max_cost) solution->add_play_hard(
				solution_index, all_travels[0]);
		else solution->add_play_hard(solution_index, all_travels[1]);
	}

	return NULL;
}

oma::PlayHardMergeTripleTask::PlayHardMergeTripleTask(Travels *r, tbb::mutex *rl,
		Travels *t1, Travels *t2, Travels *t3, Alliances *a)
{
	results = r;
	results_lock = rl;
	travels1 = t1;
	travels2 = t2;
	travels3 = t3;
	alliances = a;
}

tbb::task* oma::PlayHardMergeTripleTask::execute()
{
	PathMergingTripleOuterLoop pmtol(travels1, travels2, travels3, alliances);
	parallel_reduce(blocked_range<unsigned int>(0, travels1->size()), pmtol);

	if (pmtol.get_cheapest() != NULL)
	{
		mutex::scoped_lock l(*results_lock);
		results->push_back(*pmtol.get_cheapest());
	}

	return NULL;
}

ComputePathTask::ComputePathTask(Travel *t, string dst, Travels *ft, mutex *ftl,
		unsigned long tmi, unsigned long tma, Parameters *p, Alliances *a, CostRange *mr,
		concurrent_hash_map<string, Location> *lm, unsigned int l)
{
	travel = t;
	destination = dst;
	final_travels = ft;
	final_travels_lock = ftl;
	t_min = tmi;
	t_max = tma;
	parameters = p;
	alliances = a;
	min_range = mr;
	location_map = lm;
	level = l;
}

/** This method spawns more compute path tasks. If a path to the destination
 *  is found, it is placed into the "final_travels" vector. */
task* ComputePathTask::execute()
{
	Flight *current_city = &(travel->flights.back());

	concurrent_hash_map<string, Location>::const_accessor a;
	if (!location_map->find(a, current_city->to))
	{
		cerr << "Fehler: Stadt " << current_city->to << " ist nicht bekannt." << endl;
		return NULL;
	}

	const Location *from = &(a->second);

	tbb::task_list tl;
	unsigned int tl_count = 0;

	unsigned int s = from->outgoing_flights.size();
	for (unsigned int i = 0; i < s; i++)
	{
		Flight *flight = (Flight*) &(from->outgoing_flights[i]);
		if (flight->take_off_time >= t_min && flight->land_time <= t_max
				&& (flight->take_off_time > current_city->land_time)
				&& flight->take_off_time - current_city->land_time
						<= parameters->max_layover_time
				&& nerver_traveled_to(*travel, flight->to)
				&& flight->cost * 0.7 + travel->min_cost <= min_range->max)
		{

			Travel *new_travel = new Travel(*travel);
			new_travel->add_flight(*flight, alliances);

			if (flight->to == destination)
			{
				mutex::scoped_lock lock(*final_travels_lock);

				final_travels->push_back(*new_travel);
				min_range->from_travel(new_travel);
			}
			else
			{
				tl.push_back(
						*new (tbb::task::allocate_child()) ComputePathTask(new_travel,
								destination, final_travels, final_travels_lock, t_min,
								t_max, parameters, alliances, min_range, location_map,
								level + 1));
				tl_count++;
			}
		}
	}

	if (level > 0)
	{
		delete travel;
	}

	if (tl_count > 0)
	{
		set_ref_count(tl_count + 1);
		tbb::task::spawn_and_wait_for_all(tl);
	}

	return NULL;
}
