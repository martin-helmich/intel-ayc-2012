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
#include "tbb/mutex.h"
#include "../types.h"

using namespace std;
using namespace tbb;

namespace oma
{

class CostComputer
{
public:
	Travel *travel;
	double costs;

	CostComputer(Travel *t) :
			travel(t), costs(0)
	{
	}
	CostComputer(CostComputer &cc, split) :
			travel(cc.travel), costs(0)
	{
	}

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
			mutex *tl, mutex *ftl, unsigned long tmi, unsigned long tma, Parameters &p, Flight *c,
			Travel *ct, string to)
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

}

#endif /* COSTCOMPARATOR_H_ */
