/*
 * Header definitions for custom tasks.
 *
 * (C) 2012 Martin Helmich <martin.helmich@hs-osnabrueck.de>
 *          Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>
 *
 *          University of Applied Sciences Osnabrück
 */

#ifndef TASKS_H_
#define TASKS_H_

#include <vector>
#include "tbb/task.h"

#include "../types.h"
#include "../methods.h"

// Yes, we are lazy and don't want to type "vector<vector<string> >" too often... ;)
typedef vector<Travel> Travels;
typedef vector<vector<string> > Alliances;


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
	Travels *travels;
	int t_min, t_max;

public:
	/**
	 * Creates a new task.
	 *
	 * @param f Starting point.
	 * @param t Destination point.
	 * @param tmi Minimum
	 */
	FindPathTask(string f, string t, int tmi, int tma, Parameters *p, vector<Flight> *fl,
			Travels *tr);
	task* execute();
};

/**
 * Solves the "work hard" problem.
 *
 * This task accepts sets of possible paths from "home to conference" and vice
 * versa as input parameters and then
 *
 *   1. merges these paths to possible solutions for the "work hard" problem, AND
 *   2. finds the cheapest of these solutions.
 *
 */
class WorkHardTask: public tbb::task
{
private:
	Travels *home_to_conference, *conference_to_home;
	Solution *solution;
	Alliances *alliances;

public:

	/**
	 * Creates a new "work hard" task.
	 *
	 * @param htc All possible "home to conference" routes.
	 * @param cth All possible "conference to home" routes.
	 * @param s   A pointer to the solution object. When done, the best route is written
	 *            to "s->work_hard".
	 * @param a   A pointer to the alliances vector.
	 */
	WorkHardTask(Travels *htc, Travels *cth, Solution *s, Alliances *a);

	/**
	 * Executes the task.
	 *
	 * Merges the two sets of input routes (by building the carthesian product and selecting
	 * all items where arrival time of the last flight of set A is less then departure time
	 * of the first flight of set B) and finds the cheapest of the possible routes.
	 */
	task* execute();
};

/**
 * Solves ONE specific of the "play hard" problems.
 *
 * This task accepts sets of possible paths from "home to conference", "home to vacation",
 * "vacation to conference" and each vice versa as input parameters and then
 *
 *   1. merges these paths to possible solutions for ONE of the "play hard" problems, AND
 *   2. finds the cheapest of these solutions.
 *
 */
class PlayHardTask: public tbb::task
{
private:
	Travels *home_to_vacation, *vacation_to_conference, *conference_to_home,
			*home_to_conference, *vacation_to_home, *conference_to_vacation;
	Solution *solution;
	unsigned int solution_index;
	Alliances *alliances;

public:

	/**
	 * Creates a new "play hard" task.
	 *
	 * @param htv All possible "home to vacation" routes.
	 * @param vtc All possible "vacation to conference" routes.
	 * @param cth All possible "conference to home" routes.
	 * @param htc All possible "home to conference" routes.
	 * @param vth All possible "vacation to home" routes.
	 * @param ctv All possible "conference to vacation" routes.
	 * @param s   A pointer to the solution object. When done, the best route is written
	 *            to "s->play_hard[i]".
	 * @param i   Index at which this solution should be inserted into the solution vector.
	 *            The order of insertion is quite important, otherwise a simple ->push_back
	 *            would have sufficed.
	 * @param a   A pointer to the alliances vector.
	 */
	PlayHardTask(Travels *htv, Travels *vtc, Travels *cth, Travels *htc, Travels *vth,
			Travels *ctv, Solution *s, unsigned int i, Alliances *a);

	/**
	 * Executes the task.
	 *
	 * Merges the two sets of input routes (by building the carthesian product and selecting
	 * all items where arrival time of the last flight of set A is less then departure time
	 * of the first flight of set B) and finds the cheapest of the possible routes.
	 */
	task* execute();
};


}


#endif /* TASKS_H_ */
