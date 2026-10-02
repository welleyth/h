#include "../eolymp.h"
#include "../eolymp-shapes.h"

#include <cfenv>
#include <chrono>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <csignal>
#if defined(_WIN32)
#include "windows/posix.h"
#else
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "harness.h"

#include "core.inc"
#include "fmt.inc"
#include "parse.inc"
#include "io.inc"
#include "pattern.inc"
#include "diag.inc"
#include "validate.inc"
#include "check.inc"
#include "interact.inc"
#include "phases.inc"
#include "generate.inc"
#include "control.inc"
#include "shapes.inc"
#include "shapes_trees.inc"
#include "shapes_graphs.inc"
#include "shapes_weighted.inc"
#include "shapes_grids.inc"
#include "shapes_permutations.inc"
#include "shapes_sequences.inc"
#include "shapes_intervals.inc"
#include "shapes_queries.inc"
#include "boundaries.inc"
#include "pinned.inc"

int main() { return eot::main_of_tests(); }
