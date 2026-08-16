#include "minidb/core/Database.h"
#include "minidb/concurrency/LockManager.h"
#include "minidb/concurrency/TransactionManager.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>


namespace minidb
{

    // Files used by the benchmark. They are removed before every run so that
    // measurements always start from an empty database.
    static const std::string BENCH_DB_FILE = "minidb_bench.db";

    static void remove_bench_files()
    {
        std::remove(BENCH_DB_FILE.c_str());
        std::remove((BENCH_DB_FILE + ".log").c_str());
    }

    // Builds the same key layout the SQL executor uses: "<table>:<id>"
    static std::string make_key(int id)
    {
        return "bench:" + std::to_string(id);
    }

    static std::string make_row(int id)
    {
        return std::to_string(id) + ",user_" + std::to_string(id) + ",42";
    }

    // Prints one result line: total time, throughput and average latency
    static void report(const std::string& name, size_t ops, double seconds)
    {
        double ops_per_sec = (seconds > 0.0) ? static_cast<double>(ops) / seconds : 0.0;
        double us_per_op = (ops > 0) ? (seconds * 1e6) / static_cast<double>(ops) : 0.0;

        std::cout << "  " << std::left << std::setw(34) << name
                  << std::right
                  << std::setw(9) << ops << " ops"
                  << std::setw(10) << std::fixed << std::setprecision(2) << (seconds * 1000.0) << " ms"
                  << std::setw(14) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/s"
                  << std::setw(12) << std::fixed << std::setprecision(1) << us_per_op << " us/op"
                  << "\n";
    }

    // Small RAII stopwatch around std::chrono::steady_clock
    class Timer
    {
    public:
        Timer()
            : start_(std::chrono::steady_clock::now())
        {
        }

        double elapsed_seconds() const
        {
            std::chrono::duration<double> diff = std::chrono::steady_clock::now() - start_;
            return diff.count();
        }

    private:
        std::chrono::steady_clock::time_point start_;
    };


    // --- Workload 1: logged writes ---------------------------------------------
    // Every Database::set() appends to the WAL and flushes it before touching the
    // B+ tree. Note the flush is not a physical fsync, so this does not include
    // disk sync latency.
    static void bench_insert(Database& db, int num_ops)
    {
        Timer timer;
        for (int i = 0; i < num_ops; ++i)
        {
            db.set(make_key(i), make_row(i));
        }
        report("INSERT (WAL append + flush)", num_ops, timer.elapsed_seconds());
    }

    // --- Workload 2: point lookups --------------------------------------------
    // No transaction: a plain B+ tree descent plus buffer pool lookup.
    static void bench_point_get(Database& db, int num_ops)
    {
        size_t found = 0;
        Timer timer;
        for (int i = 0; i < num_ops; ++i)
        {
            if (db.get(make_key(i)))
            {
                ++found;
            }
        }
        double seconds = timer.elapsed_seconds();
        report("GET point lookup (no txn)", num_ops, seconds);

        if (found != static_cast<size_t>(num_ops))
        {
            std::cout << "  !! expected " << num_ops << " hits, got " << found << "\n";
        }
    }

    // --- Workload 3: point lookups through MVCC -------------------------------
    // Same lookups, but each one also walks the version chain against a ReadView.
    static void bench_point_get_mvcc(Database& db, TransactionManager& txn_mgr, int num_ops)
    {
        auto txn = txn_mgr.begin();

        Timer timer;
        for (int i = 0; i < num_ops; ++i)
        {
            db.get(make_key(i), txn.get());
        }
        double seconds = timer.elapsed_seconds();

        txn_mgr.commit(txn.get());
        report("GET point lookup (MVCC txn)", num_ops, seconds);
    }

