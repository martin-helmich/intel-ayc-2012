/*!
 * @file tasks.cpp
 * @brief This file contains method bodies for custom tasks.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#include <iostream>
#include <limits>

#include "tbb/parallel_reduce.h"

#include "tasks.h"
#include "loop_bodies.h"
#include "../methods.h"

using namespace std;
using namespace oma;

void oma::find_path_task(string f, string t, int tmi, int tma, Parameters *p,
		vector<Travel> *tr, Alliances *a)
{
	Travels temp_travels, all_paths;
	CostRange min_range;

	fill_travel(&temp_travels, &all_paths, f, tmi, tma, &min_range, t, a);
	compute_path(t, &temp_travels, tmi, tma, *p, &all_paths, &min_range, a);

	#pragma omp parallel for shared(all_paths, tr)
	for (unsigned int i = 0; i < all_paths.size(); i++)
	{
		if ((&(all_paths.at(i)))->min_cost <= min_range.max)
		{
			tr->push_back(all_paths.at(i));
		}
	}

	return;
}

void oma::work_hard_task(Travels *htc, Travels *cth, Solution *s, Alliances *a)
{
	// Return empty travel when one of the two partial routes is empty.
	if (htc->size() == 0 || cth->size() == 0)
	{
		Travel empty;
		s->work_hard = empty;

		return;
	}

	CostRange mr;
	unsigned int s1 = htc->size(), s2 = cth->size();
	Travel *t1, *t2, *tf, *cheapest = NULL;
	Flight *l1, *f2;

	# pragma omp parallel for shared(cheapest,s2) collapse(2)
	for (unsigned int i = 0; i < s1; i++)
	{
		for (unsigned int j = 0; j < s2; j++)
		{
			t1 = &(htc->at(i));
			t2 = &(cth->at(j));
			l1 = &(t1->flights.back());
			f2 = &(t2->flights.front());

			if (l1->land_time < f2->take_off_time && t1->min_cost + t2->min_cost <= mr.max)
			{
				tf = new Travel(*t1);
				tf->merge_travel(t2, a);

				if (cheapest == NULL || tf->max_cost < cheapest->max_cost)
				{
					#pragma omp critical
					if (cheapest == NULL || tf->max_cost < cheapest->max_cost)
					{
						mr.from_travel(tf);
						cheapest = tf;
					}
				}
				else
				{
					delete tf;
				}
			}
		}
	}

	s->work_hard = *cheapest;

	return;
}

void oma::play_hard_task(Travels *htv, Travels *vtc, Travels *cth, Travels *htc,
		Travels *vth, Travels *ctv, Solution *s, unsigned int si, Alliances *a)
{
	vector<Travel> all_travels, home_to_vacation_to_conference;
	mutex rlock;
	task_list merge_paths;
	Travel *c1, *c2;

	#pragma omp parallel sections
	{
		#pragma omp section
		{
			c1 = merge_path_triple(&all_travels, &rlock, htv, vtc, cth, a);
		}

		#pragma omp section
		{
			c2 = merge_path_triple(&all_travels, &rlock, htc, ctv, vth, a);
		}
	}

	if (c1 != NULL) all_travels.push_back(*c1);
	if (c2 != NULL) all_travels.push_back(*c2);

	if (all_travels.size() == 0)
	{
		Travel empty;
		s->add_play_hard(si, empty);
	}
	else if (all_travels.size() == 1)
	{
		s->add_play_hard(si, all_travels[0]);
	}
	else
	{
		if (all_travels[0].max_cost < all_travels[1].max_cost) s->add_play_hard(
				si, all_travels[0]);
		else s->add_play_hard(si, all_travels[1]);
	}
}

oma::PlayHardMergeTripleTask::PlayHardMergeTripleTask(Travels *r, tbb::mutex *rl,
		Travels *t1, Travels *t2, Travels *t3, Alliances *a)
{
	results = r;
	results_lock = rl;
	travels1 = t1;
	travels2 = t2;
	travels3 = t3;
	alliances = a;
}

Travel* oma::merge_path_triple(Travels *r, tbb::mutex *rl,
		Travels *trs1, Travels *trs2, Travels *trs3, Alliances *a)
{
	unsigned int s1 = trs1->size(), s2 = trs2->size(), s3 = trs3->size();
	Travel *t1, *t2, *t3, *t12, *tf, *cheapest = NULL;
	Flight *l1, *l2, *f2, *f3;
	CostRange min_range;

	if(s1 * s2 * s3 == 0) return NULL;

	#pragma omp parallel for collapse(2) default(shared)
	for (unsigned int i = 0; i < s1; i++)
	{
		for (unsigned int j = 0; j < s2; j++)
		{
			t1 = &(trs1->at(i));
			t2 = &(trs2->at(j));

			l1 = &(t1->flights.back());
			f2 = &(t2->flights.front());

			if (l1 == NULL || f2 == NULL) continue;

			if (l1->land_time < f2->take_off_time)
			{
				t12 = new Travel(*t1);
				t12->merge_travel(t2, a);

				for (unsigned int k = 0; k < s3; k++)
				{
					t3 = &(trs3->at(k));

					l2 = &(t2->flights.back());
					f3 = &(t3->flights.front());

					if (l2->land_time < f3->take_off_time
							&& t12->min_cost + t3->min_cost <= min_range.max)
					{
						tf = new Travel(*t12);
						tf->merge_travel(t3, a);

						if (cheapest == NULL || tf->max_cost < cheapest->max_cost)
						{
							#pragma omp critical
							{
								cheapest = tf;
								min_range.from_travel(tf);
							}
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

	return cheapest;
}

tbb::task* oma::PlayHardMergeTripleTask::execute()
{
	PathMergingTripleOuterLoop pmtol(travels1, travels2, travels3, alliances);
	parallel_reduce(blocked_range<unsigned int>(0, travels1->size()), pmtol);

	if (pmtol.get_cheapest() != NULL)
	{
		mutex::scoped_lock l(*results_lock);
		results->push_back(*pmtol.get_cheapest());
	}

	return NULL;
}

/** This method spawns more compute path tasks. If a path to the destination
 *  is found, it is placed into the "final_travels" vector. */
