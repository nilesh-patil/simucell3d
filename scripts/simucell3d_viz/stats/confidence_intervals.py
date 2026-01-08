"""
Confidence interval computation for time series data.

Provides both parametric and non-parametric methods for estimating
uncertainty in population means.
"""

from typing import Tuple, Optional
import numpy as np
from scipy import stats
import matplotlib.pyplot as plt


def compute_parametric_ci(
    mean: np.ndarray,
    std: np.ndarray,
    n_samples: int,
    confidence: float = 0.95
) -> Tuple[np.ndarray, np.ndarray]:
    """
    Compute parametric confidence interval on the population mean using t-distribution.

    For aggregated data (mean ± std from N samples), this computes the CI on the TRUE
    population mean, not on individual measurements.

    Formula: CI = mean ± t_(α/2, N-1) * (std / sqrt(N))

    where t_(α/2, N-1) is the critical value from the t-distribution with N-1 degrees
    of freedom.

    This is more rigorous than ±1σ bands because:
    1. Accounts for finite sample size (becomes narrower with more samples)
    2. Uses t-distribution (appropriate for unknown population variance)
    3. Gives actual confidence level (e.g., 95%) rather than ~68% from ±1σ

    Args:
        mean: Array of mean values at each timepoint (shape: [n_timepoints])
        std: Array of standard deviations at each timepoint (shape: [n_timepoints])
        n_samples: Number of samples used to compute mean/std (e.g., number of cells)
        confidence: Confidence level (default 0.95 for 95% CI)

    Returns:
        Tuple of (lower_bound, upper_bound) arrays

    References:
        - Altman & Bland (2011), "How to obtain the confidence interval from a P value"
        - Student's t-distribution for small sample sizes

    Example:
        >>> mean_pressure = np.array([100, 101, 102])  # Pa
        >>> std_pressure = np.array([10, 12, 11])       # Pa
        >>> n_cells = 50
        >>> lower, upper = compute_parametric_ci(mean_pressure, std_pressure, n_cells)
        >>> # Now: P(lower <= true_mean <= upper) = 0.95
    """
    if len(mean) != len(std):
        raise ValueError(f"mean and std must have same length: {len(mean)} vs {len(std)}")

    if n_samples < 2:
        raise ValueError(f"n_samples must be >= 2, got {n_samples}")

    # Degrees of freedom
    df = n_samples - 1

    # Critical value from t-distribution
    alpha = 1 - confidence
    t_critical = stats.t.ppf(1 - alpha/2, df)

    # Standard error of the mean
    se_mean = std / np.sqrt(n_samples)

    # Confidence interval
    margin = t_critical * se_mean
    lower = mean - margin
    upper = mean + margin

    return lower, upper


def compute_bootstrap_ci_mean(
    data_2d: np.ndarray,
    confidence: float = 0.95,
    n_bootstrap: int = 1000,
    random_seed: Optional[int] = None
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Compute bootstrap confidence interval on mean using percentile method.

    This is the NON-PARAMETRIC approach that doesn't assume normality.
    Requires raw data (not just aggregated mean/std).

    Args:
        data_2d: Raw data with shape [n_timepoints, n_samples]
                 e.g., pressure values for each cell at each timepoint
        confidence: Confidence level (default 0.95)
        n_bootstrap: Number of bootstrap iterations (default 1000)
        random_seed: Random seed for reproducibility

    Returns:
        Tuple of (mean, lower_bound, upper_bound) arrays

    Algorithm:
        For each timepoint:
        1. Resample N values with replacement
        2. Compute mean of resample
        3. Repeat 1000 times -> distribution of bootstrap means
        4. Take 2.5th and 97.5th percentiles for 95% CI

    References:
        - Efron & Tibshirani (1993), "An Introduction to the Bootstrap"
        - DiCiccio & Efron (1996), "Bootstrap confidence intervals"

    Example:
        >>> # Pressure for 50 cells across 100 timepoints
        >>> pressures = np.random.randn(100, 50) * 10 + 100
        >>> mean, lower, upper = compute_bootstrap_ci_mean(pressures)
    """
    if random_seed is not None:
        np.random.seed(random_seed)

    n_timepoints, n_samples = data_2d.shape

    if n_samples < 2:
        raise ValueError(f"Need at least 2 samples for bootstrap, got {n_samples}")

    alpha = 1 - confidence
    percentile_lower = 100 * alpha / 2
    percentile_upper = 100 * (1 - alpha / 2)

    # Storage for results
    means = np.zeros(n_timepoints)
    lower = np.zeros(n_timepoints)
    upper = np.zeros(n_timepoints)

    for i in range(n_timepoints):
        samples = data_2d[i]

        # Remove NaNs
        samples = samples[np.isfinite(samples)]
        if len(samples) < 2:
            means[i] = np.nan
            lower[i] = np.nan
            upper[i] = np.nan
            continue

        # Bootstrap resampling
        boot_means = np.zeros(n_bootstrap)
        for b in range(n_bootstrap):
            resample = np.random.choice(samples, size=len(samples), replace=True)
            boot_means[b] = np.mean(resample)

        # Percentile-based CI
        means[i] = np.mean(samples)
        lower[i] = np.percentile(boot_means, percentile_lower)
        upper[i] = np.percentile(boot_means, percentile_upper)

    return means, lower, upper


def add_ci_band(
    ax: plt.Axes,
    x: np.ndarray,
    y_lower: np.ndarray,
    y_upper: np.ndarray,
    color,
    alpha: float = 0.25,
    label: str = "95% CI"
) -> None:
    """
    Add confidence interval band to matplotlib axes.

    Args:
        ax: Matplotlib axes object
        x: X-axis values (e.g., time)
        y_lower: Lower bound of confidence interval
        y_upper: Upper bound of confidence interval
        color: Band color
        alpha: Transparency (default 0.25)
        label: Legend label (default "95% CI")
    """
    ax.fill_between(
        x,
        y_lower,
        y_upper,
        color=color,
        alpha=alpha,
        linewidth=0,
        label=label
    )
