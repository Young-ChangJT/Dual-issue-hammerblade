#include "bsg_manycore_regression.h"
#include "breadth_first_search_graph.hpp"
#include <set>

void breadth_first_search_graph(
    int root, int V,
    const std::vector<int>&fwd_offsets,
    const std::vector<int>&fwd_nonzeros,
    const std::vector<int>&rev_offsets,
    const std::vector<int>&rev_nonzeros,
    std::vector<int> &distance
) 
{
    distance = std::vector<int>(V, -1);
    distance[root] = 0;
    std::set<int> curr_frontier, next_frontier;
    curr_frontier.insert(root);

    int d = 0;
    int rev_not_fwd = 0;  // start fwd;

    while (!curr_frontier.empty()) {
        d++;

        // determine direction;
        if (rev_not_fwd) {
          // rev -> fwd?
          // heuristic;
          rev_not_fwd = curr_frontier.size() >= (V/20);
        } else {
          // fwd -> rev?
          // number of edges going out from the frontier;
          int mf = 0;
          for (int src : curr_frontier) {
            mf += (fwd_offsets[src+1] - fwd_offsets[src]);
          }
          // number of edges going out from the unvisited;
          int mu = 0;
          for (int i = 0; i < distance.size(); i++) {
            if (distance[i] == -1) {
              mu += (fwd_offsets[i+1] - fwd_offsets[i]);
            }
          }
      
          // heuristic;
          rev_not_fwd = mf >= (mu/20);
        }       

        int edge_traversed = 0;
        int frontier_size = curr_frontier.size();

        if (rev_not_fwd) {
          // rev;
          for (int dst = 0; dst < distance.size(); dst++) {
            if (distance[dst] == -1) {
              int start = rev_offsets[dst];
              int stop = rev_offsets[dst+1];
              for (int nz = start; nz < stop; nz++) {
                int src = rev_nonzeros[nz];
                edge_traversed++;
                if (curr_frontier.find(src) != curr_frontier.end()) {
                  distance[dst] = d;
                  next_frontier.insert(dst);
                  break;
                }
              }
            }
          }
        } else {
          // fwd;
          for (int src : curr_frontier) {
            int start = fwd_offsets[src];
            int stop = fwd_offsets[src+1];
            edge_traversed += stop-start;
            for (int nz = start; nz < stop; nz++) {
              int dst = fwd_nonzeros[nz];
              if (distance[dst] == -1) {
                distance[dst] = d;
                next_frontier.insert(dst);
              }
            }
          }
        }

        bsg_pr_info("d=%d, rev_not_fwd=%d, frontier_size=%d, edge_traversed=%d\n",
          d, rev_not_fwd, frontier_size, edge_traversed);

        // swap frontier;
        curr_frontier = std::move(next_frontier);
        next_frontier.clear();
    }
}
