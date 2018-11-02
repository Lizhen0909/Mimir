//
// (c) 2016 by University of Delaware, Argonne National Laboratory, San Diego
//     Supercomputer Center, National University of Defense Technology,
//     National Supercomputer Center in Guangzhou, and Sun Yat-sen University.
//
//     See COPYRIGHT in top-level directory.
//

#include <string.h>
#include <sys/stat.h>
#include <mpi.h>
#include "container.h"

using namespace MIMIR_NS;

uint64_t Container::mem_bytes = 0;
