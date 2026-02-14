"""Tests for the 5 new visualization plots."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def test_new_plots_registered():
    """Test that all 5 new plots are registered in PLOT_REGISTRY."""
    from plot_benchmark_unified import PLOT_REGISTRY

    expected_new_ids = ['13', '14', '15', '16', '17']
    for plot_id in expected_new_ids:
        assert plot_id in PLOT_REGISTRY, \
            f"Plot '{plot_id}' not found in PLOT_REGISTRY. Available: {list(PLOT_REGISTRY.keys())}"

    print("PASS: test_new_plots_registered")


def test_total_plot_count():
    """Test that we have at least 17 plots registered (12 original + 5 new)."""
    from plot_benchmark_unified import PLOT_REGISTRY

    assert len(PLOT_REGISTRY) >= 17, \
        f"Expected >= 17 plots, got {len(PLOT_REGISTRY)}"

    print("PASS: test_total_plot_count")


def test_plot_functions_callable():
    """Test that all new plot functions are callable."""
    from plot_benchmark_unified import PLOT_REGISTRY

    for plot_id in ['13', '14', '15', '16', '17']:
        plot_info = PLOT_REGISTRY[plot_id]
        assert callable(plot_info.function), \
            f"Plot {plot_id} function is not callable"

    print("PASS: test_plot_functions_callable")


if __name__ == "__main__":
    test_name = sys.argv[1] if len(sys.argv) > 1 else None
    tests = {
        'test_new_plots_registered': test_new_plots_registered,
        'test_total_plot_count': test_total_plot_count,
        'test_plot_functions_callable': test_plot_functions_callable,
    }

    if test_name:
        tests[test_name]()
    else:
        for name, fn in tests.items():
            fn()
        print(f"\nAll {len(tests)} tests passed.")
