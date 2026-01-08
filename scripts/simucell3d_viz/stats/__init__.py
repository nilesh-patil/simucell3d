"""
Statistical analysis functions for SimuCell3D benchmarks.

Provides rigorous statistical methods for publication-quality analysis:
- Confidence intervals (parametric and bootstrap)
- Hypothesis testing (energy conservation, scheduler comparisons)
- Regression diagnostics (residuals, outliers, goodness-of-fit)
"""

from .confidence_intervals import (
    compute_parametric_ci,
    compute_bootstrap_ci_mean,
    add_ci_band,
)

from .hypothesis_tests import (
    test_energy_conservation,
    bootstrap_permutation_test,
    EnergyConservationResult,
    PermutationTestResult,
)

from .regression_diagnostics import (
    compute_power_law_fit,
    compute_residuals,
    compute_cooks_distance,
    PowerLawFitResult,
)

__all__ = [
    # Confidence intervals
    "compute_parametric_ci",
    "compute_bootstrap_ci_mean",
    "add_ci_band",
    # Hypothesis tests
    "test_energy_conservation",
    "bootstrap_permutation_test",
    "EnergyConservationResult",
    "PermutationTestResult",
    # Regression diagnostics
    "compute_power_law_fit",
    "compute_residuals",
    "compute_cooks_distance",
    "PowerLawFitResult",
]
