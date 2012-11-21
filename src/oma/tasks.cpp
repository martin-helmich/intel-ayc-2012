/*
 * tasks.cpp
 *
 *  Created on: 21.11.2012
 *      Author: mhelmich
 */

#include <iostream>
#include "tasks.h"

using namespace std;

oma::FindPathTask::FindPathTask(string f, string t, int tmi, int tma,
		Parameters *p, vector<Flight> *fl, vector<Travel> *tr)
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
	cout << "Compute path from " << from << " to " << to << endl;

	vector<Travel> temporary_travels;

	fill_travel(&temporary_travels, *flights, from, t_min, t_max);
	compute_path(*flights, to, &temporary_travels, t_min, t_max, *parameters, travels);

	cout << "Compute path from " << from << " to " << to << " DONE" << endl;

	return NULL;
}