void oma::compute_path_task(Travel *t, std::string &dst, Travels *ft, tbb::mutex *ftl,
			unsigned long &tmi, unsigned long &tma, Parameters *p, Alliances *a,
			CostRange *mr, tbb::concurrent_hash_map<string, Location> *lm, unsigned int l)
{
	Flight *current_city = &(t->flights.back());

	concurrent_hash_map<string, Location>::const_accessor ac;
	if (!lm->find(ac, current_city->to))
	{
		cerr << "Fehler: Stadt " << current_city->to << " ist nicht bekannt." << endl;
		return;
	}

	const Location *from = &(ac->second);

	unsigned int s = from->outgoing_flights.size();
	for (unsigned int i = 0; i < s; i++)
	{
		Flight *flight = (Flight*) &(from->outgoing_flights[i]);
		if (flight->take_off_time >= tmi && flight->land_time <= tma
				&& (flight->take_off_time > current_city->land_time)
				&& flight->take_off_time - current_city->land_time
						<= p->max_layover_time
				&& nerver_traveled_to(*t, flight->to)
				&& flight->cost * 0.7 + t->min_cost <= mr->max)
		{
			Travel *new_travel = new Travel(*t);
			new_travel->add_flight(*flight, a);

			if (flight->to == dst)
			{
				#pragma omp critical
				{
					ft->push_back(*new_travel);
					mr->from_travel(new_travel);
				}
			}
			else
			{
				#pragma omp task default(shared) firstprivate(new_travel)
				compute_path_task((Travel*) new_travel, dst, (Travels*) ft, (mutex*) ftl, tmi, tma,
						(Parameters*) p, (Alliances*) a, (CostRange*) mr,
						(tbb::concurrent_hash_map<std::string, Location>*) lm, l + 1);
			}
		}
	}


	if (l > 0)
	{
		delete t;
	}

	#pragma omp taskwait

	return;
}
