/*!
 * @file tasks.h
 * @brief This file contains header definitions for custom tasks.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#ifndef TASKS_H_
#define TASKS_H_

#include <vector>
#include "tbb/task.h"
#include "tbb/mutex.h"

#include "../types.h"
#include "../methods.h"

using namespace tbb;

namespace oma
{

/// Computes all possible paths between two locations.
/** This task computes all possible paths between two locations. This is done
 *  by performing a breadth-first-search on the flight graph. Routes to the
 *  destination point are only accepted in the candidate list if they are
 *  potentially cheaper than the cheapest known solution (there is a small window
 *  if uncertainty because the flight discounts are not completely known in
 *  advance, so instead of fixed costs we use "cost ranges" consisting of the
 *  minimum and maximum possible costs of a route). */
class FindPathTask: public tbb::task
{
private:
	string from, to;
	Parameters *parameters;
	Travels *travels;
	int t_min, t_max;
	Alliances *alliances;

public:

	/// Creates a new task.
	/** @param f Starting point.
	 *  @param t Destination point.
	 *  @param tmi Minimum departure time.
	 *  @param tma Maximum departure time.
	 *  @param p Input parameters.
	 *  @param tr Output travel vector.
	 *  @param a All alliances. */
	FindPathTask(string f, string t, int tmi, int tma, Parameters *p, Travels *tr,
			Alliances *a);

	/// Executes the "find path" task.
	task* execute();
};

/// Solves the "work hard" problem.
/** This task accepts sets of possible paths from "home to conference" and vice
 *  versa as input parameters and then
 *
 *    1. merges these paths to possible solutions for the "work hard" problem, AND
 *    2. finds the cheapest of these solutions. */
class WorkHardTask: public tbb::task
{
private:
	Travels *home_to_conference, *conference_to_home;
	Solution *solution;
	Alliances *alliances;

public:

	/// Creates a new "work hard" task.
	/** @param htc All possible "home to conference" routes.
	 *  @param cth All possible "conference to home" routes.
	 *  @param s   A pointer to the solution object. When done, the best route is written
	 *             to "s->work_hard".
	 *  @param a   A pointer to the alliances vector. */
	WorkHardTask(Travels *htc, Travels *cth, Solution *s, Alliances *a);

	/// Executes the "work hard" task.
	/** Merges the two sets of input routes (by building the carthesian product and selecting
	 *  all items where arrival time of the last flight of set A is less then departure time
	 *  of the first flight of set B) and finds the cheapest of the possible routes. */
	task* execute();
};

/// Solves ONE specific of the "play hard" problems.
/** This task accepts sets of possible paths from "home to conference", "home to vacation",
 *  "vacation to conference" and each vice versa as input parameters and then
 *
 *    1. merges these paths to possible solutions for ONE of the "play hard" problems, AND
 *    2. finds the cheapest of these solutions. */
class PlayHardTask: public tbb::task
{
private:
	Travels *home_to_vacation, *vacation_to_conference, *conference_to_home,
			*home_to_conference, *vacation_to_home, *conference_to_vacation;
	Solution *solution;
	unsigned int solution_index;
	Alliances *alliances;

public:

	/// Creates a new "play hard" task.
	/** @param htv All possible "home to vacation" routes.
	 *  @param vtc All possible "vacation to conference" routes.
	 *  @param cth All possible "conference to home" routes.
	 *  @param htc All possible "home to conference" routes.
	 *  @param vth All possible "vacation to home" routes.
	 *  @param ctv All possible "conference to vacation" routes.
	 *  @param s   A pointer to the solution object. When done, the best route is written
	 *             to "s->play_hard[i]".
	 *  @param i   Index at which this solution should be inserted into the solution vector.
	 *             The order of insertion is quite important, otherwise a simple ->push_back
	 *             would have sufficed.
	 *  @param a   A pointer to the alliances vector. */
	PlayHardTask(Travels *htv, Travels *vtc, Travels *cth, Travels *htc, Travels *vth,
			Travels *ctv, Solution *s, unsigned int i, Alliances *a);

	/// Executes the "play hard" task.
	/** Merges the three sets of input routes (by building the carthesian product and selecting
	 *  all items where arrival time of the last flight of set A is less then departure time
	 *  of the first flight of set B) and finds the cheapest of the possible routes. */
	task* execute();
};

/// Task for creating a specific subset of the "play hard" solution.
/** This task creates a specific subset of the "play hard" solution (i.e. either
 *  "home -> vacation -> conference -> home" or "home -> conference -> vacation-> home"). */
class PlayHardMergeTripleTask: public tbb::task
{
private:
	Travels *travels1, *travels2, *travels3;
	Travels *results;
	mutex *results_lock;
	Alliances *alliances;

public:

	/// Creates a new "play hard merge" task.
	/** @param r Output result vector.
	 *  @param rl Mutex for synchronizing access on output result vector.
	 *  @param t1 Input travel vector #1.
	 *  @param t2 Input travel vector #2.
	 *  @param t3 Input travel vector #3.
	 *  @param a Alliances. */
	PlayHardMergeTripleTask(Travels *r, tbb::mutex *rl, Travels *t1, Travels *t2,
			Travels *t3, Alliances *a);

	/// Executes the "play hard merge" task.
	task* execute();
};

}

#endif /* TASKS_H_ */
