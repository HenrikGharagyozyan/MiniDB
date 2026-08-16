# MiniDB

A small relational database engine written from scratch in C++20 — no storage library, no SQL library, no third-party runtime dependencies. Data lives in 4 KB pages managed by a buffer pool, is indexed by a B+ tree, is logged ahead of every page write for crash recovery, and is queried through a hand-written SQL tokenizer, parser and executor. Transactions are isolated with MVCC snapshots and strict two-phase locking.

It is a learning project, built one subsystem at a time. It is not production software — see [Known limitations](#known-limitations), which is deliberately detailed.

```
db> CREATE TABLE users (id INT, name VARCHAR);
Table 'users' created successfully.
db> INSERT INTO users VALUES (1, 'Henrik');
1 row inserted into 'users'.
db> INSERT INTO users VALUES (2, 'Alex');
1 row inserted into 'users'.
db> BEGIN;
Transaction 0 started.
db (txn:0)> DELETE FROM users WHERE id = 2;
1 row(s) deleted from 'users'.
db (txn:0)> ROLLBACK;
Transaction rolled back.
db> SELECT * FROM users;
id | name
---------------------
1,Henrik
2,Alex
```

## Contents

- [Building](#building)
- [Using the CLI](#using-the-cli)
- [Architecture](#architecture)
- [How a query travels through the system](#how-a-query-travels-through-the-system)
- [Storage format](#storage-format)
- [Transactions](#transactions)
- [Benchmarks](#benchmarks)
- [Tests](#tests)
- [Known limitations](#known-limitations)
- [License](#license)

## Building

Requirements: a C++20 compiler, CMake 3.20+. GoogleTest is fetched automatically by CMake, so the first configure needs network access.

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

This produces three targets:

| Target | Path | What it is |
| --- | --- | --- |
| `minidb_core` | `build/libminidb_core.a` | The engine, as a static library |
| `minidb_cli` | `build/minidb_cli` | Interactive REPL |
| `minidb_bench` | `build/minidb_bench` | Benchmark runner |

The build is compiled with `-Wall -Wextra -Wpedantic -Werror`.

To skip the optional targets:

```bash
cmake -S . -B build -DBUILD_TESTING=OFF -DBUILD_BENCHMARKS=OFF
```

## Using the CLI

```bash
./build/minidb_cli
```

The database file is `database.db` in the current working directory, with the write-ahead log alongside it as `database.db.log`. Both are created on first use.

### SQL

| Statement | Example |
| --- | --- |
| `CREATE TABLE` | `CREATE TABLE users (id INT, name VARCHAR);` |
| `INSERT INTO` | `INSERT INTO users VALUES (1, 'Henrik');` |
| `SELECT` | `SELECT * FROM users;` |
| `SELECT` with filter | `SELECT * FROM users WHERE age >= 18;` |
| `UPDATE` | `UPDATE users SET name = 'Alex' WHERE id = 2;` |
| `DELETE` | `DELETE FROM users WHERE age < 18;` |

Column types are `INT` and `VARCHAR`. String literals use single quotes. Comparison operators are `=`, `!=`, `>`, `<`, `>=`, `<=`; `INT` columns are compared numerically, `VARCHAR` columns lexicographically. Keywords are case-insensitive. Every statement ends with `;`.

### Key-value commands

The engine underneath is a key-value store, and it is reachable directly:

| Command | Meaning |
| --- | --- |
| `SET <key> <value>` | Write a key |
| `GET <key>` | Read a key |
| `DELETE <key>` | Delete a key |

`DELETE` belongs to both languages. The REPL routes `DELETE FROM ...` to the SQL engine and anything else to the key-value store.

### Transactions

| Command | Meaning |
| --- | --- |
| `BEGIN` | Start a transaction; the prompt becomes `db (txn:N)>` |
| `COMMIT` | Commit and release locks |
| `ROLLBACK` | Undo every change made by the transaction |
| `VACUUM` | Drop undo records no active transaction can still see |
| `EXIT` / `QUIT` | Leave the REPL |

Outside an explicit `BEGIN`, a read takes a fresh snapshot for the duration of that one statement, while a write is applied directly with no undo record — so it cannot be rolled back afterwards. If a statement throws, the REPL aborts the open transaction automatically.

## Architecture

```
                       ┌──────────────┐
                       │     REPL     │  src/cli
                       └──────┬───────┘
                              │  routes SQL vs. KV commands
             ┌────────────────┴─────────────────┐
             │                                  │
   ┌─────────▼─────────┐                        │
   │  Tokenizer        │  src/sql               │
   │    → Parser → AST │                        │
   │    → Executor     │  uses Catalog          │
   └─────────┬─────────┘  for table schemas     │
             │                                  │
             │  set / get / remove / scan       │
             └────────────────┬─────────────────┘
                              │
                     ┌────────▼────────┐
                     │    Database     │  src/core
                     │  MVCC visibility│
                     └────────┬────────┘
                              │
         ┌────────────────────┼────────────────────┐
         │                    │                    │
 ┌───────▼───────┐   ┌────────▼────────┐  ┌────────▼────────┐
 │ TransactionMgr│   │      BTree      │  │       Wal       │
 │ ReadView      │   │   + Iterator    │  │  append + fsync │
 │ undo log      │   └────────┬────────┘  └─────────────────┘
 │ LockManager   │            │
 └───────────────┘   ┌────────▼────────┐
                     │ BufferPoolMgr   │  LRU replacer, pinning
                     └────────┬────────┘
                              │
                     ┌────────▼────────┐
                     │      Pager      │  4 KB pages on disk
                     └─────────────────┘
```

| Layer | Files | Responsibility |
| --- | --- | --- |
| Storage | `storage/Pager` | Reads and writes fixed 4 KB pages, appends new ones |
| | `storage/Page` | Slotted-page layout, binary search, cell insertion |
| | `storage/Wal` | Append-only log with per-record checksums, flushed before pages change, replayed on startup |
| Buffer | `buffer/BufferPoolManager` | Frame table, pin counts, dirty tracking, eviction |
| | `buffer/LRUReplacer` | Chooses the victim frame |
| Index | `index/BTree` | B+ tree: descent, leaf split, separator propagation |
| | `index/BTreeIterator` | Ordered scan along the leaf sibling chain |
| Concurrency | `concurrency/LockManager` | Per-key shared/exclusive locks with a 1 s timeout |
| | `concurrency/Transaction` | Transaction state, held locks, undo records |
| | `concurrency/TransactionManager` | Ids, commit, abort, global undo log, vacuum |
| | `concurrency/ReadView` | Snapshot visibility rules |
| Core | `core/Database` | Ties the layers together; encodes row versions; MVCC reads |
| SQL | `sql/Tokenizer` → `sql/Parser` → `sql/AST` | Text to syntax tree |
| | `sql/Catalog` | Table schemas |
| | `sql/Executor` | Walks the tree and calls `Database` |
| CLI | `cli/Repl`, `cli/main` | Prompt, dispatch, transaction commands |

## How a query travels through the system

`INSERT INTO users VALUES (1, 'Henrik');` inside a transaction:

1. **Repl** sees `INSERT` and hands the line to the SQL path.
2. **Tokenizer** produces `INSERT INTO IDENTIFIER(users) VALUES ( NUMBER(1) , STRING(Henrik) ) ;`.
3. **Parser** builds an `InsertStatement`.
4. **Executor** looks the schema up in the **Catalog**, checks the value count, then builds the storage key `users:1` and the row `1,Henrik`.
5. **Database::set** takes an exclusive lock on the key, writes an undo record (so `ROLLBACK` can reverse it), and stamps the new version with the transaction id.
6. **Wal** appends the record and flushes it — the log is written before any page is touched (see the durability caveat under [Known limitations](#known-limitations)).
7. **BTree** descends to the right leaf through the **BufferPoolManager** and inserts the cell, splitting the leaf and propagating a separator to the parent if it is full.

A `SELECT` follows the same path down to `Database::scan`, which walks the leaf chain and, for every row, resolves the version visible to the reader's `ReadView` by following the undo chain backwards.

## Storage format

**Page.** Every page is 4 KB and slotted: an 18-byte header, then a slot array growing forward, then cell payloads growing backwards from the end.

```
┌────────────┬──────────────┬───────────────┬─────────────────┐
│  header    │ slot array → │   free space  │ ← cell payloads │
│  18 bytes  │  2 B / cell  │               │                 │
└────────────┴──────────────┴───────────────┴─────────────────┘
```

The header holds the node type, root flag, cell count, free-space pointer, parent id, next-leaf id and rightmost-child id. Cells are kept in sorted order by the slot array, so lookups are a binary search. The two node types store cells differently:

```
leaf cell     [key_len:2][val_len:2][key][value]
internal cell [left_child_id:4][key_len:2][key]
```

An internal cell's child pointer covers the keys *below* its separator; keys at or above the last separator live under `rightmost_child`.

**Row.** The SQL layer maps a row onto one key-value pair: the key is `<table>:<first column value>`, the value is the columns joined with commas. So `INSERT INTO users VALUES (1, 'Henrik')` becomes `users:1` → `1,Henrik`. The first column therefore acts as the primary key.

**Version.** Each stored value is prefixed with a `TupleMeta` header:

```
[txn_id:8][undo_lsn:8][is_deleted:1] + row bytes
```

`undo_lsn` links to the previous version in the transaction manager's undo log, forming the version chain that MVCC reads walk. `is_deleted` marks a tombstone.

**WAL.** Records are `[type:1][key_len:2][val_len:4][key][value][checksum:4]`, appended and flushed before the corresponding page is modified — write-ahead ordering is respected. On startup `Database` replays the log into the tree and then truncates it.

## Transactions

**Isolation.** Each transaction gets a `ReadView` at `BEGIN`: its own id, the set of transactions active at that moment, and the next id to be handed out. A version is visible if it was created by this transaction, or by a transaction that had already committed when the snapshot was taken. Readers never block writers — a reader that finds a too-new version follows `undo_lsn` back through the undo log until it reaches one it may see.

**Writes.** Writers take an exclusive lock per key and hold it until `COMMIT` or `ROLLBACK` (strict 2PL), which is what keeps concurrent writers from interleaving. Lock acquisition times out after one second and raises an error rather than deadlocking forever.

**Rollback.** Every write appends an undo record to the transaction. `ROLLBACK` replays them in reverse: an insert is undone by writing a tombstone, an update or delete by restoring the previous value.

**Vacuum.** Undo records stay reachable until no live snapshot can need them. `VACUUM` finds the oldest active transaction and drops every undo record older than it.

## Benchmarks

```bash
./build/minidb_bench          # 2000 operations per workload
./build/minidb_bench 5000     # or pick your own
```

The runner creates `minidb_bench.db` in the working directory, times each workload and removes the file afterwards. It verifies its own results — if a workload returns the wrong number of rows it prints a warning line, and if a workload hits an unimplemented limit it is reported as `SKIPPED` instead of aborting the run.

Measured on an Intel i7-6500U (2.50 GHz, 4 threads), GCC 13.3, `-O0` default build, 2000 operations:

| Workload | Throughput | Latency |
| --- | ---: | ---: |
| `INSERT` (WAL append + flush) | 46 200 ops/s | 21.6 µs |
| `GET` point lookup, no transaction | 104 900 ops/s | 9.5 µs |
| `GET` point lookup, MVCC snapshot | 95 100 ops/s | 10.5 µs |
| `UPDATE` in a transaction | 32 300 ops/s | 31.0 µs |
| `SCAN` full table | 248 000 rows/s | 4.0 µs |
| `ROLLBACK` (undo per record) | 85 800 ops/s | 11.7 µs |

Reading these numbers: scans are fastest because they follow the leaf sibling chain and never descend the tree. Point lookups pay for a root-to-leaf descent, and the MVCC variant adds the visibility check on top. Inserts pay for the WAL write on top of the tree insert, on every single write, with no group commit — though note that flush is not a physical `fsync`, so these figures do not include disk sync latency. Updates cost more than inserts because each one also allocates an undo record and pushes the previous version into the version chain.

The default build has no optimisation flags; configure with `-DCMAKE_BUILD_TYPE=Release` for meaningful absolute numbers.

## Tests

```bash
cd build && ctest --output-on-failure
```

12 GoogleTest binaries cover the stack bottom to top: pager, page layout, LRU replacer, buffer pool, B+ tree, WAL, database engine, lock manager, strict 2PL, read view, MVCC, and the SQL parser.

## Known limitations

Correctness and scale limits worth knowing before trusting anything to this engine.

**The WAL is not crash-proof against power loss.** `Wal::flush()` calls `std::fstream::flush()`, which hands the bytes to the operating system but never calls `fsync`, despite the declaration in `Wal.h` promising `fflush + fsync`. The log therefore survives a crash of the `minidb` process, but an OS crash or power cut can lose records the engine has already reported as written. Adding a real `fsync` would close the gap at a large cost in write throughput — the `INSERT` figure above is fast precisely because no physical sync happens.

**Schemas do not survive a restart.** The `Catalog` lives in memory and is owned by the REPL. Reopening the CLI loses every `CREATE TABLE`, so `SELECT * FROM users` fails with `Table not found` even though the rows are still on disk and still readable through `GET users:1`. Persisting the catalog into a system table is the natural next step.

**The tree stops growing at one internal level.** Leaf splits are implemented, internal node splits are not. Once the root internal node fills up — around 10 000 rows with short keys, fewer with long ones — an insert raises `BTree: internal node is full`. It fails loudly rather than corrupting the tree, but it is a hard ceiling.

**Nothing is ever reclaimed.** Deletes write tombstones and never free cells; updates append a new payload without compacting the old one; pages are never merged. A page that is repeatedly updated will split even though its live data would fit. There is no page-level vacuum, only undo-log vacuum.

**The database file name is hardcoded.** `main()` always opens `database.db` in the working directory and ignores command-line arguments.

**SQL coverage is narrow.** Only `SELECT *` — no column lists, no `JOIN`, `ORDER BY`, `GROUP BY`, aggregates, or subqueries. `WHERE` takes exactly one condition; there is no `AND`/`OR`. `UPDATE` sets one column at a time, and its `WHERE` supports only `=` and `!=` on `VARCHAR` columns, while `SELECT` and `DELETE` support the full operator set on strings.

**Rows are stored as CSV text.** A `VARCHAR` value containing a comma will be split into the wrong number of fields when read back. Values are not escaped and types are not enforced on insert.

**The key is the first column, implicitly.** There is no `PRIMARY KEY` declaration and no uniqueness check: inserting a row whose first column repeats overwrites the earlier row silently.

**Rollback loses version-chain links.** Undoing an update restores the old value with fresh metadata, so its `undo_lsn` back-pointer is dropped. A concurrent reader holding an older snapshot can no longer walk past that row to the version it should see. This is invisible in the single-threaded CLI.

**Concurrency is in-process only.** `LockManager` coordinates threads inside one process. Two `minidb_cli` processes opening the same file will corrupt it — there is no file locking.

## License

MIT — see [LICENSE](LICENSE).
