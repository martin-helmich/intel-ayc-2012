/*
 * types.cpp
 *
 *  Created on: 29.11.2012
 *      Author: mhelmich
 */

#include <iostream>

#include "../types.h"
#include "../methods.h"

using namespace std;

void Travel::add_flight(Flight &f, vector<vector<string> > *a)
{
	float discount = 1.0;

	if (size > 0)
	{
		if (flights[size - 1].company == f.company)
		{
			discount = 0.7;
		}
		else if (company_are_in_a_common_alliance(flights[size - 1].company,
				f.company, a)) discount = 0.8;

		if (discount < 1)
		{
			if (discounts[size - 1] > discount)
			{
				max_cost -= (discounts[size - 1] - discount) * flights[size - 1].cost;
				discounts[size - 1] = discount;
			}
		}
	}

	min_cost += f.cost * 0.7;
	max_cost += f.cost * discount;
	flights.push_back(f);
	discounts.push_back(discount);
	size ++;
}

void Travel::merge_travel(Travel *t, vector<vector<string> > *a)
{
	Flight *l1, *f2;
	float discount = 1.0;

	l1 = &(flights.back());
	f2 = &(t->flights.front());

	if (l1->company == f2->company)
	{
	}

	if (discounts[size - 1] > discount)
	{
		max_cost -= (discounts[size - 1] - discount) * l1->cost;
		discounts[size - 1] = discount;
	}

	if (t->discounts[0] > discount)
	{
		t->max_cost -= (t->discounts[0] - discount) * f2->cost;
		t->discounts[0] = discount;
	}

	flights.insert(flights.end(), t->flights.begin(), t->flights.end());
	discounts.insert(discounts.end(), t->discounts.begin(), t->discounts.end());

	min_cost += t->min_cost;
	max_cost += t->max_cost;
}
