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
#include "tbb/concurrent_hash_map.h"

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
 *  minimum and maximum possible costs of a route).
 *
 *  @param f Starting point.
 *  @param t Destination point.
 *  @param tmi Minimum departure time.
 *  @param tma Maximum departure time.
 *  @param p Input parameters.
 *  @param tr Output travel vector.
 *  @param a All alliances. */
void find_path_task(string f, string t, int tmi, int tma, Parameters *p, Travels *tr, Alliances *a);

/// Solves the "work hard" problem.
/** This task accepts sets of possible paths from "home to conference" and vice
 *  versa as input parameters and then merges the two sets of input routes (by
 *  building the carthesian product and selecting all items where arrival time
 *  of the last flight of set A is less then departure time of the first flight of
 *  set B) and finds the cheapest of the possible routes.
 *
 *  @param htc All possible "home to conference" routes.
 *  @param cth All possible "conference to home" routes.
 *  @param s   A pointer to the solution object. When done, the best route is written
 *             to "s->work_hard".
 *  @param a   A pointer to the alliances vector. */
void work_hard_task(Travels *htc, Travels *cth, Solution *s, Alliances *a);

/// Solves ONE specific of the "play hard" problems.
/** This task accepts sets of possible paths from "home to conference", "home
 *  to vacation", "vacation to conference" and each vice versa as input
 *  parameters and then merges the three sets of input routes (by building the
 *  carthesian product and selecting all items where arrival time of the last
 *  flight of set A is less then departure time of the first flight of set B)
 *  and finds the cheapest of the possible routes.
 *
 *  @param htv All possible "home to vacation" routes.
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
void play_hard_task(Travels *htv, Travels *vtc, Travels *cth, Travels *htc, Travels *vth,
		Travels *ctv, Solution *s, unsigned int i, Alliances *a);

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

/// Task for computing a path between two different locations.
/** This task recursively computes possible paths between two different
 *  locations. It accepts a single travel as input parameter and creates new
 *  tasks, each exploring a different follow-up travel of the input travel.
 *
 *  In comparison to the reference implementation, this algorithms performs a
 *  breadth-first-search (instead of a depth-first-search). This is more
 *  efficient, since the optimal route is probably rather short and should
 *  be found quicker using BFS.
 *
 *  @param t   Input travel.
 *  @param dst Name of destination.
 *  @param ft  Output vector.
 *  @param ftl Output vector mutex.
 *  @param tmi Minimum departure time.
 *  @param tma Maximum departure time.
 *  @param p   Program parameters.
 *  @param a   Alliance list.
 *  @param mr  Minimum cost range.
 *  @param lm  Location map.
 *  @param r   Recursion level. */
void compute_path_task(Travel *t, std::string &dst, Travels *ft, tbb::mutex *ftl,
			unsigned long &tmi, unsigned long &tma, Parameters *p, Alliances *a,
			CostRange *mr, tbb::concurrent_hash_map<string, Location> *lm, unsigned int l = 0);

Travel* merge_path_triple(Travels *r, tbb::mutex *rl, Travels *t1,
		Travels *t2, Travels *t3, Alliances *a);

}

#endif /* TASKS_H_ */
