/*
 * CostComparator.h
 *
 *  Created on: 13.11.2012
 *      Author: mhelmich
 */

#ifndef COSTCOMPARATOR_H_
#define COSTCOMPARATOR_H_

#include <vector>
#include "tbb/blocked_range.h"
#include "../types.h"

using namespace std;
using namespace tbb;

namespace oma
{

class CostComputer
{
public:
	vector<Flight> &flights;
	float costs;

	CostComputer(vector<Flight> &f) :
			flights(f), costs(0)
	{
	}
	;
	CostComputer(CostComputer &cc, split) :
			flights(cc.flights), costs(0)
	{
	}
	;

	void operator()(const blocked_range<unsigned int> range);
	void join(CostComputer &cc);
};

class CostComputingLoop
{
public:
	vector<vector<string> > alliances;

	CostComputingLoop(vector<vector<string> > &a) :
			alliances(a)
	{
	}

	void operator()(const blocked_range<unsigned int> range) const;
};

}

#endif /* COSTCOMPARATOR_H_ */
