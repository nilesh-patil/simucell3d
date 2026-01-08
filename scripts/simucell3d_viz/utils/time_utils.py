"""Time series utilities for SimuCell3D benchmark visualization.

This module provides time computation utilities to avoid circular imports.
"""

from typing import List, Optional
import numpy as np
import pandas as pd


def find_column(df: pd.DataFrame, candidates: List[str], default: str = None) -> Optional[str]:
    """Find the first matching column from a list of candidates."""
    for col in candidates:
        if col in df.columns:
            return col
    return default


def compute_time_seconds(df: pd.DataFrame, col: str = 'timestamp') -> np.ndarray:
    """Convert wall-clock timestamp column to seconds since simulation process started.

    IMPORTANT: This returns WALL-CLOCK time (how long the simulation process has been running),
    NOT simulation/biological time. The timestamp column contains ISO format wall-clock
    times when each data point was recorded.

    Args:
        df: DataFrame containing timestamp data
        col: Column name for wall-clock timestamps (default: 'timestamp')

    Returns:
        Array of seconds since the first timestamp (wall-clock elapsed time)
    """
    if df is None or len(df) == 0:
        return np.array([0.0])

    n_rows = len(df)

    # Priority order for wall-clock time columns (NOT simulation_time which is biological)
    wall_clock_cols = [col, 'wall_time_sec', 'elapsed_time_sec', 'process_time_sec']

    timestamps = None

    for try_col in wall_clock_cols:
        if try_col in df.columns:
            if try_col in ['wall_time_sec', 'elapsed_time_sec', 'process_time_sec']:
                # Already in seconds
                return df[try_col].values.astype(float)
            timestamps = df[try_col]
            break

    if timestamps is None:
        # No wall-clock timestamp found - estimate from cumulative iteration times
        time_col = find_column(df, ['total_time_ms', 'time_ms', 'iter_time_ms'])
        if time_col:
            # Cumulative sum of iteration times gives wall-clock estimate
            times = df[time_col].values.astype(float)
            times = np.nan_to_num(times, nan=0.0)
            cumulative_ms = np.cumsum(times)
            return cumulative_ms / 1000.0  # ms to seconds
        # Last resort: return index-based array
        return np.arange(n_rows).astype(float)

    # Parse ISO timestamp strings to datetime
    if timestamps.dtype == 'object':
        for fmt in ['%Y-%m-%d %H:%M:%S.%f', '%Y-%m-%d_%H:%M:%S', '%Y-%m-%d %H:%M:%S']:
            try:
                timestamps = pd.to_datetime(timestamps, format=fmt, errors='coerce')
                if not timestamps.isna().all():
                    break
            except Exception:
                continue

    # Calculate seconds since first timestamp (wall-clock elapsed time)
    if hasattr(timestamps, 'dt') or pd.api.types.is_datetime64_any_dtype(timestamps):
        start_time = timestamps.min()
        if pd.isna(start_time):
            # Fallback to index-based
            return np.arange(n_rows).astype(float)
        result = (timestamps - start_time).dt.total_seconds()
        return result.fillna(0).values

    # Final fallback
    return np.arange(n_rows).astype(float)
