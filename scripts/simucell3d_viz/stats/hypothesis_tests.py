"""
Statistical hypothesis tests for benchmark analysis.

Provides tests for energy conservation and scheduler performance comparison.
"""

from dataclasses import dataclass
from typing import Tuple, Optional
import numpy as np
from scipy import stats
from statsmodels.tsa.stattools import kpss


@dataclass
class EnergyConservationResult:
    """Results from energy conservation statistical test."""

    # Linear regression test
    slope: float  # Energy drift rate (J/s)
    slope_pvalue: float  # P-value for H0: slope = 0
    r_squared: float  # Coefficient of determination

    # KPSS stationarity test
    kpss_statistic: float  # KPSS test statistic
    kpss_pvalue: float  # P-value (higher is better for stationarity)
    kpss_critical_values: dict  # Critical values at different significance levels

    # Relative drift rate
    rel_drift_rate: float  # (dE/E) per second
    mean_energy: float  # Mean energy (J)

    # Conservation verdict
    is_conserved: bool  # True if energy appears conserved

    def __str__(self) -> str:
        """Format results as human-readable string."""
        lines = [
            "Energy Conservation Analysis",
            "=" * 50,
            f"Mean Energy: {self.mean_energy:.2e} J",
            f"",
            f"Linear Regression Test (H0: slope = 0)",
            f"  Drift Rate: {self.slope:.2e} J/s",
            f"  R²: {self.r_squared:.4f}",
            f"  P-value: {self.slope_pvalue:.4f}",
            f"",
            f"KPSS Stationarity Test (H0: stationary)",
            f"  Test Statistic: {self.kpss_statistic:.4f}",
            f"  P-value: {self.kpss_pvalue:.4f}",
            f"  Critical Values: {self.kpss_critical_values}",
            f"",
            f"Relative Drift: {self.rel_drift_rate:.2e}/s ({abs(self.rel_drift_rate)*100:.4f}%/s)",
            f"",
            f"Verdict: {'✓ CONSERVED' if self.is_conserved else '⚠ NOT CONSERVED'}",
        ]
        return "\n".join(lines)


def test_energy_conservation(
    time_seconds: np.ndarray,
    energy: np.ndarray,
    significance: float = 0.05
) -> EnergyConservationResult:
    """
    Test whether total energy is conserved using multiple statistical methods.

    Performs three complementary tests:
    1. Linear regression: Is the trend significantly non-zero?
    2. KPSS stationarity test: Is the series stationary?
    3. Relative drift rate: How fast is energy changing?

    Args:
        time_seconds: Time array in seconds
        energy: Total energy array (J)
        significance: Significance level for tests (default 0.05)

    Returns:
        EnergyConservationResult with test statistics and verdict

    Interpretation:
        Energy is considered CONSERVED if:
        - Linear regression p-value > 0.05 (no significant trend)
        - KPSS p-value > 0.05 (series is stationary)
        - Relative drift rate < 1e-6 /s (< 0.0001%/s)

    Example:
        >>> t = np.linspace(0, 100, 1000)
        >>> E = np.ones(1000) * 1e-10  # Constant energy
        >>> result = test_energy_conservation(t, E)
        >>> print(result.is_conserved)  # True
    """
    # Remove NaN/inf values
    valid = np.isfinite(time_seconds) & np.isfinite(energy)
    time_valid = time_seconds[valid]
    energy_valid = energy[valid]

    if len(time_valid) < 10:
        raise ValueError(f"Need at least 10 valid points for test, got {len(time_valid)}")

    # 1. Linear regression test: E(t) = a*t + b
    # H0: a = 0 (no drift)
    slope, intercept, r_value, p_value, std_err = stats.linregress(time_valid, energy_valid)
    r_squared = r_value ** 2

    # 2. KPSS stationarity test
    # H0: series is stationary (conserved)
    # HA: series has unit root (drifting)
    try:
        kpss_stat, kpss_p, kpss_lags, kpss_crit = kpss(energy_valid, regression='c', nlags='auto')
        kpss_crit_dict = {f"{k}%": v for k, v in kpss_crit.items()}
    except Exception as e:
        # Fallback if KPSS fails (e.g., too few points)
        kpss_stat = np.nan
        kpss_p = np.nan
        kpss_crit_dict = {}

    # 3. Relative drift rate
    mean_energy = np.mean(energy_valid)
    rel_drift_rate = slope / mean_energy if mean_energy != 0 else np.inf

    # Conservation verdict (all three tests must pass)
    is_conserved = (
        p_value > significance and  # No significant linear trend
        kpss_p > significance and  # Series is stationary
        abs(rel_drift_rate) < 1e-6  # Drift < 0.0001%/s
    )

    return EnergyConservationResult(
        slope=slope,
        slope_pvalue=p_value,
        r_squared=r_squared,
        kpss_statistic=kpss_stat,
        kpss_pvalue=kpss_p,
        kpss_critical_values=kpss_crit_dict,
        rel_drift_rate=rel_drift_rate,
        mean_energy=mean_energy,
        is_conserved=is_conserved
    )


