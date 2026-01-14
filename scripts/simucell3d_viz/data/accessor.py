"""
Data accessor for scheduler benchmark metrics.

Provides type-safe, validated access to scheduler data with common patterns
extracted from plot functions. Eliminates code duplication and improves testability.
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

import numpy as np
import pandas as pd


@dataclass
class TimeSeriesData:
    """Validated time series data with uncertainty quantification."""

    time: np.ndarray  # Time in seconds since process start
    values: np.ndarray  # Main data values (e.g., pressure, energy)
    std: Optional[np.ndarray] = None  # Standard deviation (if available)
    n_valid: int = 0  # Number of valid data points
    column_name: str = ""  # Name of the column used

    def __post_init__(self):
        """Validate data consistency."""
        if len(self.time) != len(self.values):
            raise ValueError(f"Time and values must have same length: {len(self.time)} vs {len(self.values)}")
        if self.std is not None and len(self.std) != len(self.values):
            raise ValueError(f"Std must match values length: {len(self.std)} vs {len(self.values)}")
        if self.n_valid == 0:
            self.n_valid = len(self.values)


@dataclass
class StatsSummary:
    """Statistical summary of a data column."""

    mean: float
    std: float
    median: float
    q25: float  # 25th percentile
    q75: float  # 75th percentile
    min: float
    max: float
    n_valid: int

    def coefficient_of_variation(self) -> float:
        """Compute coefficient of variation (CV = std/mean)."""
        return self.std / self.mean if self.mean != 0 else float('inf')

    def iqr(self) -> float:
        """Interquartile range."""
        return self.q75 - self.q25


class SchedulerDataAccessor:
    """
    Type-safe accessor for scheduler benchmark data.

    Encapsulates common data access patterns:
    - Dynamic column finding with fallbacks
    - Time series extraction with validation
    - Invalid value filtering
    - Standard deviation retrieval

    Example:
        >>> accessor = SchedulerDataAccessor(data, 'adaptive', 'biological')
        >>> ts = accessor.get_time_series(['avg_pressure', 'pressure'],
        ...                                ['std_pressure'], min_points=10)
        >>> if ts:
        ...     plot(ts.time, ts.values, yerr=ts.std)
    """

    def __init__(self, data: Dict[str, Any], scheduler_name: str, data_type: str):
        """
        Initialize accessor for a specific scheduler and data type.

        Args:
            data: Main data dictionary from load_and_clean_data()
            scheduler_name: Name of scheduler (e.g., 'adaptive', 'static')
            data_type: Type of data ('biological', 'computational', 'phase', 'simulation_stats')
        """
        self.data = data
        self.scheduler_name = scheduler_name
        self.data_type = data_type

        # Get the dataframe
        key = f'{scheduler_name}_{data_type}'
        self.df: Optional[pd.DataFrame] = data.get(key)

        # Import compute_time_seconds from parent module
        # We'll import it dynamically to avoid circular imports
        self._compute_time_fn = None

    def is_available(self) -> bool:
        """Check if data is available for this scheduler/type combination."""
        return self.df is not None and len(self.df) > 0

    def get_column(self, candidates: List[str], default: Optional[str] = None) -> Optional[str]:
        """
        Find the first matching column from a list of candidates.

        Args:
            candidates: List of candidate column names (in priority order)
            default: Default column name if none found

        Returns:
            First matching column name, or default if none found

        Example:
            >>> col = accessor.get_column(['avg_pressure', 'mean_pressure', 'pressure'])
            >>> if col:
            ...     values = accessor.df[col].values
        """
        if self.df is None:
            return default

        for col in candidates:
            if col in self.df.columns:
                return col

        return default

    def get_time_series(
        self,
        value_cols: List[str],
        std_cols: Optional[List[str]] = None,
        min_points: int = 2,
        filter_zeros: bool = False,
        filter_outliers: bool = False,
        outlier_iqr_multiplier: float = 3.0
    ) -> Optional[TimeSeriesData]:
        """
        Extract validated time series data with optional std deviation.

        This is the workhorse method that replaces repetitive patterns like:
            - df = data.get(f'{sched.name}_biological')
            - if df is None or len(df) == 0: continue
            - col = find_column(df, ['avg_pressure', 'pressure'])
            - if col is None: continue
            - t = compute_time_seconds(df)
            - values = df[col].values.astype(float)
            - valid = np.isfinite(values) & (values != 0)
            - if np.sum(valid) < min_points: continue

        Args:
            value_cols: Candidate column names to search for (in priority order)
            std_cols: Candidate std deviation column names (optional)
            min_points: Minimum valid points required
            filter_zeros: Whether to filter out zero values
            filter_outliers: Whether to apply IQR-based outlier filtering
            outlier_iqr_multiplier: IQR multiplier for outlier detection (default 3.0)

        Returns:
            TimeSeriesData or None if data unavailable/insufficient
        """
        if not self.is_available():
            return None

        # Find value column
        value_col = self.get_column(value_cols)
        if value_col is None:
            return None

        # Get time series (import compute_time_seconds dynamically to avoid circular imports)
        if self._compute_time_fn is None:
            from ..utils.time_utils import compute_time_seconds
            self._compute_time_fn = compute_time_seconds

        time = self._compute_time_fn(self.df)
        values = self.df[value_col].values.astype(float)

        # Get std if requested
        std_values = None
        if std_cols:
            std_col = self.get_column(std_cols)
            if std_col:
                std_values = self.df[std_col].values.astype(float)

        # Build validity mask
        valid_mask = np.isfinite(values)

        if filter_zeros:
            valid_mask &= (values != 0)

        # Apply outlier filtering if requested
        if filter_outliers:
            from .. import plot_benchmark_unified
            outlier_mask = plot_benchmark_unified.filter_outliers_iqr(
                values, multiplier=outlier_iqr_multiplier
            )
            valid_mask &= outlier_mask

        # Check minimum points
        n_valid = np.sum(valid_mask)
        if n_valid < min_points:
            return None

        # Filter arrays
        time_valid = time[valid_mask]
        values_valid = values[valid_mask]
        std_valid = std_values[valid_mask] if std_values is not None else None

        return TimeSeriesData(
            time=time_valid,
            values=values_valid,
            std=std_valid,
            n_valid=n_valid,
            column_name=value_col
        )

    def get_two_column_series(
        self,
        col1_candidates: List[str],
        col2_candidates: List[str],
        min_points: int = 2,
        filter_zeros: bool = False
    ) -> Optional[Tuple[np.ndarray, np.ndarray, np.ndarray]]:
        """
        Extract two related columns (e.g., cells and time_ms for scaling analysis).

        Args:
            col1_candidates: Candidates for first column (e.g., ['cells', 'cell_count'])
            col2_candidates: Candidates for second column (e.g., ['total_time_ms', 'time_ms'])
            min_points: Minimum valid points
            filter_zeros: Whether to filter zeros

        Returns:
            Tuple of (time, col1_values, col2_values) or None
        """
        if not self.is_available():
            return None

        col1 = self.get_column(col1_candidates)
        col2 = self.get_column(col2_candidates)

        if col1 is None or col2 is None:
            return None

        # Get time (import compute_time_seconds dynamically to avoid circular imports)
        if self._compute_time_fn is None:
            from ..utils.time_utils import compute_time_seconds
            self._compute_time_fn = compute_time_seconds

        time = self._compute_time_fn(self.df)
        val1 = self.df[col1].values.astype(float)
        val2 = self.df[col2].values.astype(float)

        # Filter valid
        valid = np.isfinite(val1) & np.isfinite(val2)
        if filter_zeros:
            valid &= (val1 > 0) & (val2 > 0)

        if np.sum(valid) < min_points:
            return None

        return time[valid], val1[valid], val2[valid]

    def compute_statistics(self, column_candidates: List[str]) -> Optional[StatsSummary]:
        """
        Compute statistical summary for a column.

        Args:
            column_candidates: Candidate column names

        Returns:
            StatsSummary or None if column not found
        """
        if not self.is_available():
            return None

        col = self.get_column(column_candidates)
        if col is None:
            return None

        values = self.df[col].values.astype(float)
        values = values[np.isfinite(values)]

        if len(values) == 0:
            return None

        return StatsSummary(
            mean=float(np.mean(values)),
            std=float(np.std(values)),
            median=float(np.median(values)),
            q25=float(np.percentile(values, 25)),
            q75=float(np.percentile(values, 75)),
            min=float(np.min(values)),
            max=float(np.max(values)),
            n_valid=len(values)
        )

    def get_raw_dataframe(self) -> Optional[pd.DataFrame]:
        """
        Get raw DataFrame for advanced operations.

        Use this sparingly - prefer the accessor methods for type safety.
        """
        return self.df


def create_accessor_for_scheduler(
    data: Dict[str, Any],
    scheduler_name: str
) -> Dict[str, SchedulerDataAccessor]:
    """
    Create accessors for all data types for a given scheduler.

    Args:
        data: Main data dictionary
        scheduler_name: Name of scheduler

    Returns:
        Dict mapping data_type -> accessor

    Example:
        >>> accessors = create_accessor_for_scheduler(data, 'adaptive')
        >>> bio = accessors['biological']
        >>> ts = bio.get_time_series(['avg_pressure', 'pressure'])
    """
    data_types = ['biological', 'computational', 'phase', 'simulation_stats']

    return {
        dtype: SchedulerDataAccessor(data, scheduler_name, dtype)
        for dtype in data_types
    }
