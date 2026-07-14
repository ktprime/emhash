// Strict comparison: emilib2 (original) vs emilib2_v2 (optimized)
// Run each test 20 times, discard top/bottom 5, average middle 10
// Compile: g++ -std=c++17 -O3 -march=native -I./include bench/bench_emihmap2_v2_compare.cpp -o bench_v2

#include "emilib/emihmap2.hpp"
#include "emilib/emihmap2_v2.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>

using namespace std;

// Robust timing: run N times, trim outliers, return mean of middle samples
template <typename Setup, typename Body>
double bench(int runs, int trim, Setup setup, Body body) {
    vector<double> times;
    times.reserve(runs);
    for (int i = 0; i < runs; i++) {
        setup();
        auto start = chrono::high_resolution_clock::now();
        body();
        auto end = chrono::high_resolution_clock::now();
        times.push_back(chrono::duration<double, chrono::nanoseconds::period>(end - start).count());
    }
    sort(times.begin(), times.end());
    double sum = 0;
    int count = 0;
    for (int i = trim; i < runs - trim; i++) { sum += times[i]; count++; }
    return sum / count;
}

struct Result {
    string name;
    double v1;
    double v2;
    Result(const string& n, double a, double b) : name(n), v1(a), v2(b) {}
    double delta_pct() const { return (v2 - v1) / v1 * 100; }
};

vector<Result> g_results;

void print_header(const string& title) {
    cout << "\n--- " << title << " ---\n";
    cout << left << setw(28) << "Operation" << right
         << setw(12) << "emilib2" << setw(12) << "v2"
         << setw(10) << "delta" << "\n";
    cout << string(62, '-') << "\n";
}

void print_row(const string& name, double v1, double v2, const string& unit) {
    cout << left << setw(28) << name << right
         << fixed << setprecision(3)
         << setw(9) << v1 << unit
         << setw(9) << v2 << unit
         << setw(8) << showpos << ((v2 - v1) / v1 * 100) << "%" << noshowpos << "\n";
    g_results.push_back(Result(name, v1, v2));
}

