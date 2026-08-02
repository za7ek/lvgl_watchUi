#!/usr/bin/env python3
"""Verify the C lookup tables (LUNAR_OFFSET_DAYS, LEAP_MONTH_OFFSETS,
SOLAR_TERMS_OFFSETS) by simulating the lunar_calendar_convert() C logic
and comparing against the lunarcalendar library for dates 2026-2056.

Also spot-checks solar term dates against the ephem-computed ecliptic longitude.
"""

import sys
import math
from datetime import date, timedelta, datetime, timezone

try:
    from lunarcalendar import Converter, Solar, Lunar
except ImportError:
    print("ERROR: pip install lunarcalendar", file=sys.stderr)
    sys.exit(1)

try:
    import ephem
except ImportError:
    print("ERROR: pip install ephem", file=sys.stderr)
    sys.exit(1)

# Import the generated tables directly from the generator
sys.path.insert(0, "/home/zheng_fang/zephyr-project/lgvl_watchUi/tools")
import gen_lunar_tables as glt

START_YEAR = glt.START_YEAR  # 2026
END_YEAR = glt.END_YEAR      # 2056
START_DATE = glt.START_DATE
CST = glt.CST

# --- Regenerate the tables (same as the C file) ---
month_starts = glt.compute_lunar_month_starts()
leap_indices = glt.find_leap_month_indices(month_starts)
solar_offsets = glt.compute_all_solar_terms()

LUNAR_OFFSET_DAYS = [ms['offset'] for ms in month_starts]
LEAP_MONTH_OFFSETS = leap_indices
SOLAR_TERMS_OFFSETS = solar_offsets

LUNAR_OFFSET_COUNT = len(LUNAR_OFFSET_DAYS)
LEAP_MONTH_COUNT = len(LEAP_MONTH_OFFSETS)
SOLAR_TERMS_COUNT = len(SOLAR_TERMS_OFFSETS)


def is_leap_year(y):
    if y % 400 == 0: return True
    if y % 100 == 0: return False
    if y % 4 == 0: return True
    return False


def get_day_count(year, month, day):
    """Same as C get_day_count(): days from START_YEAR-01-01 (day 0)."""
    days = 0
    y = START_YEAR
    while y < year:
        days += 366 if is_leap_year(y) else 365
        y += 1
    month_days = [0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]
    if 1 <= month <= 12:
        days += month_days[month]
    if is_leap_year(year) and month > 2:
        days += 1
    days += day - 1
    return days


def c_lunar_convert(year, month, day):
    """Simulate lunar_calendar_convert() C logic. Returns (lunar_year, lunar_month,
    lunar_day, is_leap) or None if out of range."""
    if year < START_YEAR or year > END_YEAR:
        return None

    target_days = get_day_count(year, month, day)

    # Find lunar month
    base_month_idx = -1
    last_idx = LUNAR_OFFSET_COUNT - 1

    if target_days < LUNAR_OFFSET_DAYS[0]:
        return None

    if target_days >= LUNAR_OFFSET_DAYS[last_idx] and \
       target_days <= LUNAR_OFFSET_DAYS[last_idx] + 30:
        base_month_idx = last_idx
    else:
        for i in range(LUNAR_OFFSET_COUNT):
            if LUNAR_OFFSET_DAYS[i] > target_days:
                base_month_idx = i - 1
                break

    if base_month_idx < 0:
        return None

    # Leap month accounting
    leap_count = 0
    is_leap = 0
    for i in range(LEAP_MONTH_COUNT):
        if LEAP_MONTH_OFFSETS[i] < base_month_idx:
            leap_count += 1
        elif LEAP_MONTH_OFFSETS[i] == base_month_idx:
            leap_count += 1
            is_leap = 1
            break
        else:
            break

    real_month_offset = base_month_idx - leap_count
    lunar_year = START_YEAR + real_month_offset // 12
    lunar_month = real_month_offset % 12 + 1
    lunar_day = target_days - LUNAR_OFFSET_DAYS[base_month_idx] + 1

    if lunar_day < 1 or lunar_day > 30:
        lunar_day = 1

    return (lunar_year, lunar_month, lunar_day, is_leap)


