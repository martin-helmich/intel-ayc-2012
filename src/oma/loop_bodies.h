/*!
 * @file loop_bodies.h
 * @brief This file contains declarations for various parallel loop bodies.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#ifndef LOOPBODIES_H_
#define LOOPBODIES_H_

#include <vector>
#include "tbb/blocked_range.h"
#include "tbb/mutex.h"
#include "tbb/parallel_do.h"
#include "tbb/concurrent_hash_map.h"
#include "../types.h"

using namespace std;
using namespace tbb;

namespace oma
{

/**
 * Loop body for parsing flights from an input file.
 *
 * This loop body expects that the entire input file is mapped into memory
 * (for example via "mmap") and that the positions of all line endings in this
 * file are known. Based on the known positions of line endings, the individual
 * lines can then be parsed in parallel.
 */
class ParseFlightsLoop
{
private:
	vector<Flight> *flights;
	char *input;
	vector<int>* lfs;

public:
	/**
	 * @brief Initial constructor.
	 * @param i Pointer to mapped input file.
	 * @param l Pointer to vector containing positions of line endings (LFs).
	 * @param f Pointer to flight output vector.
	 */
	ParseFlightsLoop(char* i, vector<int>* l, vector<Flight> *f);

	/**
	 * @brief Split constructor.
	 * @param pfl Parent loop.
	 * @param split Magical parameter (ask Intel what it does).
	 */
	ParseFlightsLoop(ParseFlightsLoop &pfl, split);

	/**
	 * @brief Actual loop body.
	 * @param range Range to be iterated over.
	 */
	void operator()(const blocked_range<int> range);

	/**
	 * @brief Joins two loop results into one.
	 * @param pfl Another loop body.
	 */
	void join(ParseFlightsLoop &pfl);
};

class PathComputingInnerLoop
{
private:
	vector<Travel> *travels, *final_travels;
	vector<Flight> *flights;
	mutex *travels_lock, *final_travels_lock;
	unsigned long t_min;
	unsigned long t_max;
	Parameters parameters;
	Flight *current_city;
	Travel *travel;
	string to;

public:
	PathComputingInnerLoop(vector<Travel> *t, vector<Travel> *ft, vector<Flight> *f,
			mutex *tl, mutex *ftl, unsigned long tmi, unsigned long tma, Parameters &p,
			Flight *c, Travel *ct, string to)
	{
		travels = t;
		final_travels = ft;
		flights = f;
		travels_lock = tl;
		final_travels_lock = ftl;

		t_min = tmi;
		t_max = tma;
		parameters = p;
		current_city = c;
		travel = ct;
		this->to = to;
	}

	void operator()(blocked_range<unsigned int> &range) const;
};

/**
 * Loop body for merging two seperate sets of routes into one set of routes.
 *
 * This loop body merges two vectors of routes (i.e. vector<Travel>) into
 * one vector of routes. This is done by building all possible combinations of
 * travels in vector a and all travels in vector b (assuming the landing time
 * of the last flight in a and the takeoff time of the first flight in b match).
 *
 * This loop also determines the cheapest route on-the-fly.
 */
class PathMergingOuterLoop
{
protected:
	Travels *travels1, *travels2;
	Alliances *alliances;
	Travel *cheapest;

public:
	CostRange min_range;

	/**
	 * @brief Initial constructor.
	 * @param t1 First travel vector.
	 * @param t2 Second travel vector.
	 * @param a Alliance list.
	 */
	PathMergingOuterLoop(Travels *t1, Travels *t2, Alliances *a);

	/**
	 * @brief Split constructor.
	 * @param pmol Parent loop.
	 * @param split Magical parameter (ask Intel what it does).
	 */
	PathMergingOuterLoop(PathMergingOuterLoop &pmol, split);

	/**
	 * @brief Actual loop body.
	 * @param range Range to be iterated over.
	 */
	void operator()(blocked_range<unsigned int> &range);

	/**
	 * @brief Joins two loop results into one.
	 * @param pfl Another loop body.
	 */
	void join(PathMergingOuterLoop& pmol);

	/**
	 * @brief Gets the cheapest combination of input travels.
	 */
	Travel* get_cheapest();
};

/**
 * Loop body for merging THREE seperate sets of routes into one set of routes.
 *
 * This loop body merges THREE vectors of routes (i.e. vector<Travel>) into
 * one vector of routes. This is done by building all possible combinations of
 * travels in vector a, b and c (assuming the landing time
 * of the last flight in {a,b} and the takeoff time of the first flight in {b,c} match).
 *
 * This loop also determines the cheapest route on-the-fly.
 */
class PathMergingTripleOuterLoop
{
protected:
	Travels *travels1, *travels2, *travels3;
	Alliances *alliances;
	Travel *cheapest;

public:
	CostRange min_range;

	/**
	 * @brief Initial constructor.
	 * @param t1 First travel vector.
	 * @param t2 Second travel vector.
	 * @param t3 Third travel vector.
	 * @param a Alliance list.
	 */
	PathMergingTripleOuterLoop(Travels *t1, Travels *t2, Travels *t3, Alliances *a);

	/**
	 * @brief Split constructor.
	 * @param pmol Parent loop.
	 * @param split Magical parameter (ask Intel what it does).
	 */
	PathMergingTripleOuterLoop(PathMergingTripleOuterLoop &pmol, split);

	/**
	 * @brief Actual loop body.
	 * @param range Range to be iterated over.
	 */
	void operator()(blocked_range<unsigned int> &range);

	/**
	 * @brief Joins two loop results into one.
	 * @param pfl Another loop body.
	 */
	void join(PathMergingTripleOuterLoop& pmol);

	/**
	 * @brief Gets the cheapest combination of input travels.
	 */
	Travel* get_cheapest();
};

/**
 * Loop body for computing all possible paths between two locations.
 *
 * This loop body computes all possible paths between two locations. It is
 * intended to be used with a parallel_do call.
 *
 * In comparison to the reference implementation, this algorithms performs a
 * breadth-first-search (instead of a depth-first-search). This is more
 * efficient, since the optimal route is probably rather short and should
 * be found quicker using BFS.
 */
class ComputePathOuterLoop
{
private:
	vector<Travel> *final_travels;
	mutex *final_travels_lock;
	Parameters parameters;
	string to;
	unsigned long t_min, t_max;
	CostRange *min_range;
	Alliances *alliances;
	concurrent_hash_map<string, Location> *location_map;

public:
	/**
	 * @brief Constructor
	 * @param ft Vector containing found routes to destination.
	 * @param ftl Mutex for synchronizing access on "ft".
	 * @param p Input parameters.
	 * @param t Destination
	 * @param tmi Minimum departure time.
	 * @param tma Maximum departure time.
	 * @param r Global cost-range object.
	 * @param a Alliance vector.
	 * @param lm Global location map.
	 */
	ComputePathOuterLoop(vector<Travel> *ft, mutex *ftl, Parameters &p, string t,
			unsigned long tmi, unsigned long tma, CostRange *r, Alliances *a,
			concurrent_hash_map<string, Location> *lm);

	/**
	 * @brief Actual loop body.
	 * @param range Range to be iterated over.
	 * @param f Queue in order to add more items to be processed into the parallel_do queue.
	 */
	void operator()(Travel travel, parallel_do_feeder<Travel>& f) const;
};

}

#endif /* LOOPBODIES_H_ */
