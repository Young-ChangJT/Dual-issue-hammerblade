#include "bsg_manycore.h"
#include "bsg_manycore.hpp"
#include "bsg_manycore_atomic.h"
#include "bsg_cuda_lite_barrier.h"
#include "bsg_mcs_mutex.h"
#include <algorithm>

#define TAG_TO_SPARSE 0
#define TAG_TO_DENSE  1
#define TAG_ZERO_DENSE  2
#define TAG_FWD_TRAVERSAL 3
#define TAG_REV_TRAVERSAL 4
#define TAG_DECIDE_DIRECTION 5

__attribute__((section(".dram")))
int g_root;

__attribute__((section(".dram")))
int g_V;
int V;
__attribute__((section(".dram")))
int g_E;
int E;

__attribute__((section(".dram")))
const int *g_fwd_offsets;
const int *fwd_offsets;

__attribute__((section(".dram")))
const int *g_fwd_nonzeros;
const int *fwd_nonzeros;

__attribute__((section(".dram")))
const int *g_rev_offsets;
const int *rev_offsets;

__attribute__((section(".dram")))
const int *g_rev_nonzeros;
const int *rev_nonzeros;

__attribute__((section(".dram")))
int *g_distance;
int *distance;

__attribute__((section(".dram")))
int *g_curr_frontier;
__attribute__((section(".dram")))
int g_curr_frontier_size = 0;
__attribute__((section(".dram")))
int g_curr_frontier_dense = 0;

__attribute__((section(".dram")))
int *g_next_frontier;
__attribute__((section(".dram")))
int  g_next_frontier_size = 0;
__attribute__((section(".dram")))
int  g_next_frontier_dense = 1;

__attribute__((section(".dram")))
int g_rev_not_fwd = 0;

__attribute__((section(".dram")))
int *g_dense_to_sparse_set;

__attribute__((section(".dram")))
int *g_sparse_to_dense_set;

__attribute__((section(".dram")))
int g_mu = 0;
__attribute__((section(".dram")))
int g_mf = 0;

__attribute__((section(".dram")))
bsg_mcs_mutex_t printf_lock = 0;

#define serial(block)                           \
    do {                                        \
        bsg_barrier_hw_tile_group_sync();       \
        if (__bsg_id == 0) {                    \
            block;                              \
        }                                       \
        bsg_fence();                            \
        bsg_barrier_hw_tile_group_sync();       \
    } while (0)

//#define DEBUG
#ifdef DEBUG
#define pr_int_dbg(i)                           \
    bsg_print_int(i)
#else
#define pr_int_dbg(i)
#endif

