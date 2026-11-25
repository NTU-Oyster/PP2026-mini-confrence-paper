// Thread team. OpenMP by default; std::thread under ThreadSanitizer, because
// libgomp is not instrumented and its barriers would be reported as races.
#pragma once
#include <vector>
#ifdef PM_STD_THREAD
#include <thread>
#else
#include <omp.h>
#endif

namespace pm {

template <class F>
void run_team(int p, F&& f) {
    if (p == 1) {
        f(0);
        return;
    }
#ifdef PM_STD_THREAD
    std::vector<std::thread> ts;
    for (int t = 1; t < p; ++t) ts.emplace_back([&f, t] { f(t); });
    f(0);
    for (auto& t : ts) t.join();
#else
#pragma omp parallel num_threads(p)
    f(omp_get_thread_num());
#endif
}

}  // namespace pm
