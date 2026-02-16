"""Tests for static HTML dashboard generation."""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def test_generate_minimal_dashboard():
    """Test generating a minimal dashboard HTML file."""
    from ci.generate_dashboard import generate_dashboard

    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = Path(tmpdir) / "index.html"

        generate_dashboard(
            output_path=output_path,
            title="SimuCell3D Benchmarks",
            plots_dir=None,
            regression_report=None,
            run_metadata={'commit': 'abc1234', 'branch': 'main', 'scheduler': 'adaptive'}
        )

        assert output_path.exists(), "Dashboard HTML not created"
        html = output_path.read_text()
        assert "<html" in html, "Not valid HTML"
        assert "SimuCell3D" in html, "Title not in HTML"
        assert "abc1234" in html, "Commit not in HTML"

    print("PASS: test_generate_minimal_dashboard")


def test_dashboard_with_plots():
    """Test dashboard with mock plot images."""
    from ci.generate_dashboard import generate_dashboard

    with tempfile.TemporaryDirectory() as tmpdir:
        plots_dir = Path(tmpdir) / "plots"
        plots_dir.mkdir()
        for i in range(3):
            (plots_dir / f"0{i+1}_test_plot.png").write_bytes(
                b'\x89PNG\r\n\x1a\n' + b'\x00' * 100
            )

        output_path = Path(tmpdir) / "index.html"

        generate_dashboard(
            output_path=output_path,
            title="SimuCell3D Benchmarks",
            plots_dir=plots_dir,
            regression_report="## No regressions detected",
            run_metadata={'commit': 'abc1234', 'branch': 'main'}
        )

        html = output_path.read_text()
        assert "01_test_plot.png" in html, "Plot reference not in HTML"

    print("PASS: test_dashboard_with_plots")


if __name__ == "__main__":
    test_name = sys.argv[1] if len(sys.argv) > 1 else None
    tests = {
        'test_generate_minimal_dashboard': test_generate_minimal_dashboard,
        'test_dashboard_with_plots': test_dashboard_with_plots,
    }

    if test_name:
        tests[test_name]()
    else:
        for name, fn in tests.items():
            fn()
        print(f"\nAll {len(tests)} tests passed.")