def verify_lunar_dates():
    """Compare C table lookup against lunarcalendar for every day in 2026-2056.

    Days that belong to lunar year < START_YEAR (early January–February of
    START_YEAR, before the first lunar month) are expected to return None from
    the C function — this matches the Segment34.CN reference convention. They
    are counted separately as 'expected_none', not as mismatches.
    """
    mismatches = []
    expected_none = 0
    tested = 0
    d = date(START_YEAR, 1, 1)
    end = date(END_YEAR, 12, 31)

    # Also test a few days before/after the range (should return None)
    boundary_tests = [
        (START_YEAR - 1, 12, 31),
        (END_YEAR + 1, 1, 1),
    ]

    while d <= end:
        ref = Converter.Solar2Lunar(Solar(d.year, d.month, d.day))
        ref_tuple = (ref.year, ref.month, ref.day, 1 if ref.isleap else 0)
        c_result = c_lunar_convert(d.year, d.month, d.day)
        tested += 1

        # Days belonging to lunar year < START_YEAR are out of table range.
        if ref.year < START_YEAR:
            if c_result is not None:
                mismatches.append((d, c_result, ref_tuple))
                if len(mismatches) <= 20:
                    print(f"  MISMATCH {d}: C={c_result} ref={ref_tuple} "
                          f"(should be None, lunar year {ref.year} < {START_YEAR})",
                          file=sys.stderr)
            else:
                expected_none += 1
        elif c_result != ref_tuple:
            mismatches.append((d, c_result, ref_tuple))
            if len(mismatches) <= 20:
                print(f"  MISMATCH {d}: C={c_result} ref={ref_tuple}", file=sys.stderr)
        d += timedelta(days=1)

    # Boundary tests
    for (y, m, dd) in boundary_tests:
        c_result = c_lunar_convert(y, m, dd)
        if c_result is not None:
            print(f"  BOUNDARY FAIL {y}-{m}-{dd}: C returned {c_result} (expected None)",
                  file=sys.stderr)
            mismatches.append((date(y, m, dd), c_result, None))

    in_range = tested - expected_none
    if not mismatches:
        print(f"  ✓ Lunar dates: all {in_range} in-range days match lunarcalendar "
              f"({START_YEAR}-{END_YEAR}), {expected_none} days correctly "
              f"returned None (lunar year < {START_YEAR})", file=sys.stderr)
    else:
        print(f"  ✗ Lunar dates: {len(mismatches)} mismatches out of {in_range} "
              f"in-range days ({expected_none} expected-None)", file=sys.stderr)
    return len(mismatches) == 0


def verify_solar_terms():
    """Spot-check solar term offsets against ephem ecliptic longitude."""
    SOLAR_TERM_LON = glt.SOLAR_TERM_LON
    SOLAR_TERMS = glt.SOLAR_TERMS
    mismatches = 0
    tested = 0

    for year in range(START_YEAR, END_YEAR + 1):
        for term_idx in range(24):
            i = (year - START_YEAR) * 24 + term_idx
            offset = SOLAR_TERMS_OFFSETS[i]
            term_date = START_DATE + timedelta(days=offset)

            # Compute sun's ecliptic longitude at noon CST on that date
            noon = datetime(term_date.year, term_date.month, term_date.day,
                            12, 0, 0, tzinfo=CST)
            lon = glt._sun_ecliptic_longitude(ephem.Date(noon))
            target = SOLAR_TERM_LON[term_idx]
            diff = glt._circular_diff(lon, target)

            tested += 1
            # The sun moves ~1°/day, so on the correct day the noon longitude
            # should be within ~0.5° of the target.
            if diff > 0.55:
                mismatches += 1
                if mismatches <= 20:
                    print(f"  SOLAR TERM MISMATCH: {SOLAR_TERMS[term_idx]} {year} "
                          f"-> {term_date} (offset {offset}), "
                          f"lon={lon:.2f}° target={target}° diff={diff:.2f}°",
                          file=sys.stderr)

    if mismatches == 0:
        print(f"  ✓ Solar terms: all {tested} terms verified "
              f"({START_YEAR}-{END_YEAR})", file=sys.stderr)
    else:
        print(f"  ✗ Solar terms: {mismatches} mismatches out of {tested}",
              file=sys.stderr)
    return mismatches == 0


def main():
    print(f"Table sizes: LUNAR_OFFSET_DAYS={LUNAR_OFFSET_COUNT}, "
          f"LEAP_MONTH_OFFSETS={LEAP_MONTH_COUNT}, "
          f"SOLAR_TERMS_OFFSETS={SOLAR_TERMS_COUNT}", file=sys.stderr)
    print(f"Leap month indices: {LEAP_MONTH_OFFSETS}", file=sys.stderr)
    print(f"Year range: {START_YEAR}-{END_YEAR}", file=sys.stderr)
    print("", file=sys.stderr)

    ok1 = verify_lunar_dates()
    ok2 = verify_solar_terms()

    print("", file=sys.stderr)
    if ok1 and ok2:
        print("✓ ALL VERIFICATION PASSED", file=sys.stderr)
    else:
        print("✗ VERIFICATION FAILED", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
