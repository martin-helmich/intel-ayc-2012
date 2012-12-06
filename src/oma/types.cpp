/*!
 * @file tasks.cpp
 * @brief This file contains method bodies for model classes.
 * @author Martin Helmich <martin.helmich@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 * @author Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>, University of Applied Sciences Osnabrück
 */

#include <iostream>

#include "../types.h"
#include "../methods.h"

using namespace std;

/**
 * This method adds a new flight to this travel. It also ensures that the
 * minimal and maximal costs are updated:
 *
 *   * The minimal costs are always updated with the flight's costs and the
 *     highest possible discount (i.e. 30%).
 *   * The maximal costs are updated with the flight's costs and the lowest
 *     possible discount (usually 0%, except a discount applies due to a
 *     previous flight).
 */
void Travel::add_flight(Flight &f, Alliances *a)
{
	float discount = 1.0;
	Flight *l = &(flights.back());

	if (size > 0)
	{
		if (l->company == f.company)
		{
			discount = 0.7;
		}
		else if (company_are_in_a_common_alliance(l->company, f.company, a))
		{
			discount = 0.8;
		}

		if (discount < 1)
		{
			if (discounts[size - 1] > discount)
			{
				max_cost -= (discounts[size - 1] - discount) * l->cost;
				discounts[size - 1] = discount;
			}
		}
	}

	min_cost += f.cost * 0.7;
	max_cost += f.cost * discount;

	flights.push_back(f);
	discounts.push_back(discount);

	size++;
}

void Travel::merge_travel(Travel *t, vector<vector<string> > *a)
{
	Flight *l1, *f2;
	float discount = 1.0;

	l1 = &(flights.back());
	f2 = &(t->flights.front());

	if (l1->company == f2->company)
	{
		discount = 0.7;
	}
	else if (company_are_in_a_common_alliance(l1->company, f2->company, a))
	{
		discount = 0.8;
	}

	flights.insert(flights.end(), t->flights.begin(), t->flights.end());
	discounts.insert(discounts.end(), t->discounts.begin(), t->discounts.end());

	// Reset pointers. The insert() calls above may have triggered a reallocation,
	// resulting in the pointers pointing to bad addresses.
	l1 = &(flights.at(size - 1));
	f2 = &(flights.at(size));

	min_cost += t->min_cost;
	max_cost += t->max_cost;

	if (discounts[size - 1] > discount)
	{
		max_cost -= (discounts[size - 1] - discount) * l1->cost;
		discounts[size - 1] = discount;
	}

	if (discounts[size] > discount)
	{
		max_cost -= (discounts[size] - discount) * f2->cost;
		discounts[size] = discount;
	}

	size += t->size;
}

void Travel::print()
{
	for (unsigned int i = 0; i < flights.size(); i++)
	{
		cout << flights[i].id << " (" << flights[i].cost << "@" << discounts[i] << ") - ";
	}
	cout << max_cost << endl;
}

/**
 * This constructor creates a new solution object and allocates memory for
 * *n* play hard solutions.
 */
Solution::Solution(unsigned int s)
{
	play_hard = new Travel[s];
}

/**
 * This method adds a new travel as a "play hard" solution.
 *
 * Concurrent access is generally possible, but should not be a problem as
 * long as every actor accesses a different index of the "play hard" array.
 */
void Solution::add_play_hard(unsigned int i, Travel &t)
{
	play_hard[i] = t;
}

/**
 * Initializes min and max prices with very high default values.
 */
CostRange::CostRange()
{
	min = numeric_limits<int>::max();
	max = numeric_limits<int>::max();
}

/**
 * This method reads min and max prices from an existing travel object.
 *
 * Access to this method is protected using a spinlock.
 */
void CostRange::from_travel(Travel *t)
{
	lock.lock();
	if (t->max_cost <= min)
	{
		// Use integers for comparison. We've had some problems with comparing
		// float values for equality, so we round up to the next highest integer value.
		max = (int) ceil(t->max_cost);
		min = (int) floor(t->min_cost);
	}
	lock.unlock();
}
