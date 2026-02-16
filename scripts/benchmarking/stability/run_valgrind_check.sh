#!/usr/bin/env bash
# run_valgrind_check.sh — Run stability tests under Valgrind for leak detection
#
# Usage: ./run_valgrind_check.sh [--build-dir DIR] [--test-pattern PATTERN]
#
# Runs CTest stability tests under Valgrind memcheck and reports results.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
TEST_PATTERN="stability"
SUPP_FILE="$SCRIPT_DIR/valgrind_suppressions.supp"
OUTPUT_DIR="/tmp/simucell3d_valgrind_$$"

usage() {
    echo "Usage: $0 [--build-dir DIR] [--test-pattern PATTERN]"
    echo ""
    echo "Options:"
    echo "  --build-dir DIR        Build directory (default: ./build)"
    echo "  --test-pattern PATTERN CTest filter pattern (default: stability)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --test-pattern) TEST_PATTERN="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# Check for Valgrind
if ! command -v valgrind &>/dev/null; then
    echo "Error: valgrind not found. Install it with: apt install valgrind"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "=== Valgrind Memory Check ==="
echo "Build dir: $BUILD_DIR"
echo "Test pattern: $TEST_PATTERN"
echo "Output dir: $OUTPUT_DIR"
echo ""

# Get list of test executables matching the pattern
cd "$BUILD_DIR"
TEST_EXECUTABLES=()
TEST_ARGS=()

# Extract test commands from CTest
while IFS= read -r line; do
    # CTest -N output format: "Test #N: test_name"
    test_name=$(echo "$line" | sed 's/.*: //')
    TEST_EXECUTABLES+=("$test_name")
done < <(ctest -N -R "$TEST_PATTERN" 2>/dev/null | grep "Test #")

if [[ ${#TEST_EXECUTABLES[@]} -eq 0 ]]; then
    echo "No tests found matching pattern '$TEST_PATTERN'"
    exit 1
fi

echo "Found ${#TEST_EXECUTABLES[@]} test(s) to check"
echo ""

# Build Valgrind options
VALGRIND_OPTS=(
    --tool=memcheck
    --leak-check=full
    --show-reachable=no
    --track-origins=yes
    --error-exitcode=42
    --xml=yes
)

if [[ -f "$SUPP_FILE" ]]; then
    VALGRIND_OPTS+=(--suppressions="$SUPP_FILE")
    echo "Using suppressions: $SUPP_FILE"
fi

PASS=0
FAIL=0
ERRORS=0

# Run each test under Valgrind via CTest
for test_name in "${TEST_EXECUTABLES[@]}"; do
    echo "--- Running: $test_name ---"
    XML_FILE="$OUTPUT_DIR/${test_name}.xml"
    LOG_FILE="$OUTPUT_DIR/${test_name}.log"

    VALGRIND_OPTS_WITH_XML=("${VALGRIND_OPTS[@]}" --xml-file="$XML_FILE")

    # Run test under Valgrind with timeout (5 min per test)
    if timeout 300 ctest -R "^${test_name}$" \
        --test-command valgrind "${VALGRIND_OPTS_WITH_XML[@]}" \
        --output-on-failure \
        > "$LOG_FILE" 2>&1; then
        echo "  PASS (no leaks)"
        PASS=$((PASS + 1))
    else
        EXIT_CODE=$?
        if [[ $EXIT_CODE -eq 42 ]]; then
            echo "  LEAK DETECTED — see $XML_FILE"
            ERRORS=$((ERRORS + 1))
        else
            echo "  FAIL (exit code $EXIT_CODE) — see $LOG_FILE"
            FAIL=$((FAIL + 1))
        fi
    fi
done

echo ""
echo "=== Valgrind Summary ==="
echo "Passed: $PASS"
echo "Leaks:  $ERRORS"
echo "Failed: $FAIL"
echo "Reports: $OUTPUT_DIR/"

if [[ $ERRORS -gt 0 || $FAIL -gt 0 ]]; then
    exit 1
fi