    // --- Workload 4: updates ---------------------------------------------------
    // Overwriting an existing key pushes the previous version into the undo log,
    // so this also measures version-chain growth.
    static void bench_update(Database& db, TransactionManager& txn_mgr, int num_ops)
    {
        auto txn = txn_mgr.begin();

        Timer timer;
        for (int i = 0; i < num_ops; ++i)
        {
            db.set(make_key(i), make_row(i) + ",updated", txn.get());
        }
        double seconds = timer.elapsed_seconds();

        txn_mgr.commit(txn.get());
        report("UPDATE in txn (version chain)", num_ops, seconds);
    }

    // --- Workload 5: full scan -------------------------------------------------
    // Walks every leaf through the B+ tree sibling pointers. Reported per row.
    static void bench_scan(Database& db, int num_rows)
    {
        Timer timer;
        auto rows = db.scan();
        double seconds = timer.elapsed_seconds();

        report("SCAN full table", rows.size(), seconds);

        if (rows.size() != static_cast<size_t>(num_rows))
        {
            std::cout << "  !! expected " << num_rows << " rows, got " << rows.size() << "\n";
        }
    }

    // --- Workload 6: rollback --------------------------------------------------
    // Inserts inside a transaction and then aborts, undoing every record.
    static void bench_rollback(Database& db, TransactionManager& txn_mgr, int num_ops)
    {
        auto txn = txn_mgr.begin();
        for (int i = 0; i < num_ops; ++i)
        {
            db.set("rollback:" + std::to_string(i), make_row(i), txn.get());
        }

        Timer timer;
        txn_mgr.abort(txn.get(), &db);
        double seconds = timer.elapsed_seconds();

        report("ROLLBACK (undo per record)", num_ops, seconds);
    }

    // Runs one workload and keeps going if it hits a structural limit of the
    // engine, so a single unsupported phase does not abort the whole report.
    template <typename Workload>
    static void run_phase(const std::string& name, Workload workload)
    {
        try
        {
            workload();
        }
        catch (const std::exception& e)
        {
            std::cout << "  " << std::left << std::setw(34) << name
                      << "  SKIPPED: " << e.what() << "\n";
        }
    }

    static void run_all(int num_ops)
    {
        remove_bench_files();

        std::cout << "MiniDB benchmark\n";
        std::cout << "  database file : " << BENCH_DB_FILE << "\n";
        std::cout << "  operations    : " << num_ops << "\n";
        std::cout << "  page size     : " << PAGE_SIZE << " bytes\n\n";

        {
            Database db(BENCH_DB_FILE);
            LockManager lock_mgr;
            TransactionManager txn_mgr;

            db.set_lock_manager(&lock_mgr);
            txn_mgr.set_lock_manager(&lock_mgr);
            db.set_transaction_manager(&txn_mgr);

            run_phase("INSERT",   [&] { bench_insert(db, num_ops); });
            run_phase("GET",      [&] { bench_point_get(db, num_ops); });
            run_phase("GET MVCC", [&] { bench_point_get_mvcc(db, txn_mgr, num_ops); });
            run_phase("UPDATE",   [&] { bench_update(db, txn_mgr, num_ops); });
            run_phase("SCAN",     [&] { bench_scan(db, num_ops); });
            run_phase("ROLLBACK", [&] { bench_rollback(db, txn_mgr, num_ops); });

            // Drop the versions the workloads above accumulated
            Timer timer;
            txn_mgr.vacuum();
            report("VACUUM (undo log cleanup)", 1, timer.elapsed_seconds());
        }

        remove_bench_files();
        std::cout << "\nDone.\n";
    }

} // namespace minidb


int main(int argc, char** argv)
{
    // Kept below the point where a single internal node runs out of room,
    // see the "Known limitations" section of the README.
    int num_ops = 2000;

    if (argc > 1)
    {
        try
        {
            num_ops = std::stoi(argv[1]);
        }
        catch (const std::exception&)
        {
            std::cerr << "Usage: minidb_bench [num_ops]\n";
            return 1;
        }
    }

    if (num_ops <= 0)
    {
        std::cerr << "Error: num_ops must be positive\n";
        return 1;
    }

    try
    {
        minidb::run_all(num_ops);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Benchmark failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
