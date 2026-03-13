// Moved this to kernel body to avoid stack loading/off-loading.
/*
#include "spmm.hpp"
#include "spmm_barrier.hpp"

void spmm_init(sparse_matrix_partition_t *__restrict__ A_ptr, // csr
               sparse_matrix_partition_t *__restrict__ B_ptr, // csr
               sparse_matrix_partition_t *__restrict__ C_ptr, // csr
               std::atomic<intptr_t> *mem_pool_arg) // mem pool
{
}
*/
