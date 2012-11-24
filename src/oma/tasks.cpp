/*
 * Method bodies for custom tasks.
 *
 * (C) 2012 Martin Helmich <martin.helmich@hs-osnabrueck.de>
 *          Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>
 *
 *          University of Applied Sciences Osnabrück
 */

#include <iostream>
#include "tasks.h"
#include "../methods.h"

using namespace std;

oma::FindPathTask::FindPathTask(string f, string t, int tmi, int tma, Parameters *p,
		vector<Flight> *fl, vector<Travel> *tr)
{
	from = f;
	to = t;
	parameters = p;

	flights = fl;
	travels = tr;

	t_min = tmi;
	t_max = tma;
}

tbb::task* oma::FindPathTask::execute()
{
	vector<Travel> temp_travels;

	fill_travel(&temp_travels, *flights, from, t_min, t_max);
	compute_path(*flights, to, &temp_travels, t_min, t_max, *parameters, travels);

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
	vector<Travel> work_hard = *home_to_conference;
	merge_path(work_hard, *conference_to_home);

	Travel cheapest_work_hard = find_cheapest(work_hard, *alliances);
	solution->work_hard = cheapest_work_hard;

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
	vector<Travel> all_travels;

	merge_path(*home_to_vacation, *vacation_to_conference);
	merge_path(*home_to_vacation, *conference_to_home);
	all_travels = *home_to_vacation;

	vector<Travel> temp = *home_to_conference;

	merge_path(temp, *conference_to_vacation);
	merge_path(temp, *vacation_to_home);
	all_travels.insert(all_travels.end(), temp.begin(), temp.end());

	Travel cheapest_travel = find_cheapest(all_travels, *alliances);
	solution->add_play_hard(solution_index, cheapest_travel);

	return NULL;
}