@dataclass
class PermutationTestResult:
    """Results from bootstrap permutation test."""

    observed_diff: float  # Observed difference in means/medians
    pvalue: float  # P-value from permutation test
    pvalue_twosided: float  # Two-sided p-value
    effect_size: float  # Effect size (difference / pooled std)
    is_significant: bool  # True if p < significance level
    n_permutations: int  # Number of permutations performed

    def __str__(self) -> str:
        """Format results as human-readable string."""
        sig_stars = "***" if self.pvalue < 0.001 else "**" if self.pvalue < 0.01 else "*" if self.pvalue < 0.05 else "ns"
        lines = [
            "Bootstrap Permutation Test",
            "=" * 50,
            f"Observed Difference: {self.observed_diff:.4f}",
            f"Effect Size: {self.effect_size:.4f}",
            f"P-value (one-sided): {self.pvalue:.4f}",
            f"P-value (two-sided): {self.pvalue_twosided:.4f}",
            f"Significance: {sig_stars}",
            f"Permutations: {self.n_permutations:,}",
        ]
        return "\n".join(lines)


def bootstrap_permutation_test(
    group1: np.ndarray,
    group2: np.ndarray,
    n_permutations: int = 10000,
    statistic: str = "mean",
    significance: float = 0.05,
    random_seed: Optional[int] = None
) -> PermutationTestResult:
    """
    Bootstrap permutation test for comparing two independent groups.

    This is a NON-PARAMETRIC test that doesn't assume normality.
    Tests whether two groups have different central tendencies.

    Algorithm:
        1. Compute observed difference (e.g., mean1 - mean2)
        2. Pool all data from both groups
        3. Randomly permute pool and split into two groups of original sizes
        4. Compute permuted difference
        5. Repeat 10,000 times -> null distribution
        6. P-value = fraction of permuted diffs >= observed diff

    Args:
        group1: Data from first group (e.g., scheduler A times)
        group2: Data from second group (e.g., scheduler B times)
        n_permutations: Number of bootstrap permutations (default 10,000)
        statistic: Which statistic to compare ("mean" or "median")
        significance: Significance level (default 0.05)
        random_seed: Random seed for reproducibility

    Returns:
        PermutationTestResult with p-value and effect size

    References:
        - Good (2005), "Permutation, Parametric, and Bootstrap Tests of Hypotheses"
        - Efron & Tibshirani (1993), "An Introduction to the Bootstrap"

    Example:
        >>> scheduler_a_times = np.array([10.2, 10.5, 10.1, 10.3])
        >>> scheduler_b_times = np.array([12.1, 12.3, 12.0, 12.2])
        >>> result = bootstrap_permutation_test(scheduler_a_times, scheduler_b_times)
        >>> print(f"P-value: {result.pvalue:.4f}")
        >>> print(f"B is {'significantly' if result.is_significant else 'not'} slower than A")
    """
    if random_seed is not None:
        np.random.seed(random_seed)

    # Remove NaNs
    group1 = group1[np.isfinite(group1)]
    group2 = group2[np.isfinite(group2)]

    if len(group1) < 2 or len(group2) < 2:
        raise ValueError(f"Need at least 2 samples per group, got {len(group1)} and {len(group2)}")

    # Choose statistic function
    if statistic == "mean":
        stat_fn = np.mean
    elif statistic == "median":
        stat_fn = np.median
    else:
        raise ValueError(f"statistic must be 'mean' or 'median', got '{statistic}'")

    # Observed difference
    obs_stat1 = stat_fn(group1)
    obs_stat2 = stat_fn(group2)
    observed_diff = obs_stat1 - obs_stat2

    # Pool all data
    pooled = np.concatenate([group1, group2])
    n1 = len(group1)
    n2 = len(group2)

    # Permutation test
    perm_diffs = np.zeros(n_permutations)

    for i in range(n_permutations):
        # Randomly permute and split
        np.random.shuffle(pooled)
        perm_group1 = pooled[:n1]
        perm_group2 = pooled[n1:]

        # Compute permuted difference
        perm_diffs[i] = stat_fn(perm_group1) - stat_fn(perm_group2)

    # P-value (one-sided: is diff significantly > 0?)
    pvalue_onesided = np.mean(perm_diffs >= observed_diff)

    # P-value (two-sided: is diff significantly != 0?)
    pvalue_twosided = np.mean(np.abs(perm_diffs) >= abs(observed_diff))

    # Effect size (Cohen's d-like measure)
    pooled_std = np.std(pooled, ddof=1)
    effect_size = observed_diff / pooled_std if pooled_std > 0 else np.inf

    is_significant = pvalue_twosided < significance

    return PermutationTestResult(
        observed_diff=observed_diff,
        pvalue=pvalue_onesided,
        pvalue_twosided=pvalue_twosided,
        effect_size=effect_size,
        is_significant=is_significant,
        n_permutations=n_permutations
    )
