Word Hard/Play Hard Problem
===========================

Martin Helmich <martin.helmich@hs-osnabrueck.de>
Oliver Erxleben <oliver.erxleben@hs-osnabrueck.de>

Problem description
-------------------

1. Find optimal route from an origin to a destination and vice versa (side
   conditions apply, like time constraints or different discounts, applied to
   flights of same airline or of airlines of common "alliances").
2. Find optimal routes encompassing the destination point and each of any
   number of additional destinations. Order of intermediate destinations
   does not matter.
   
Problem solution
----------------

### Algorithm

1. Read input file. Build a graph with all flight targets and destinations as
   nodes. **If a flight's depature time lies outside of the specified time
   window, ignore it.**

2. Divide the problem up into partial routes consisting of two locations each.
   Then, for each partial route:
   
   1. Find a set of potentially cheapest routes by performing a breadth-first
      search on the flight graph. Consider side conditions (time frame)
      whenever necessary.
   2. Apply flight discounts while building possible routes. Since final
      prices cannot be determined due to potential discounts, use *fuzzy price
      ranges*, describing the lowest and highest possible costs of each route.
   3. Ignore route if lowest possible price is greater that highest possible
      price of cheapest known route.
      
3. Merge two sets of possible partial routes by building carthesian product of
   partial routes. Consider side conditions. Cheapest route of merged is
   solution for problem (1). Use *fuzzy price ranges* like in (2).

4. For each additional destination: Like (3). Merge three sets for each
   additional locations. Cheapest routes are solutions for problem (2).
   
### Parallelism

1. Input file is parsed in parallel.
2. Partial routes are computed using recursive task-based parallelism.
3. Merging of routes is performed using multidimensional parallel loops.
   
### Other

1. Use pointers whenever possible!
2. Avoid std::strings and use simple *char pointers instead!