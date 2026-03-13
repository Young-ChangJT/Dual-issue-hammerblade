#pragma once
#include <vector>
void breadth_first_search_graph(
    int root, int V, int E,
    const std::vector<int>&fwd_offsets,
    const std::vector<int>&fwd_nonzeros,
    std::vector<int> &distance);
