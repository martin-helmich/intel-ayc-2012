/*
 * CostComparator.cpp
 *
 *  Created on: 13.11.2012
 *      Author: mhelmich
 */


#include "../methods.h"
#include "loop_bodies.h"


void oma::CostComputer::operator()(const blocked_range<unsigned int> range)
{
	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		costs += travel->flights[i].cost * travel->discounts[i];
	}
}

void oma::CostComputer::join(CostComputer &cc)
{
	costs += cc.costs;
}

void oma::PathComputingInnerLoop::operator()(blocked_range<unsigned int> &range) const
{
	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		Flight flight = flights->at(i);

		if (flight.take_off_time >= t_min && flight.land_time <= t_max
				&& (flight.take_off_time > current_city->land_time)
				&& flight.take_off_time - current_city->land_time
						<= parameters.max_layover_time
				&& nerver_traveled_to(*travel, flight.to))
		{
			Travel newTravel = *travel;
			newTravel.flights.push_back(flight);
			if (flight.to == to)
			{
				final_travels_lock->lock();
				final_travels->push_back(newTravel);
				final_travels_lock->unlock();
			}
			else
			{
				travels_lock->lock();
				travels->push_back(newTravel);
				travels_lock->unlock();
			}
		}
	}
}
