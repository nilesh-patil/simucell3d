"""
Regression diagnostics for scaling analysis.

Provides tools for validating power-law fits and detecting outliers.
"""

from dataclasses import dataclass
from typing import Tuple, Optional
import numpy as np
from scipy import stats


@dataclass
class PowerLawFitResult:
    """Results from power-law regression analysis."""

    # Fitted parameters
    exponent: float  # Power-law exponent (e.g., 1.33 for O(N^4/3))
    coefficient: float  # Scaling coefficient
    r_squared: float  # Coefficient of determination

    # Prediction intervals
    pred_lower: np.ndarray  # Lower 95% prediction interval
    pred_upper: np.ndarray  # Upper 95% prediction interval

    # Residuals
    residuals: np.ndarray  # Residuals in log space
    residuals_normalized: np.ndarray  # Standardized residuals

    # Fit quality metrics
    rmse: float  # Root mean squared error (log space)
    mape: float  # Mean absolute percentage error

    # Data arrays for plotting
    x_data: np.ndarray  # Original x values
    y_data: np.ndarray  # Original y values
    y_pred: np.ndarray  # Predicted y values

    def __str__(self) -> str:
        """Format results as human-readable string."""
        lines = [
            "Power-Law Fit Analysis",
            "=" * 50,
            f"Model: y = {self.coefficient:.2e} * x^{self.exponent:.4f}",
            f"R²: {self.r_squared:.4f}",
            f"RMSE: {self.rmse:.4f}",
            f"MAPE: {self.mape:.2f}%",
            f"",
            f"Exponent: {self.exponent:.4f} (theoretical 4/3 = {4/3:.4f})",
            f"Deviation: {abs(self.exponent - 4/3):.4f}",
        ]
        return "\n".join(lines)


def compute_power_law_fit(
    x: np.ndarray,
    y: np.ndarray,
    confidence: float = 0.95
) -> PowerLawFitResult:
    """
    Fit power-law model y = a * x^b and compute diagnostic statistics.

    Uses log-log linear regression: log(y) = log(a) + b*log(x)

    Args:
        x: Independent variable (e.g., cell count)
        y: Dependent variable (e.g., time per iteration)
        confidence: Confidence level for prediction intervals (default 0.95)

    Returns:
        PowerLawFitResult with fit parameters and diagnostics

    Diagnostics included:
        - R²: Goodness of fit (1.0 = perfect)
        - RMSE: Root mean squared error in log space
        - MAPE: Mean absolute percentage error
        - Residuals: For checking normality assumption
        - Prediction intervals: 95% uncertainty band

    Example:
        >>> cells = np.array([10, 20, 50, 100, 200])
        >>> time_ms = 2.5 * cells**(4/3) + np.random.randn(5)
        >>> fit = compute_power_law_fit(cells, time_ms)
        >>> print(f"Fitted exponent: {fit.exponent:.3f}")
    """
    # Remove invalid values
    valid = (x > 0) & (y > 0) & np.isfinite(x) & np.isfinite(y)
    x_valid = x[valid]
    y_valid = y[valid]

    if len(x_valid) < 3:
        raise ValueError(f"Need at least 3 valid points for regression, got {len(x_valid)}")

    # Log transform
    log_x = np.log10(x_valid)
    log_y = np.log10(y_valid)

    # Linear regression in log-log space
    slope, intercept, r_value, p_value, std_err = stats.linregress(log_x, log_y)
    r_squared = r_value ** 2

    # Extract power-law parameters
    exponent = slope
    coefficient = 10 ** intercept

    # Predictions
    log_y_pred = intercept + slope * log_x
    y_pred = 10 ** log_y_pred

    # Residuals (in log space)
    residuals = log_y - log_y_pred

    # Standardized residuals
    residuals_std = residuals / np.std(residuals, ddof=1)

    # RMSE in log space
    rmse = np.sqrt(np.mean(residuals ** 2))

    # MAPE (mean absolute percentage error) in original space
    mape = 100 * np.mean(np.abs((y_valid - y_pred) / y_valid))

    # 95% prediction intervals
    # For new observations, not just confidence on the mean
    n = len(x_valid)
    dof = n - 2  # Degrees of freedom

    # Standard error of estimate
    se = np.sqrt(np.sum(residuals ** 2) / dof)

    # t-value for confidence level
    alpha = 1 - confidence
    t_val = stats.t.ppf(1 - alpha/2, dof)

    # Leverage for prediction interval
    # h = 1/n + (log_x - mean(log_x))^2 / sum((log_x - mean(log_x))^2)
    mean_log_x = np.mean(log_x)
    ss_x = np.sum((log_x - mean_log_x) ** 2)
    leverage = 1/n + (log_x - mean_log_x)**2 / ss_x

    # Prediction interval margin (in log space)
    margin = t_val * se * np.sqrt(1 + leverage)

    # Convert back to original space
    pred_lower = 10 ** (log_y_pred - margin)
    pred_upper = 10 ** (log_y_pred + margin)

    return PowerLawFitResult(
        exponent=exponent,
        coefficient=coefficient,
        r_squared=r_squared,
        pred_lower=pred_lower,
        pred_upper=pred_upper,
        residuals=residuals,
        residuals_normalized=residuals_std,
        rmse=rmse,
        mape=mape,
        x_data=x_valid,
        y_data=y_valid,
        y_pred=y_pred
    )


