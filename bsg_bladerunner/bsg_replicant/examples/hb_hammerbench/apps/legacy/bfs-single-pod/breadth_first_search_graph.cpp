#include "bsg_manycore_regression.h"
#include "breadth_first_search_graph.hpp"
#include <set>
void breadth_first_search_graph(
    int root, int V, int E,
    const std::vector<int>&fwd_offsets,
    const std::vector<int>&fwd_nonzeros,
    std::vector<int> &distance) {
    int traversed = 0;
    distance = std::vector<int>(V, -1);
    distance[root] = 0;
    std::set<int> curr_frontier, next_frontier;
    curr_frontier.insert(root);
    int d = 0;
    while (!curr_frontier.empty()) {
        d++;
        int traversed_this_iter = 0;
        for (int src : curr_frontier) {
            int start = fwd_offsets[src];
            int stop = fwd_offsets[src+1];
            traversed_this_iter += stop-start;
            for (int nz = start; nz < stop; nz++) {
                int dst = fwd_nonzeros[nz];
                if (distance[dst] == -1) {
                    distance[dst] = d;
                    next_frontier.insert(dst);
                }
            }
        }
        curr_frontier = std::move(next_frontier);
        next_frontier.clear();
        bsg_pr_info("breadth first search iteration %d: traversed %d edges\n",
                    d-1,
                    traversed_this_iter);
        traversed += traversed_this_iter;
    }
    bsg_pr_info("breadth first search traversed %d edges\n", traversed);        
}