int main() {
    const int N = 200000;
    const int RUNS = 20;
    const int TRIM = 5;
    mt19937_64 rng(42);

    // Prepare key sets
    vector<int> seq_keys(N);
    for (int i = 0; i < N; i++) seq_keys[i] = i;
    vector<int> rand_keys(N);
    for (int i = 0; i < N; i++) rand_keys[i] = rng() % (N * 10);
    vector<int> miss_keys(N);
    for (int i = 0; i < N; i++) miss_keys[i] = rng() + N * 100;

    cout << "========================================================\n";
    cout << "  emilib2 vs emilib2_v2 (N=" << N << ", runs=" << RUNS << ", trim=" << TRIM << ")\n";
    cout << "========================================================\n";

    // ---------- Insert (sequential) ----------
    print_header("Insert (sequential keys)");
    {
        emilib2::HashMap<int, int> m;
        double t = bench(RUNS, TRIM,
            [&]() { m.clear(); m.reserve(N); },
            [&]() { for (int k : seq_keys) m[k] = k; });
        print_row("insert_seq", t / N, t / N, "ns");
    }
    {
        emilib2_v2::HashMap<int, int> m;
        double t = bench(RUNS, TRIM,
            [&]() { m.clear(); m.reserve(N); },
            [&]() { for (int k : seq_keys) m[k] = k; });
        // reuse v1 from g_results
        g_results.back().v2 = t / N;
        cout << "\r" << left << setw(28) << "insert_seq" << right
             << fixed << setprecision(3)
             << setw(9) << g_results.back().v1 << "ns"
             << setw(9) << g_results.back().v2 << "ns"
             << setw(8) << showpos << (g_results.back().delta_pct()) << "%" << noshowpos << "\n";
    }

    // ---------- Insert (random) ----------
    print_header("Insert (random keys)");
    double ins_r_v1, ins_r_v2;
    {
        emilib2::HashMap<int, int> m;
        double t = bench(RUNS, TRIM,
            [&]() { m.clear(); m.reserve(N); },
            [&]() { for (int k : rand_keys) m[k] = k; });
        ins_r_v1 = t / N;
    }
    {
        emilib2_v2::HashMap<int, int> m;
        double t = bench(RUNS, TRIM,
            [&]() { m.clear(); m.reserve(N); },
            [&]() { for (int k : rand_keys) m[k] = k; });
        ins_r_v2 = t / N;
    }
    print_row("insert_rand", ins_r_v1, ins_r_v2, "ns");

    // ---------- Lookup hit ----------
    print_header("Lookup (hit, 5N lookups)");
    emilib2::HashMap<int, int> m1;
    m1.reserve(N);
    for (int k : rand_keys) m1[k] = k;
    emilib2_v2::HashMap<int, int> m2;
    m2.reserve(N);
    for (int k : rand_keys) m2[k] = k;

    vector<int> hit_lookup(N * 5);
    for (int i = 0; i < hit_lookup.size(); i++) hit_lookup[i] = rand_keys[rng() % N];

    double lk_v1, lk_v2;
    {
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (int k : hit_lookup) { auto it = m1.find(k); if (it != m1.end()) sum += it->second; }
        });
        lk_v1 = t / (N * 5);
    }
    {
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (int k : hit_lookup) { auto it = m2.find(k); if (it != m2.end()) sum += it->second; }
        });
        lk_v2 = t / (N * 5);
    }
    print_row("lookup_hit", lk_v1, lk_v2, "ns");

    // ---------- Lookup miss ----------
    print_header("Lookup (miss, 5N lookups)");
    vector<int> miss_lookup(N * 5);
    for (int i = 0; i < miss_lookup.size(); i++) miss_lookup[i] = rng() + N * 1000;

    double lkm_v1, lkm_v2;
    {
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (int k : miss_lookup) { auto it = m1.find(k); if (it != m1.end()) sum += it->second; }
        });
        lkm_v1 = t / (N * 5);
    }
    {
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (int k : miss_lookup) { auto it = m2.find(k); if (it != m2.end()) sum += it->second; }
        });
        lkm_v2 = t / (N * 5);
    }
    print_row("lookup_miss", lkm_v1, lkm_v2, "ns");

    // ---------- Erase (sequential) ----------
    print_header("Erase (sequential, full)");
    double er_v1, er_v2;
    {
        double t = bench(RUNS, TRIM,
            [&]() { m1.clear(); m1.reserve(N); for (int k : rand_keys) m1[k] = k; },
            [&]() { for (int k : rand_keys) m1.erase(k); });
        er_v1 = t / N;
    }
    {
        double t = bench(RUNS, TRIM,
            [&]() { m2.clear(); m2.reserve(N); for (int k : rand_keys) m2[k] = k; },
            [&]() { for (int k : rand_keys) m2.erase(k); });
        er_v2 = t / N;
    }
    print_row("erase_seq", er_v1, er_v2, "ns");

    // ---------- Erase random 50% ----------
    print_header("Erase (random 50%)");
    vector<int> erase_half(rand_keys.begin(), rand_keys.begin() + N / 2);
    shuffle(erase_half.begin(), erase_half.end(), rng);

    double er_r_v1, er_r_v2;
    {
        double t = bench(RUNS, TRIM,
            [&]() { m1.clear(); m1.reserve(N); for (int k : rand_keys) m1[k] = k; },
            [&]() { for (int k : erase_half) m1.erase(k); });
        er_r_v1 = t / (N / 2);
    }
    {
        double t = bench(RUNS, TRIM,
            [&]() { m2.clear(); m2.reserve(N); for (int k : rand_keys) m2[k] = k; },
            [&]() { for (int k : erase_half) m2.erase(k); });
        er_r_v2 = t / (N / 2);
    }
    print_row("erase_rand50", er_r_v1, er_r_v2, "ns");

    // ---------- Lookup after erase ----------
    print_header("Lookup after 50% erase (remaining keys)");
    double lk_post_v1, lk_post_v2;
    vector<int> remaining_keys(rand_keys.begin() + N / 2, rand_keys.end());
    remaining_keys.resize(remaining_keys.size() * 5);
    for (size_t i = remaining_keys.size() / 2; i < remaining_keys.size(); i++)
        remaining_keys[i] = rng() + N * 1000;

    {
        m1.clear(); m1.reserve(N);
        for (int k : rand_keys) m1[k] = k;
        for (int k : erase_half) m1.erase(k);
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (int k : remaining_keys) { auto it = m1.find(k); if (it != m1.end()) sum += it->second; }
        });
        lk_post_v1 = t / remaining_keys.size();
    }
    {
        m2.clear(); m2.reserve(N);
        for (int k : rand_keys) m2[k] = k;
        for (int k : erase_half) m2.erase(k);
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (int k : remaining_keys) { auto it = m2.find(k); if (it != m2.end()) sum += it->second; }
        });
        lk_post_v2 = t / remaining_keys.size();
    }
    print_row("lookup_post_erase", lk_post_v1, lk_post_v2, "ns");

    // ---------- Iteration ----------
    print_header("Iteration (100 passes)");
    double it_v1, it_v2;
    {
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (auto& [k, v] : m1) (void)sum, (void)k, (void)v;
        });
        it_v1 = t / N;
    }
    {
        double t = bench(RUNS, TRIM, [](){}, [&]() {
            volatile int sum = 0;
            for (auto& [k, v] : m2) (void)sum, (void)k, (void)v;
        });
        it_v2 = t / N;
    }
    print_row("iterate", it_v1, it_v2, "ns");

    // ---------- Summary ----------
    cout << "\n========================================================\n";
    cout << "  Summary (negative delta = v2 faster)\n";
    cout << "========================================================\n";
    cout << left << setw(20) << "Operation" << right
         << setw(12) << "emilib2" << setw(12) << "v2"
         << setw(10) << "delta" << "\n";
    cout << string(54, '-') << "\n";
    for (auto& r : g_results) {
        // rebuild from stored v1/v2 (the first insert_seq has v2 already set)
    }
    // Re-print clean summary from collected raw values
    cout << left << setw(20) << "insert_seq" << right << fixed << setprecision(3)
         << setw(9) << g_results[0].v1 << "ns" << setw(9) << g_results[0].v2 << "ns"
         << setw(8) << showpos << g_results[0].delta_pct() << "%" << noshowpos << "\n";
    cout << left << setw(20) << "insert_rand" << right
         << setw(9) << ins_r_v1 << "ns" << setw(9) << ins_r_v2 << "ns"
         << setw(8) << showpos << (ins_r_v2 - ins_r_v1) / ins_r_v1 * 100 << "%" << noshowpos << "\n";
    cout << left << setw(20) << "lookup_hit" << right
         << setw(9) << lk_v1 << "ns" << setw(9) << lk_v2 << "ns"
         << setw(8) << showpos << (lk_v2 - lk_v1) / lk_v1 * 100 << "%" << noshowpos << "\n";
    cout << left << setw(20) << "lookup_miss" << right
         << setw(9) << lkm_v1 << "ns" << setw(9) << lkm_v2 << "ns"
         << setw(8) << showpos << (lkm_v2 - lkm_v1) / lkm_v1 * 100 << "%" << noshowpos << "\n";
    cout << left << setw(20) << "erase_seq" << right
         << setw(9) << er_v1 << "ns" << setw(9) << er_v2 << "ns"
         << setw(8) << showpos << (er_v2 - er_v1) / er_v1 * 100 << "%" << noshowpos << "\n";
    cout << left << setw(20) << "erase_rand50" << right
         << setw(9) << er_r_v1 << "ns" << setw(9) << er_r_v2 << "ns"
         << setw(8) << showpos << (er_r_v2 - er_r_v1) / er_r_v1 * 100 << "%" << noshowpos << "\n";
    cout << left << setw(20) << "lookup_post_erase" << right
         << setw(9) << lk_post_v1 << "ns" << setw(9) << lk_post_v2 << "ns"
         << setw(8) << showpos << (lk_post_v2 - lk_post_v1) / lk_post_v1 * 100 << "%" << noshowpos << "\n";
    cout << left << setw(20) << "iterate" << right
         << setw(9) << it_v1 << "ns" << setw(9) << it_v2 << "ns"
         << setw(8) << showpos << (it_v2 - it_v1) / it_v1 * 100 << "%" << noshowpos << "\n";

    return 0;
}