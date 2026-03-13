#pragma once
#include <vector>
void breadth_first_search_graph(
    int root, int V,
    const std::vector<int>&fwd_offsets,
    const std::vector<int>&fwd_nonzeros,
    const std::vector<int>&rev_offsets,
    const std::vector<int>&rev_nonzeros,
    std::vector<int> &distance
);
