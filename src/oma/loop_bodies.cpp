/*
 * CostComparator.cpp
 *
 *  Created on: 13.11.2012
 *      Author: mhelmich
 */


#include "loop_bodies.h"


void oma::CostComputer::operator()(const blocked_range<unsigned int> range)
{
	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		costs += flights[i].cost * flights[i].discout;
	}
}

void oma::CostComputer::join(CostComputer &cc)
{
	costs += cc.costs;
}
