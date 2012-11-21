/*
 * tasks.h
 *
 *  Created on: 21.11.2012
 *      Author: mhelmich
 */

#ifndef TASKS_H_
#define TASKS_H_

#include <vector>
#include "tbb/task.h"

#include "../types.h"
#include "../methods.h"

namespace oma
{

/**
 * Task for computing all possible paths between two locations.
 */
class FindPathTask: public tbb::task
{
private:
	string from, to;
	Parameters *parameters;
	vector<Flight> *flights;
	vector<Travel> *travels;
	int t_min, t_max;

public:
	FindPathTask(string f, string t, int tmi, int tma, Parameters *p,
			vector<Flight> *fl, vector<Travel> *tr);
	task* execute();
};

}

#endif /* TASKS_H_ */
