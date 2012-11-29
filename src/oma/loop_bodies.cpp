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

oma::PathMergingOuterLoop::PathMergingOuterLoop(Travels *t1, Travels *t2,
		Alliances *a, bool f)
{
	travels1 = t1;
	travels2 = t2;
	alliances = a;
	cheapest = NULL;
	final = f;
}

oma::PathMergingOuterLoop::PathMergingOuterLoop(PathMergingOuterLoop &pmol, split)
{
	travels1 = pmol.travels1;
	travels2 = pmol.travels2;
	alliances = pmol.alliances;
	cheapest = NULL;
	final = pmol.final;
}

void oma::PathMergingOuterLoop::operator()(blocked_range<unsigned int> &range)
{
	unsigned int s2 = travels2->size();

	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		Travel *t1 = &(travels1->at(i));
		for (unsigned j = 0; j < s2; j++)
		{
			Travel *t2 = &(travels2->at(j));
			Flight *last_flight_t1 = &t1->flights.back();
			Flight *first_flight_t2 = &t2->flights[0];
			if (last_flight_t1->land_time < first_flight_t2->take_off_time
					&& t1->min_cost + t2->min_cost < min_range.max)
			{
				Travel *new_travel = new Travel(*t1), t2c = *t2;
				new_travel->merge_travel(&t2c, alliances);

				min_range.from_travel(new_travel);

				if (final)
				{
					if (cheapest == NULL || new_travel->max_cost < cheapest->max_cost)
					{
						cheapest = new_travel;
					}
					else
					{
						delete new_travel;
					}
				}
				else
				{
					results.push_back(*new_travel);
				}
			}
		}
	}
}

void oma::PathMergingOuterLoop::join(PathMergingOuterLoop &pmol)
{
	if (final)
	{
		if (pmol.cheapest != NULL && pmol.cheapest->max_cost < cheapest->max_cost)
		{
			cheapest = pmol.cheapest;
		}
	}
	else
	{
		results.insert(results.end(), pmol.results.begin(), pmol.results.end());
		min_range.cas(&pmol.min_range);
	}
}

Travels *oma::PathMergingOuterLoop::get_results() { return &results; }
Travel *oma::PathMergingOuterLoop::get_cheapest() { return cheapest; }
