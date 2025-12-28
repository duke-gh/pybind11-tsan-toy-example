#include <pybind11/embed.h>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

namespace py = pybind11;

static long g_racy_counter = 0;  // intentionally NOT atomic

static void DoThreadWork(int tid, int iters, std::atomic<long>& total) {
    for (int i = 0; i < iters; ++i) {
        

        py::gil_scoped_acquire gil;
        //   std::this_thread::yield();
        g_racy_counter++;

        //continue;

        // Simple Python work: use numpy if available, else fallback to pure python.
        try {
            auto np = py::module_::import("numpy");
            // Create a small array and sum it
            auto arr = np.attr("arange")(1000);
            long s = arr.attr("sum")().cast<long>();
            //total.fetch_add(s, std::memory_order_relaxed);
        } catch (const py::error_already_set&) {
            // Fallback: pure python sum(range(1000))
            auto builtins = py::module_::import("builtins");
            long s = builtins.attr("sum")(builtins.attr("range")(1000)).cast<long>();
            total.fetch_add(s, std::memory_order_relaxed);
        }
    }
}

int main(int argc, char** argv) {
    int num_threads = 8;
    int iters = 100;

    if (argc >= 2) num_threads = std::stoi(argv[1]);
    if (argc >= 3) iters = std::stoi(argv[2]);

    // Start interpreter (one per process).
    py::scoped_interpreter guard{};

    // Pre-import heavy shared deps single-threaded to avoid import deadlock patterns.
    {
        py::gil_scoped_acquire gil;
        try {
            py::module_::import("numpy");
            std::cout << "Pre-imported numpy\n";
        } catch (const py::error_already_set& e) {
            std::cout << "Numpy not available (ok): " << e.what() << "\n";
        }
    }

    // Release GIL in main while workers run.
    py::gil_scoped_release release;

    std::atomic<long> total{0};
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, iters, &total]() { DoThreadWork(t, iters, total); });
    }

    for (auto& th : threads) th.join();

    std::cout << "g_racy_counter=" << g_racy_counter << "\n";

    std::cout << "Done. total=" << total.load() << "\n";
    return 0;
} 