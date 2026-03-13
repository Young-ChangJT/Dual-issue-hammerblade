#pragma once
#include <vector>
void host_bfs(
    int root,
    int V,
    const int* fwd_offsets,
    const int* fwd_nonzeros,
    const int* rev_offsets,
    const int* rev_nonzeros,
    std::vector<int> &distance,
    std::vector<int> &direction
);
