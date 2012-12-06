/*
 * Method bodies for custom tasks.
 *
 * (C) 2012 Martin Helmich <martin.helmich@hs-osnabrueck.de>
 *          Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>
 *
 *          University of Applied Sciences Osnabrück
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
		vector<Flight> *fl, vector<Travel> *tr, Alliances *a)
{
	from = f;
	to = t;
	parameters = p;

	flights = fl;
	travels = tr;
	alliances = a;

	t_min = tmi;
	t_max = tma;
}

tbb::task* oma::FindPathTask::execute()
{
	Travels temp_travels, all_paths;
	CostRange min_range;

	OUT("STRT: " << from << " -> " << to);

	fill_travel(&temp_travels, &all_paths, *flights, from, t_min, t_max, &min_range, to,
			alliances);

	OUT("INIT: " << from << " -> " << to << " : " << temp_travels.size() << "/" << all_paths.size() << ", "
			<< min_range.min << "-" << min_range.max);

	compute_path(*flights, to, &temp_travels, t_min, t_max, *parameters, &all_paths,
			&min_range, alliances);

	OUT("DONE: " << from << " -> " << to << " : " << all_paths.size() << ", "
			<< min_range.min << "-" << min_range.max);

	unsigned int s = all_paths.size();
	for (unsigned int i = 0; i < s; i++)
	{
		if (all_paths[i].min_cost <= min_range.max)
		{
			travels->push_back(all_paths[i]);
		}
	}

	OUT("REDC: " << from << " -> " << to << " : " << travels->size());

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

	vector<Travel> work_hard;
	merge_path(&work_hard, home_to_conference, conference_to_home, alliances);

	solution->work_hard = work_hard[0];

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
		results_lock->lock();
		results->push_back(*pmtol.get_cheapest());
		results_lock->unlock();
	}

	return NULL;
}
