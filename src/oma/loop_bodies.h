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
#include "tbb/blocked_range2d.h"
#include "tbb/blocked_range3d.h"
#include "tbb/mutex.h"
#include "tbb/parallel_do.h"
#include "tbb/concurrent_hash_map.h"
#include "../types.h"

using namespace std;
using namespace tbb;

namespace oma
{

/// Loop body for parsing flights from an input file.
/** This loop body expects that the entire input file is mapped into memory
 *  (for example via "mmap") and that the positions of all line endings in this
 *  file are known. Based on the known positions of line endings, the individual
 *  lines can then be parsed in parallel. */
class ParseFlightsLoop
{
private:
	char *input;
	vector<int>* lfs;
	Parameters *parameters;

public:

	/// Initial constructor.
	/** @param i Pointer to mapped input file.
	 *  @param l Pointer to vector containing positions of line endings (LFs).
	 *  @param p Pointer to parameters object. */
	ParseFlightsLoop(char* i, vector<int>* l, Parameters *p);

	/// Actual loop body.
	/** @param range Range to be iterated over. */
	void operator()(const blocked_range<int> range) const;
};

/// Loop body for merging two seperate sets of routes into one set of routes.
/** This loop body merges two vectors of routes (i.e. vector<Travel>) into
 *  one vector of routes. This is done by building all possible combinations of
 *  travels in vector a and all travels in vector b (assuming the landing time
 *  of the last flight in a and the takeoff time of the first flight in b match).
 *
 *  This loop also determines the cheapest route on-the-fly. */
class PathMergingOuterLoop
{
protected:
	Travels *travels1, *travels2;
	Alliances *alliances;
	Travel *cheapest;
	CostRange min_range;

public:

	/// Initial constructor.
	/** @param t1 First travel vector.
	 *  @param t2 Second travel vector.
	 *  @param a Alliance list. */
	PathMergingOuterLoop(Travels *t1, Travels *t2, Alliances *a);

	/// Split constructor.
	/** @param pmol Parent loop. */
	PathMergingOuterLoop(PathMergingOuterLoop &pmol, split);

	/// Actual loop body.
	/** @param range Range to be iterated over. */
	void operator()(blocked_range2d<unsigned int, unsigned int> &range);

	/// Joins two loop results into one.
	/** @param pmol Another loop body. */
	void join(PathMergingOuterLoop& pmol);

	/// Gets the cheapest combination of input travels.
	Travel* get_cheapest();
};

/// Loop body for merging THREE seperate sets of routes into one set of routes.
/** This loop body merges THREE vectors of routes (i.e. vector<Travel>) into
 *  one vector of routes. This is done by building all possible combinations of
 *  travels in vector a, b and c (assuming the landing time
 *  of the last flight in {a,b} and the takeoff time of the first flight in {b,c} match).
 *
 *  This loop also determines the cheapest route on-the-fly. */
class PathMergingTripleOuterLoop
{
protected:
	Travels *travels1, *travels2, *travels3;
	Alliances *alliances;
	Travel *cheapest;
	CostRange min_range;

public:

	/// Initial constructor.
	/** @param t1 First travel vector.
	 *  @param t2 Second travel vector.
	 *  @param t3 Third travel vector.
	 *  @param a Alliance list. */
	PathMergingTripleOuterLoop(Travels *t1, Travels *t2, Travels *t3, Alliances *a);

	/// Split constructor.
	/** @param pmol Parent loop. */
	PathMergingTripleOuterLoop(PathMergingTripleOuterLoop &pmol, split);

	/// Actual loop body.
	/** @param range Range to be iterated over. */
	void operator()(blocked_range3d<unsigned int, unsigned int, unsigned int> &range);

	/// Joins two loop results into one.
	/** @param pmol Another loop body. */
	void join(PathMergingTripleOuterLoop& pmol);

	/// Gets the cheapest combination of input travels.
	Travel* get_cheapest();
};

/// Loop body for filtering travels by minimal costs.
/** This loop body filters a set of travels by a predefined minimal cost.
 *  It takes an input vector "in" and a "CostRange" object pointer as arguments and
 *  fills its output vector "out" with all travels from "in" that are possibly
 *  cheaper than defined by the cost range. */
class FilterPathsLoop
{
private:
	Travels *in, *out;
	CostRange *range;

public:
	/// Initial constructor.
	/** @param i Input vector.
	 *  @param o Output vector.
	 *  @param r Comparison cost range. */
	FilterPathsLoop(Travels *i, Travels *o, CostRange *r);

	/// Split constructor.
	/** @param fpl Parent loop body. */
	FilterPathsLoop(FilterPathsLoop &fpl, split);

	/// Loop body.
	/** @param r Range to be iterated over. */
	void operator()(blocked_range<unsigned int> r);

	/// Reduce/Join-Function.
	/** @param fpl Loop body to be joined. */
	void join(FilterPathsLoop &fpl);
};

}

#endif /* LOOPBODIES_H_ */
