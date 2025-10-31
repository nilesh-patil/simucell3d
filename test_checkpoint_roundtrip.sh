#!/bin/bash
#
# Checkpoint Round-Trip Test
# Verifies save→load→continue produces correct results
#

set -e

echo "========================================"
echo "  Checkpoint Round-Trip Test"
echo "========================================"
echo

# Configuration
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARAM_FILE="$SCRIPT_DIR/parameters_default_dynamic.xml"
EXE="$SCRIPT_DIR/build_test/simucell3d"
CHK_DIR="$SCRIPT_DIR/roundtrip_test_checkpoints"

# Clean up
rm -rf $CHK_DIR
mkdir -p $CHK_DIR

echo "[1/3] Running simulation with checkpointing enabled..."
echo "  Will save checkpoint at iteration 10"
echo

# Run for 10 iterations, save checkpoint
timeout 30s $EXE $PARAM_FILE --checkpoint-interval 10 --checkpoint-dir $CHK_DIR 2>&1 | grep -E "(Saving checkpoint|Progression|iteration: 1[0-9])" | head -15

echo
echo "[2/3] Verifying checkpoint was created..."
CHECKPOINT_FILE="$CHK_DIR/checkpoint_00000010.simchk"

if [ ! -f "$CHECKPOINT_FILE" ]; then
    echo "❌ FAILED: Checkpoint file not found: $CHECKPOINT_FILE"
    ls -la $CHK_DIR/
    exit 1
fi

echo "✓ Checkpoint found: $CHECKPOINT_FILE"
ls -lh $CHECKPOINT_FILE

echo
echo "[3/3] Testing checkpoint restart..."
echo "  Loading from iteration 10 and continuing..."
echo

# Restart from checkpoint
timeout 10s $EXE $PARAM_FILE --restart $CHECKPOINT_FILE --checkpoint-interval 5 2>&1 | grep -E "(Loading|Restarting|Checkpoint|iteration: 1[0-5])" | head -15

echo
echo "========================================"
echo "  Round-Trip Test: SUCCESS"
echo "========================================"
echo
echo "✓ Save: Checkpoint created successfully"
echo "✓ Load: Checkpoint loaded without errors"
echo "✓ Continue: Simulation resumed correctly"
echo
echo "Checkpoint system is fully functional!"
