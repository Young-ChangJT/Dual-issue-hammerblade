#include "bsg_manycore.h"
#include "read_graph.hpp"
#include "mmio.h"
#include <vector>
#include <algorithm>

int read_graph(const std::string &graph,
               /* outputs */
               int *V,
               int *E,
               std::vector<int> &offsets,
               std::vector<int> &nonzeros){
    // 1. open the file
    printf("Reading file '%s'\n", graph.c_str());
    FILE *f = fopen(graph.c_str(), "r");
    if (!f) {
        fprintf(stderr, "Error: failed to open '%s': %m\n", graph.c_str());
        return HB_MC_FAIL;
    }
    // 2. read the banner and matrix info
    MM_typecode banner;
    int r = mm_read_banner(f, &banner);
    if (r != 0) {
        fprintf(stderr, "Error: failed to read banner from '%s': %m\n", graph.c_str());
        return HB_MC_FAIL;
    }

    if (!mm_is_sparse(banner) ||
        !(mm_is_real(banner) || mm_is_integer(banner) || mm_is_pattern(banner)) ||
        !mm_is_general(banner)) {
        fclose(f);
        fprintf(stderr, "Unsupported graph input\n");
        return HB_MC_FAIL;
    }

    int M, N, nz;
    r = mm_read_mtx_crd_size(f, &M, &N, &nz);
    if (r != 0) {
        fclose(f);
        fprintf(stderr, "Error: failed to matrix crd size from '%s': %m\n", graph.c_str());
        return HB_MC_FAIL;
    }
    *V = M;
    *E = nz;
    printf("Reading graph '%s': V = %d, E = %d\n", graph.c_str(), *V, *E);
    // 3. read the nonzeros
    std::vector<std::vector<int>> rows((*V)+1);
    for (int i = 0; i < nz; i++) {
        int s, d, vi;
        float vd;
        if (mm_is_real(banner)) {
            r = fscanf(f, "%d %d %f", &s, &d, &vd);
        } else if (mm_is_pattern(banner)) {
            r = fscanf(f, "%d %d", &s, &d);
        } else {
            r = fscanf(f, "%d %d %d", &s, &d, &vi);            
        }
        
        if ((r != 3) && (r != 2)) {
            fprintf(stderr, "Error: unexpected end of file for '%s': %m\n", graph.c_str());
            fclose(f);
            return HB_MC_FAIL;
        }
        // matrix market is 1 indexed, but we use zero indexing
        d--; s--;
        rows[s].push_back(d);
    }
    fclose(f);
    // 4. conver to csr
    offsets = std::vector<int>((*V)+1);
    nonzeros = std::vector<int>(nz);
    int sum = 0;
    for (int i = 0; i < offsets.size(); i++) {
        offsets[i] = sum;
        sum += rows[i].size();
    }

    int n = 0;
    for (int i = 0; i < *V; i++) {
        std::sort(rows[i].begin(), rows[i].end());
        for (int j : rows[i]) {
            nonzeros[n++] = j;
        }
    }
    return HB_MC_SUCCESS;
}
