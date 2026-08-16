#!/usr/bin/env bash
#
# End-to-end smoke test for the MiniDB REPL.
#
# The unit tests cover the engine, but the REPL layer - command dispatch,
# transaction commands, SQL vs. key-value routing - has no unit tests of its
# own. This script drives the real binary and checks what it prints.
#
# Usage: scripts/smoke_test.sh [path-to-minidb_cli]

set -euo pipefail

CLI="${1:-build/minidb_cli}"

if [[ ! -x "$CLI" ]]; then
    echo "error: '$CLI' not found or not executable" >&2
    exit 1
fi

CLI="$(cd "$(dirname "$CLI")" && pwd)/$(basename "$CLI")"

# The CLI always opens ./database.db, so give each run its own directory
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
cd "$WORK_DIR"

failures=0
case_num=0

# run_case <name> <sql-input> then a list of assertions on the captured output:
#   present:<text>   the output must contain <text>
#   absent:<text>    the output must not contain <text>
run_case()
{
    local name="$1"
    local input="$2"
    shift 2

    case_num=$((case_num + 1))

    rm -f database.db database.db.log
    local output
    output="$(printf '%b' "$input" | "$CLI" 2>&1)"

    local ok=1
    local assertion
    for assertion in "$@"; do
        local kind="${assertion%%:*}"
        local text="${assertion#*:}"

        case "$kind" in
            present)
                if ! grep -qF -- "$text" <<< "$output"; then
                    echo "  expected to find: $text"
                    ok=0
                fi
                ;;
            absent)
                if grep -qF -- "$text" <<< "$output"; then
                    echo "  expected NOT to find: $text"
                    ok=0
                fi
                ;;
            *)
                echo "  unknown assertion kind: $kind"
                ok=0
                ;;
        esac
    done

    if [[ $ok -eq 1 ]]; then
        echo "ok $case_num - $name"
    else
        echo "not ok $case_num - $name"
        echo "--- output ---"
        sed 's/^/  | /' <<< "$output"
        echo "--------------"
        failures=$((failures + 1))
    fi
}

echo "# MiniDB smoke test using $CLI"

run_case "create, insert and select" \
    'CREATE TABLE users (id INT, name VARCHAR);\nINSERT INTO users VALUES (1, '"'"'Henrik'"'"');\nINSERT INTO users VALUES (2, '"'"'Alex'"'"');\nSELECT * FROM users;\nexit\n' \
    "present:Table 'users' created successfully." \
    "present:1 row inserted into 'users'." \
    "present:1,Henrik" \
    "present:2,Alex"

run_case "select with a WHERE filter" \
    'CREATE TABLE users (id INT, age INT);\nINSERT INTO users VALUES (1, 20);\nINSERT INTO users VALUES (2, 5);\nSELECT * FROM users WHERE age > 18;\nexit\n' \
    "present:1,20" \
    "absent:2,5"

# Regression: "DELETE FROM ..." must reach the SQL engine. It used to fall
# through to the key-value handler, which deleted a key literally named "FROM"
# and reported "OK" while every row stayed in the table.
run_case "SQL DELETE is routed to the SQL engine" \
    'CREATE TABLE users (id INT, age INT);\nINSERT INTO users VALUES (1, 20);\nINSERT INTO users VALUES (2, 5);\nDELETE FROM users WHERE age < 18;\nSELECT * FROM users;\nexit\n' \
    "present:1 row(s) deleted from 'users'." \
    "present:1,20" \
    "absent:2,5"

run_case "UPDATE rewrites the matching row" \
    'CREATE TABLE users (id INT, age INT);\nINSERT INTO users VALUES (1, 20);\nUPDATE users SET age = 99 WHERE id = 1;\nSELECT * FROM users;\nexit\n' \
    "present:1 row(s) updated in 'users'." \
    "present:1,99"

# Regression: ROLLBACK used to report success while leaving the row in place,
# because the REPL never handed its TransactionManager to the Database, so no
# undo records were ever recorded.
run_case "ROLLBACK undoes an insert made inside a transaction" \
    'CREATE TABLE users (id INT, name VARCHAR);\nINSERT INTO users VALUES (1, '"'"'Henrik'"'"');\nBEGIN;\nINSERT INTO users VALUES (2, '"'"'Alex'"'"');\nROLLBACK;\nSELECT * FROM users;\nexit\n' \
    "present:Transaction rolled back." \
    "present:1,Henrik" \
    "absent:2,Alex"

run_case "ROLLBACK restores a row deleted inside a transaction" \
    'CREATE TABLE users (id INT, name VARCHAR);\nINSERT INTO users VALUES (1, '"'"'Henrik'"'"');\nBEGIN;\nDELETE FROM users WHERE id = 1;\nROLLBACK;\nSELECT * FROM users;\nexit\n' \
    "present:1,Henrik"

run_case "COMMIT keeps the change" \
    'CREATE TABLE users (id INT, name VARCHAR);\nBEGIN;\nINSERT INTO users VALUES (1, '"'"'Henrik'"'"');\nCOMMIT;\nSELECT * FROM users;\nexit\n' \
    "present:Transaction committed." \
    "present:1,Henrik"

# "DELETE <key>" without FROM still has to reach the key-value store
run_case "key-value commands still work" \
    'SET mykey hello\nGET mykey\nDELETE mykey\nGET mykey\nexit\n' \
    "present:hello" \
    "present:(nil)"

run_case "errors are reported instead of crashing" \
    'SELECT * FROM nosuchtable;\nROLLBACK;\nNONSENSE;\nexit\n' \
    "present:Error: Table not found: nosuchtable" \
    "present:Error: No active transaction." \
    "present:Error: Unknown command"

echo "1..$case_num"

if [[ $failures -ne 0 ]]; then
    echo "# $failures of $case_num cases failed"
    exit 1
fi

echo "# all $case_num cases passed"
