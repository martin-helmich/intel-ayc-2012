/*
 * CostComparator.cpp
 *
 *  Created on: 13.11.2012
 *      Author: mhelmich
 */

#include <iostream>

#include "../methods.h"
#include "loop_bodies.h"

#include "tbb/concurrent_hash_map.h"

using namespace std;

oma::ParseFlightsLoop::ParseFlightsLoop(char* i, vector<int>* l, vector<Flight> *f) :
		input(i), lfs(l)
{
	flights = f;
}

oma::ParseFlightsLoop::ParseFlightsLoop(ParseFlightsLoop &fp, split) :
		input(fp.input), lfs(fp.lfs)
{
	flights = new vector<Flight>;
}

void oma::ParseFlightsLoop::setFlights(vector<Flight> *f)
{
	flights = f;
}

void oma::ParseFlightsLoop::operator()(const blocked_range<int> range)
{
	for (int i = range.begin(); i != range.end(); ++i)
	{
		// Determine the length of the line by computing the difference between the
		// current LF character and the previous (does always work, since the first element
		// in the "lfs" vector is a "-1").
		// Then, allocate an appropriate amount of memory (plus 1 byte for the trailing 0-byte).
		char* b = (char*) malloc(lfs->at(i) - lfs->at(i - 1) + 1);

		// Copy the line into the previously allocated line buffer.
		strncpy(b, (input + lfs->at(i - 1) + 1), lfs->at(i) - lfs->at(i - 1) - 1);

		// Add 0-byte to mark string end.
		b[lfs->at(i) - lfs->at(i - 1) - 1] = 0x00;

		// Create string from character array and parse line.
		string s(b);
		parse_flight(flights, s);
	}
}

void oma::ParseFlightsLoop::join(ParseFlightsLoop &fp)
{
	// Merge flight lists.
	flights->insert(flights->end(), fp.flights->begin(), fp.flights->end());
	delete fp.flights;
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

oma::PathMergingOuterLoop::PathMergingOuterLoop(Travels *t1, Travels *t2, Alliances *a)
{
	travels1 = t1;
	travels2 = t2;
	alliances = a;
	cheapest = NULL;
}

oma::PathMergingOuterLoop::PathMergingOuterLoop(PathMergingOuterLoop &pmol, split)
{
	travels1 = pmol.travels1;
	travels2 = pmol.travels2;
	alliances = pmol.alliances;
	cheapest = NULL;
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
					&& t1->min_cost + t2->min_cost <= min_range.max)
			{
				Travel *new_travel = new Travel(*t1), t2c = *t2;
				new_travel->merge_travel(&t2c, alliances);

				min_range.from_travel(new_travel);

				if (cheapest == NULL || new_travel->max_cost < cheapest->max_cost)
				{
					cheapest = new_travel;
				}
				else
				{
					delete new_travel;
				}
			}
		}
	}
}

void oma::PathMergingOuterLoop::join(PathMergingOuterLoop &pmol)
{
	if (pmol.cheapest != NULL && pmol.cheapest->max_cost < cheapest->max_cost)
	{
		cheapest = pmol.cheapest;
	}
}

Travel *oma::PathMergingOuterLoop::get_cheapest()
{
	return cheapest;
}

oma::PathMergingTripleOuterLoop::PathMergingTripleOuterLoop(Travels *t1, Travels *t2,
		Travels *t3, Alliances *a)
{
	travels1 = t1;
	travels2 = t2;
	travels3 = t3;
	alliances = a;
	cheapest = NULL;
}

oma::PathMergingTripleOuterLoop::PathMergingTripleOuterLoop(
		PathMergingTripleOuterLoop &pmol, split)
{
	travels1 = pmol.travels1;
	travels2 = pmol.travels2;
	travels3 = pmol.travels3;
	alliances = pmol.alliances;
	cheapest = NULL;
}

