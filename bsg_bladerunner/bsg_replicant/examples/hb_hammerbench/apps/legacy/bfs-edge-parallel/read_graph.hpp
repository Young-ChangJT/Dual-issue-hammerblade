#pragma once
#include <string>
#include <vector>
int read_graph(const std::string &graph,
               /* outputs */
               int *V,
               int *E,
               std::vector<int> &offsets,
               std::vector<int> &nonzeros);

