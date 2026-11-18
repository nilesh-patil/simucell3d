"""Tests for scripts/screening_analysis/utils/compute_IoU_euler.py."""
import importlib.util
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SCRIPT = REPO / "scripts" / "screening_analysis" / "utils" / "compute_IoU_euler.py"


def _load():
    spec = importlib.util.spec_from_file_location("compute_IoU_euler", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _run(tmp_path, monkeypatch, indexes, nb_simulations=3):
    module = _load()
    simulation_dir = tmp_path / "simulation_outputs"
    for i in range(1, nb_simulations + 1):
        (simulation_dir / f"simulation_{i}").mkdir(parents=True)
    output_dir = tmp_path / "out"
    (output_dir / "dashboard_files").mkdir(parents=True)

    submitted = []

    def fake_run(cmd, **kwargs):
        # The job scripts are deleted when the function returns: read them now.
        assert cmd[0] == "sbatch"
        submitted.append(Path(cmd[1]).read_text())

    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(module.subprocess, "run", fake_run)
    module.collect_shape_change_from_screening(str(simulation_dir), str(output_dir), indexes)
    return submitted


def _submitted_ids(scripts):
    return [s.strip().splitlines()[-1].split()[-2] for s in scripts]


def test_all_indexes(tmp_path, monkeypatch):
    assert _submitted_ids(_run(tmp_path, monkeypatch, "all")) == ["1", "2", "3"]


def test_explicit_indexes(tmp_path, monkeypatch):
    assert _submitted_ids(_run(tmp_path, monkeypatch, [2, 3])) == ["2", "3"]


def test_more_indexes_than_simulations_falls_back_to_all(tmp_path, monkeypatch):
    assert _submitted_ids(_run(tmp_path, monkeypatch, [1, 2, 3, 4, 5])) == ["1", "2", "3"]
