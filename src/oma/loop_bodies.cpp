/*!
 * @file loop_bodies.cpp
 * @brief This file contains implementations for various parallel loop bodies.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#include <iostream>

#include "../methods.h"
#include "loop_bodies.h"

#include "tbb/concurrent_hash_map.h"

using namespace std;

oma::ParseFlightsLoop::ParseFlightsLoop(char* i, vector<int>* l, Parameters *p)
{
	input = i;
	lfs = l;
	parameters = p;
}

void oma::ParseFlightsLoop::operator()(const blocked_range<int> range) const
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

		// Parse line.
		parse_flight(b, parameters);
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
	Travel *t1, *t2, *tf;
	Flight *l1, *f2;

	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		t1 = &(travels1->at(i));
		for (unsigned j = 0; j < s2; j++)
		{
			t2 = &(travels2->at(j));
			l1 = &(t1->flights.back());
			f2 = &(t2->flights.front());
			if (l1->land_time < f2->take_off_time
					&& t1->min_cost + t2->min_cost <= min_range.max)
			{
				tf = new Travel(*t1);
				tf->merge_travel(t2, alliances);

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
	Travel *t1, *t2, *t3, *t12, *tf;
	Flight *l1, *l2, *f2, *f3;

	for (unsigned int i = range.begin(); i != range.end(); ++i)
	{
		t1 = &(travels1->at(i));
		for (unsigned int j = 0; j < s2; j++)
		{
			t2 = &(travels2->at(j));

			l1 = &(t1->flights.back());
			f2 = &(t2->flights.front());

			if (l1->land_time < f2->take_off_time)
			{
				t12 = new Travel(*t1);
				t12->merge_travel(t2, alliances);

				for (unsigned int k = 0; k < s3; k++)
				{
					t3 = &(travels3->at(k));

					l2 = &(t2->flights.back());
					f3 = &(t3->flights.front());

					if (l2->land_time < f3->take_off_time
							&& t12->min_cost + t3->min_cost <= min_range.max)
					{
						tf = new Travel(*t12);
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

				delete t12;
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

void oma::ComputePathOuterLoop::operator()(Travel t,
		parallel_do_feeder<Travel>& f) const
{
	Flight *current_city = &(t.flights.back());

	// First, if a direct flight exist, it must be in the final travels.
	// Should not occur, since we already filter these out in fill_travel.
	if (current_city->to == to)
	{
		mutex::scoped_lock l(*final_travels_lock);

		min_range->from_travel(&t);
		final_travels->push_back(t);
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

		unsigned int s = from->outgoing_flights.size();
		for (unsigned int i = 0; i < s; i++)
		{
			Flight *flight = (Flight*) &(from->outgoing_flights[i]);
			if (flight->take_off_time >= t_min && flight->land_time <= t_max
					&& (flight->take_off_time > current_city->land_time)
					&& flight->take_off_time - current_city->land_time
							<= parameters.max_layover_time
					&& nerver_traveled_to(t, flight->to)
					&& flight->cost * 0.7 + t.min_cost <= min_range->max)
			{

				Travel new_travel = t;
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

oma::FilterPathsLoop::FilterPathsLoop(Travels *i, Travels *o, CostRange *r)
{
	in = i;
	out = o;
	range = r;
}

oma::FilterPathsLoop::FilterPathsLoop(FilterPathsLoop &fpl, split)
{
	in = fpl.in;
	range = fpl.range;
	out = new Travels;
}

void oma::FilterPathsLoop::operator ()(blocked_range<unsigned int> r)
{
	for (unsigned int i = r.begin(); i != r.end(); ++i)
	{
		if ((&(in->at(i)))->min_cost <= range->max)
		{
			out->push_back(in->at(i));
		}
	}
}

void oma::FilterPathsLoop::join(FilterPathsLoop &fpl)
{
	out->insert(out->end(), fpl.out->begin(), fpl.out->end());
	delete fpl.out;
}