void oma::PathMergingTripleOuterLoop::operator()(blocked_range<unsigned int> &range)
{
	unsigned int s2 = travels2->size(), s3 = travels3->size();

	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		Travel *t1 = &(travels1->at(i));
		for (unsigned int j = 0; j < s2; j++)
		{
			Travel *t2 = &(travels2->at(j));

			Flight *last_flight_t1 = &t1->flights.back();
			Flight *first_flight_t2 = &t2->flights[0];

			if (last_flight_t1->land_time < first_flight_t2->take_off_time)
			{
				Travel *t12 = new Travel(*t1);
				t12->merge_travel(t2, alliances);

				for (unsigned int k = 0; k < s3; k++)
				{
					Travel *t3 = &(travels3->at(k));

					Flight *last_flight_t2 = &t2->flights.back();
					Flight *first_flight_t3 = &t3->flights.front();

					if (last_flight_t2->land_time < first_flight_t3->take_off_time
							&& t12->min_cost + t3->min_cost <= min_range.max)
					{
						Travel *tf = new Travel(*t12);
						tf->merge_travel(t3, alliances);
						min_range.from_travel(tf);

						if (cheapest == NULL || tf->max_cost < cheapest->max_cost)
						{
							cheapest = tf;
						}
						else
						{
							delete tf;
						}
					}
				}
			}
		}
	}
}

void oma::PathMergingTripleOuterLoop::join(PathMergingTripleOuterLoop &pmol)
{
	if (pmol.cheapest != NULL && pmol.cheapest->max_cost < cheapest->max_cost)
	{
		cheapest = pmol.cheapest;
	}
}

Travel *oma::PathMergingTripleOuterLoop::get_cheapest()
{
	return cheapest;
}

oma::ComputePathOuterLoop::ComputePathOuterLoop(vector<Travel> *ft, mutex *ftl,
		Parameters &p, string t, unsigned long tmi, unsigned long tma, CostRange *r,
		Alliances *a, concurrent_hash_map<string, Location> *lm)
{
	final_travels = ft;
	final_travels_lock = ftl;
	parameters = p;
	t_min = tmi;
	t_max = tma;
	to = t;
	min_range = r;
	alliances = a;
	location_map = lm;
}

void oma::ComputePathOuterLoop::operator()(Travel travel,
		parallel_do_feeder<Travel>& f) const
{
	Flight *current_city = &(travel.flights.back());

	// First, if a direct flight exist, it must be in the final travels.
	// Should not occur, since we already filter these out in fill_travel.
	if (current_city->to == to)
	{
		mutex::scoped_lock l(*final_travels_lock);

		min_range->from_travel(&travel);
		final_travels->push_back(travel);
	}
	else
	{
		concurrent_hash_map<string, Location>::const_accessor a;
		if (!location_map->find(a, current_city->to))
		{
			cerr << "Fehler: Stadt " << current_city->to << " ist nicht bekannt." << endl;
			exit(100);
		}

		const Location *from = &(a->second);

		/*PathComputingInnerLoop loop(travels, final_travels,
		 &from.outgoing_flights, &travels_lock, &final_travels_lock,
		 t_min, t_max, parameters, &current_city, &travel, to, &best);
		 parallel_for(
		 blocked_range<unsigned int>(0,
		 from.outgoing_flights.size()), loop);*/

		unsigned int s = from->outgoing_flights.size();
		for (unsigned int i = 0; i < s; i++)
		{
			Flight *flight = (Flight*) &(from->outgoing_flights[i]);
			if (flight->take_off_time >= t_min && flight->land_time <= t_max
					&& (flight->take_off_time > current_city->land_time)
					&& flight->take_off_time - current_city->land_time
							<= parameters.max_layover_time
					&& nerver_traveled_to(travel, flight->to)
					&& flight->cost * 0.7 + travel.min_cost <= min_range->max)
			{

				Travel new_travel = travel;
				new_travel.add_flight(*flight, alliances);

				if (flight->to == to)
				{
					mutex::scoped_lock lock(*final_travels_lock);

					final_travels->push_back(new_travel);
					min_range->from_travel(&new_travel);
				}
				else
				{
					f.add(new_travel);
				}
			}
		}
	}
}