//#define VDEBUG
#ifdef  VDEBUG
#define pr_dbg(fmt, ...)                                                \
    do {                                                                \
        bsg_mcs_mutex_node_t __node;                                    \
        bsg_mcs_mutex_node_t*__nptr = bsg_tile_group_remote_pointer(__bsg_x, __bsg_y, &__node); \
        bsg_mcs_mutex_acquire(&printf_lock, &__node, __nptr);           \
        bsg_printf(fmt, ##__VA_ARGS__);                                 \
        bsg_mcs_mutex_release(&printf_lock, &__node, __nptr);            \
    } while (0);
#else
#define pr_dbg(fmt, ...)
#endif

//#define VVDEBUG
#ifdef  VVDEBUG
#define pr_vdbg(fmt, ...)                                               \
    do {                                                                \
        bsg_mcs_mutex_node_t __node;                                    \
        bsg_mcs_mutex_node_t*__nptr = bsg_tile_remote_group_pointer(__bsg_x, __bsg_y, &__node); \
        bsg_mcs_mutex_acquire(&printf_lock, &__node, __nptr);           \
        bsg_printf(fmt, ##__VA_ARGS__);                                 \
        bsg_mcs_mutex_release(&printf_lock, &__node, __nptr);            \
    } while (0);
#else
#define pr_vdbg(fmt, ...)
#endif

#define pfor_break                              \
    do {return 1;} while (0)

template <typename BodyT>
static inline void parallel_foreach_static(int START, int STOP, int STEP, BodyT body)
{
    for (int i = START+__bsg_id*STEP; i < STOP; i += STEP*bsg_tiles_X*bsg_tiles_Y) {
        if (body(i)) break;
    }
}

template <typename BodyT>
static inline void parallel_foreach_dynamic(int &START, int STOP, int STEP, BodyT body) {
    for (int i = bsg_amoadd(&START, STEP);
         i < STOP;
         i = bsg_amoadd(&START, STEP)) {
        if (body(i)) break;
    }
}

static inline void zero_dense_set(int *set)
{
    bsg_barrier_hw_tile_group_sync();
    bsg_cuda_print_stat_start(TAG_ZERO_DENSE);
    int words = (V+31)/32;
    static constexpr int STRIDE = BLOCK_WORDS;
    parallel_foreach_static(0, words, STRIDE, [=](int w_start){
            int w_stop = std::min(w_start+STRIDE, words);
            for (int w = w_start; w < w_stop; w++) {
                set[w] = 0;
            }
            return 0;
        });
    bsg_barrier_hw_tile_group_sync();
    bsg_cuda_print_stat_end(TAG_ZERO_DENSE);
}

static inline int in_dense(int member, int *set)
{
    int idx = member/32;
    int bit = member%32;
    int w = set[idx];
    return w & (1<<bit);
}

static inline void insert_into_dense(int member, int *set, int *set_size)
{
    int idx = member/32;
    int bit = member%32;
    int w = (1<<bit);
    bsg_amoor(&set[idx], w);
    bsg_amoadd(set_size, 1);
}

static inline int *to_sparse(int *set, int *set_sizep, int set_is_dense)
{
    int set_size = *set_sizep;
    __attribute__((section(".dram"))) static int new_set_size = 0;
    serial({
            new_set_size = 0;
            bsg_fence();
        });
    if (set_is_dense) {
        bsg_cuda_print_stat_start(TAG_TO_SPARSE);
        static constexpr int STRIDE = BLOCK_WORDS*32;
        parallel_foreach_static(0, V, STRIDE, [=](int v_start){
                int members[STRIDE];
                int members_size = 0;
                int v_stop = std::min(v_start + STRIDE, V);
                int v;
                for (v = v_start; v+32 <= v_stop; v += 32) {
                    int idx = v/32;
                    int w = set[idx];
                    for (int m = 0; m < 32; m++) {
                        if (w & (1<<m)) {
                            members[members_size++] = v+m;
                        }
                    }
                }
                for (; v < v_stop; v++) {
                    if (in_dense(v, set)) {
                        members[members_size++] = v;
                    }
                }
                int start = bsg_amoadd(&new_set_size, members_size);
                if (start >= set_size){
                    pfor_break;
                } else if (members_size > 0) {
                    for (int m = 0; m < members_size; m++) {
                        g_dense_to_sparse_set[start+m] = members[m];
                    }
                }
                return 0;
            });
        serial (
            {
                // set_size is incremented racily
                // as a result set_size >= the actual size of the set
                // new_set_size is the actual set size
                pr_int_dbg(3000000+set_size);
                pr_int_dbg(4000000+new_set_size);
                *set_sizep = new_set_size;
            }
            );
        bsg_barrier_hw_tile_group_sync();
        bsg_cuda_print_stat_end(TAG_TO_SPARSE);
        return g_dense_to_sparse_set;
    } else {
        bsg_barrier_hw_tile_group_sync();
        return set;
    }
}

static inline int *to_dense(int *set, int set_size, int set_is_dense)
{
    __attribute__((section(".dram"))) static int new_set_size = 0;
    serial({
            new_set_size = 0;
            bsg_fence();
        });
    if (set_is_dense) {
        return set;
    } else {
        bsg_cuda_print_stat_start(TAG_TO_DENSE);
        zero_dense_set(g_sparse_to_dense_set);
        parallel_foreach_static(0, set_size, 1, [=](int m){
                insert_into_dense(set[m], g_sparse_to_dense_set, &new_set_size);
                return 0;
            });
        bsg_barrier_hw_tile_group_sync();
        bsg_cuda_print_stat_end(TAG_TO_DENSE);
        return g_sparse_to_dense_set;
    }
}

extern "C"
int kernel()
{
    // used for load balancing
    static __attribute__((section(".dram"))) int g_dst = 0;
    static __attribute__((section(".dram"))) int g_src = 0;

    bsg_barrier_hw_tile_group_init();
    bsg_cuda_print_stat_kernel_start();
    // initialize
    V = g_V;
    E = g_E;
    fwd_offsets = g_fwd_offsets;
    fwd_nonzeros = g_fwd_nonzeros;
    rev_offsets = g_rev_offsets;
    rev_nonzeros = g_rev_nonzeros;
    distance = g_distance;

    serial({
            // setup distance
            distance[g_root] = 0;
            // setup curr_frontier
            g_curr_frontier[0] = g_root;
            g_curr_frontier_size = 1;
            g_curr_frontier_dense = 0;
            // setup next_frontier
            g_next_frontier_dense = 1;
            g_next_frontier_size = 0;
            pr_dbg("root = %d, distance[%d]=%d, curr_fontier[0]=%d, curr_frontier_size=%d\n"
                   ,g_root
                   ,g_root
                   ,distance[g_root]
                   ,g_curr_frontier[0]
                   ,g_curr_frontier_size);
            bsg_fence();
        });
    int d = 0;
    while (g_curr_frontier_size != 0) {
        d++;
        /////////////////////////
        // determine direction //
        /////////////////////////
        int *sparse_frontier = to_sparse(g_curr_frontier, &g_curr_frontier_size, g_curr_frontier_dense);
        serial({
                pr_int_dbg(g_curr_frontier_size);
                pr_dbg("curr_frontier_size = %d\n", g_curr_frontier_size);
                // set g_mf/g_mu
                g_mf = 0;
                g_mu = 0;
                // set dynamic loop counters
                g_src = g_dst = 0;
                bsg_fence();
            });
        bsg_cuda_print_stat_start(TAG_DECIDE_DIRECTION);
        if (g_rev_not_fwd == 0) {
            // determine if we should switch from forward to backward
            // 1. find sum (degree in frontier)
            int mf_local = 0;
            parallel_foreach_static(0, g_curr_frontier_size, 1, [&](int m) mutable {
                    pr_vdbg("m = %d, sparse_frontier[%d]=%d\n", m, m, sparse_frontier[m]);
                    pr_vdbg("fwd_offsets[%d+1]=%d, fwd_offsets[%d]=%d\n",
                            sparse_frontier[m],
                            fwd_offsets[sparse_frontier[m]+1],
                            sparse_frontier[m],
                            fwd_offsets[sparse_frontier[m]]
                        );
                    mf_local += fwd_offsets[sparse_frontier[m]+1]-fwd_offsets[sparse_frontier[m]];
                    return 0;
                });
            bsg_amoadd(&g_mf, mf_local);
            // 2. find sum (degree unvisited)
            int mu_local = 0;
            parallel_foreach_static(0, V, BLOCK_WORDS, [&](int v_start) mutable {
                    int v_stop = std::min(v_start+BLOCK_WORDS, V);
                    int v = v_start;
#ifndef OPT_MU_ILP_INNER
#define OPT_MU_ILP_INNER 2
#endif
#if (OPT_MU_ILP_INNER > 1)
                    constexpr int ILP = OPT_MU_ILP_INNER;
                    for (; v + ILP <= v_stop; v += ILP) {
                        register int l_distance[ILP];
                        bsg_unroll(32)
                        for (int ilp = 0; ilp < ILP; ilp++) {
                            l_distance[ilp] = distance[v+ilp];
                        }
                        bsg_compiler_memory_barrier();
                        // add an if-statement to check if we should do this...
                        // this may results in unneccesary loads in later iterations
                        register int l_fwd_offsets[ILP+1];
                        bsg_unroll(32)
                        for (int ilp = 0; ilp < ILP+1; ilp++) {
                            l_fwd_offsets[ilp] = fwd_offsets[v+ilp];
                        }
                        bsg_compiler_memory_barrier();
                        bsg_unroll(32)
                        for (int ilp = 0; ilp < ILP; ilp++) {
                            if (l_distance[ilp] == -1) {
                                mu_local += l_fwd_offsets[ilp+1]-l_fwd_offsets[ilp];
                            }
                        }
                    }
#endif
                    for (; v < v_stop; v++) {
                        pr_vdbg("distance[%d]=%d\n", v, distance[v]);
                        if (distance[v] == -1) {
                            pr_vdbg("%d: start = %d, stop = %d\n"
                                    v, fwd_offsets[v], fwd_offsets[v+1]);
                            mu_local += fwd_offsets[v+1]-fwd_offsets[v];
                        }
                    }
                    return 0;
                });
            bsg_amoadd(&g_mu, mu_local);
            // 3. compare
            serial(
                {
                    g_rev_not_fwd = (g_mf > (g_mu/20));
                    pr_dbg("mf = %d, mu = %d, rev_not_fwd = %d\n", g_mf, g_mu, g_rev_not_fwd);
                    bsg_fence();
                }
                );
        } else {
            // determine if we should switch back from backward => forward
            serial(
                {
                    g_rev_not_fwd = (g_curr_frontier_size >= V/20);
                    pr_dbg("g_curr_frontier_size = %d, V=%d, V/20=%d, rev_not_fwd = %d\n",
                           g_curr_frontier_size,
                           V,
                           V/20,
                           g_rev_not_fwd);
                    bsg_fence();
                }
                );
        }
        bsg_cuda_print_stat_end(TAG_DECIDE_DIRECTION);
        ////////////////////////////////////
        // traverse from current frontier //
        // update distance                //
        // build next frontier            //
        ////////////////////////////////////
        if (g_rev_not_fwd) {
            // backwards traversal
            bsg_cuda_print_stat_start(TAG_REV_TRAVERSAL);
            int *dense_frontier = to_dense(g_curr_frontier,
                                           g_curr_frontier_size,
                                           g_curr_frontier_dense);
            parallel_foreach_dynamic(g_dst, V, BLOCK_WORDS, [=](int dst_start){
                    int dst_stop = dst_start + BLOCK_WORDS;
                    dst_stop = std::min(dst_stop, V);
                    int dst = dst_start;
#ifndef OPT_REV_PRE_OUTER
#define OPT_REV_PRE_OUTER 1
#endif
#if     (OPT_REV_PRE_OUTER > 4)
#error "OPT_REV_PRE_OUTER must be <= 2"
#endif
#if    (OPT_REV_PRE_OUTER > 1)
                    constexpr int PRE = OPT_REV_PRE_OUTER;
                    for (; dst + PRE <= dst_stop; dst += PRE) {
                        int l_distance[PRE];
                        int l_rev_offsets[PRE+1];
#if    (OPT_REV_PRE_OUTER >= 2)
                        int d0, d1, rvo0, rvo1, rvo2;
                        d0 = distance[dst+0];
                        d1 = distance[dst+1];
                        rvo0 = rev_offsets[dst+0];
                        rvo1 = rev_offsets[dst+1];
                        rvo2 = rev_offsets[dst+2];
#endif
#if    (OPT_REV_PRE_OUTER >= 3)
                        int d2, rvo3;
                        d2 = distance[dst+2];
                        rvo3 = rev_offsets[dst+3];
#endif
#if    (OPT_REV_PRE_OUTER >= 4)
                        int d3, rvo4;
                        d3 = distance[dst+3];
                        rvo4 = rev_offsets[dst+4];
#endif
                        bsg_compiler_memory_barrier();                        
#if    (OPT_REV_PRE_OUTER >= 2)
                        l_distance[0]=d0;
                        l_distance[1]=d1;
                        l_rev_offsets[0]=rvo0;
                        l_rev_offsets[1]=rvo1;
                        l_rev_offsets[2]=rvo2;
#endif
#if    (OPT_REV_PRE_OUTER >= 3)
                        l_distance[2]=d2;
                        l_rev_offsets[3]=rvo3;
#endif
#if    (OPT_REV_PRE_OUTER >= 4)
                        l_distance[3]=d3;
                        l_rev_offsets[4]=rvo4;
#endif                        
                        bsg_compiler_memory_barrier();
                        for (int pre = 0; pre < PRE; pre++) {
                            if (l_distance[pre] == -1) {
                                int nz_start = l_rev_offsets[pre];
                                int nz_stop  = l_rev_offsets[pre+1];
                                for (int nz = nz_start; nz < nz_stop; nz++) {
                                    int src = rev_nonzeros[nz];
                                    if (in_dense(src, dense_frontier)) {
                                        distance[dst+pre] = d;
                                        insert_into_dense(dst+pre, g_next_frontier, &g_next_frontier_size);
                                        break;
                                    }
                                }
                            }
                        }
                    }
#endif
                    for (; dst < dst_stop; dst++) {
                        // skip if visited already
                        if (distance[dst] == -1) {
                            int nz_start = rev_offsets[dst];
                            int nz_stop  = rev_offsets[dst+1];
                            for (int nz = nz_start; nz < nz_stop; nz++) {
                                int src = rev_nonzeros[nz];
                                pr_int_dbg(2000000+src);
                                if (in_dense(src, dense_frontier)) {
                                    distance[dst] = d;
                                    pr_vdbg("d= %d, found %d from %d\n", d, dst, src);
                                    pr_int_dbg(1000000+dst);
                                    insert_into_dense(dst, g_next_frontier, &g_next_frontier_size);
                                    break;
                                }
                            }
                        }
                    }
                    return 0;
                });
            bsg_cuda_print_stat_end(TAG_REV_TRAVERSAL);
        } else {
            bsg_cuda_print_stat_start(TAG_FWD_TRAVERSAL);
            // forward traversal
            parallel_foreach_dynamic(g_src, g_curr_frontier_size, 1, [=](int v){
                    int src = sparse_frontier[v];
                    pr_int_dbg(2000000+src);
                    int nz_start = fwd_offsets[src];
                    int nz_stop  = fwd_offsets[src+1];
                    int nz = nz_start;
#ifndef OPT_FWD_ILP_INNER
#define OPT_FWD_ILP_INNER 1
#endif
#if (OPT_FWD_ILP_INNER > 1)
                    constexpr int ILP = OPT_FWD_ILP_INNER;
                    for (; nz + ILP <= nz_stop; nz += ILP) {
                        register int dst[ILP];
                        bsg_unroll(32)
                        for (int ilp = 0; ilp < ILP; ilp++) {
                            dst[ilp] = fwd_nonzeros[nz+ilp];
                        }
                        bsg_compiler_memory_barrier();
                        register int l_distance[ILP];
                        bsg_unroll(32)
                        for (int ilp = 0; ilp < ILP; ilp++) {
                            l_distance[ilp] = distance[dst[ilp]];
                        }
                        bsg_compiler_memory_barrier();
                        bsg_unroll(32)
                        for (int ilp = 0; ilp < ILP; ilp++) {
                            if (l_distance[ilp] == -1) {
                                distance[dst[ilp]] = d;
                                insert_into_dense(dst[ilp], g_next_frontier, &g_next_frontier_size);
                            }
                        }
                    }
#endif
                    for (; nz < nz_stop; nz++) {
                        int dst = fwd_nonzeros[nz];
                        if (distance[dst] == -1) {
                            distance[dst] = d;
                            pr_vdbg("d= %d, found %d from %d\n", d, dst, src);
                            pr_int_dbg(1000000+dst);
                            insert_into_dense(dst, g_next_frontier, &g_next_frontier_size);
                        }
                    }
                    return  0;
                });
            bsg_cuda_print_stat_end(TAG_REV_TRAVERSAL);
        }
        ////////////////////
        // swap frontiers //
        ////////////////////
        serial(
            {
                // curr = next
                int *tmp = g_curr_frontier;
                g_curr_frontier = g_next_frontier;
                g_curr_frontier_size = g_next_frontier_size;
                g_curr_frontier_dense = g_next_frontier_dense;
                // set next
                g_next_frontier = tmp;
                g_next_frontier_size = 0;
                g_next_frontier_dense = 1;
                // fence
                bsg_fence();
            }
            );
        // zero out next frontier
        zero_dense_set(g_next_frontier);
    }
    bsg_cuda_print_stat_kernel_end();
    bsg_barrier_hw_tile_group_sync();
    return 0;
}