def compute_residuals(
    y_true: np.ndarray,
    y_pred: np.ndarray,
    standardize: bool = True
) -> np.ndarray:
    """
    Compute residuals for regression diagnostics.

    Args:
        y_true: Observed values
        y_pred: Predicted values
        standardize: If True, return standardized residuals (default)

    Returns:
        Residuals array

    Standardized residuals should have:
        - Mean ≈ 0
        - Std ≈ 1
        - Approximately normal distribution
        - No patterns when plotted vs. fitted values
    """
    residuals = y_true - y_pred

    if standardize:
        std = np.std(residuals, ddof=1)
        if std > 0:
            residuals = residuals / std

    return residuals


def compute_cooks_distance(
    x: np.ndarray,
    y: np.ndarray,
    y_pred: np.ndarray,
    residuals: np.ndarray
) -> np.ndarray:
    """
    Compute Cook's distance for identifying influential outliers.

    Cook's distance measures how much the regression would change if a point
    is removed. High values indicate influential outliers that should be
    investigated.

    Args:
        x: Independent variable
        y: Dependent variable
        y_pred: Predicted values from regression
        residuals: Residuals from regression

    Returns:
        Cook's distance for each data point

    Interpretation:
        - D > 1.0: Very influential, investigate
        - D > 0.5: Moderately influential, check
        - D > 4/n: Potentially influential (common threshold)

    References:
        - Cook (1977), "Detection of Influential Observations in Linear Regression"

    Example:
        >>> cells = np.array([10, 20, 50, 100, 200, 1000])  # 1000 is outlier
        >>> time = 2 * cells**(4/3)
        >>> time[-1] *= 10  # Make outlier
        >>> y_pred = 2 * cells**(4/3)
        >>> residuals = time - y_pred
        >>> cooks_d = compute_cooks_distance(cells, time, y_pred, residuals)
        >>> print(f"Max Cook's D: {np.max(cooks_d):.2f}")  # Will be high for outlier
    """
    n = len(x)
    p = 2  # Number of parameters (intercept + slope)

    # Mean squared error
    mse = np.sum(residuals ** 2) / (n - p)

    # Leverage (hat values)
    # h_i = 1/n + (x_i - mean(x))^2 / sum((x - mean(x))^2)
    x_mean = np.mean(x)
    x_centered = x - x_mean
    ss_x = np.sum(x_centered ** 2)

    if ss_x == 0:
        # All x values are the same - can't compute leverage
        return np.zeros(n)

    leverage = 1/n + x_centered**2 / ss_x

    # Standardized residuals
    residuals_std = residuals / np.sqrt(mse * (1 - leverage))

    # Cook's distance
    cooks_d = (residuals_std ** 2 / p) * (leverage / (1 - leverage))

    return cooks_d
