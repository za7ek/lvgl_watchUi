#!/usr/bin/env python3
"""Generate LUNAR_OFFSET_DAYS, LEAP_MONTH_OFFSETS, and SOLAR_TERMS_OFFSETS tables
for the C lunar_calendar.c, covering years 2026-2056.

Uses:
  - ephem library for accurate new moon dates (astronomical computation).
  - lunarcalendar library for lunar year/month numbering and leap-month detection.
  - Solar term formula (寿星通用公式, 21st century) for solar term dates.

The output is verified against the Segment34.CN reference data (2026-2030).
"""

import sys
import math
from datetime import date, timedelta, datetime, timezone

try:
    import ephem
except ImportError:
    print("ERROR: pip install ephem", file=sys.stderr)
    sys.exit(1)

try:
    from lunarcalendar import Converter, Solar
except ImportError:
    print("ERROR: pip install lunarcalendar", file=sys.stderr)
    sys.exit(1)

START_YEAR = 2026
END_YEAR = 2056
START_DATE = date(START_YEAR, 1, 1)
CST = timezone(timedelta(hours=8))  # China Standard Time


# ─────────────────────────────────────────────────────────────────────────────
# Solar terms
# ─────────────────────────────────────────────────────────────────────────────

# 24 solar terms in order, starting from 小寒 (first term of the year).
# The formula (寿星通用公式) is only used as an initial estimate; the exact date
# is then refined using astronomical computation (ephem) by finding the day when
# the Sun's apparent geocentric ecliptic longitude equals the target value.
SOLAR_TERMS = [
    "小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明", "谷雨",
    "立夏", "小满", "芒种", "夏至", "小暑", "大暑", "立秋", "处暑",
    "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至",
]
SOLAR_TERM_MONTH = [1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12]
# C constant for the 21st century (years 2000-2099), used only for the estimate.
SOLAR_TERM_C = [
    5.4055, 20.12,  3.87,  18.73,  5.63,  20.646, 4.81,  20.1,
    5.52,   21.04,  5.678, 21.37,  7.108, 22.83,  7.5,   23.13,
    7.646,  23.042, 8.318, 23.439, 7.438, 22.36,  7.18,  22.6,
]
# Target ecliptic longitude (degrees) for each of the 24 solar terms.
# 小寒 = 285°, then +15° per term, wrapping past 360°.
SOLAR_TERM_LON = [(285 + i * 15) % 360 for i in range(24)]


def _sun_ecliptic_longitude(ephem_date):
    """Sun's apparent geocentric ecliptic longitude in degrees [0, 360).

    Converts from equatorial (ra, dec) to ecliptic (lambda) using the mean
    obliquity of the ecliptic. The obliquity varies by <0.00001°/year, so a
    fixed value (23.4393°, J2000 mean) is more than accurate enough for
    ±1-day solar term determination (the Sun moves ~1°/day).
    """
    sun = ephem.Sun(ephem_date)
    ra = float(sun.ra)
    dec = float(sun.dec)
    eps = math.radians(23.4393)
    lam = math.atan2(
        math.sin(ra) * math.cos(eps) + math.tan(dec) * math.sin(eps),
        math.cos(ra),
    )
    if lam < 0:
        lam += 2 * math.pi
    return math.degrees(lam)


def _circular_diff(a, b):
    """Smallest absolute difference |a - b| considering 360° wraparound."""
    d = abs(a - b) % 360.0
    if d > 180.0:
        d = 360.0 - d
    return d


def compute_solar_term_date(year, term_index):
    """Compute the exact Gregorian date of a solar term.

    Uses the 寿星 formula for an initial estimate, then refines by finding the
    day (at noon CST) whose Sun ecliptic longitude is closest to the target.
    """
    y = year - 2000
    c = SOLAR_TERM_C[term_index]
    day_est = int(0.2422 * y + c) - int(y / 4)
    month = SOLAR_TERM_MONTH[term_index]
    approx = date(year, month, day_est)

    target = SOLAR_TERM_LON[term_index]
    best_date = approx
    best_diff = 999.0
    for delta in range(-2, 3):
        d = approx + timedelta(days=delta)
        d_noon = datetime(d.year, d.month, d.day, 12, 0, 0, tzinfo=CST)
        lon = _sun_ecliptic_longitude(ephem.Date(d_noon))
        diff = _circular_diff(lon, target)
        if diff < best_diff:
            best_diff = diff
            best_date = d
    return best_date


def compute_all_solar_terms():
    """Compute all solar term day-offsets from START_DATE for 2026-2056."""
    offsets = []
    for year in range(START_YEAR, END_YEAR + 1):
        for term_idx in range(24):
            d = compute_solar_term_date(year, term_idx)
            offset = (d - START_DATE).days
            offsets.append(offset)
    return offsets


# ─────────────────────────────────────────────────────────────────────────────
# Lunar month starts (using ephem for new moons)
# ─────────────────────────────────────────────────────────────────────────────

def find_new_moons(d_start, d_end):
    """Find all new moon dates (in CST) between d_start and d_end (inclusive)."""
    new_moons = []
    observer = ephem.Observer()
    observer.date = ephem.Date(datetime(d_start.year, d_start.month, d_start.day, 0, 0, 0))

    end_jd = ephem.Date(datetime(d_end.year, d_end.month, d_end.day, 23, 59, 59))

    while observer.date < end_jd:
        nm = ephem.next_new_moon(observer.date)
        # Convert UTC → CST, take the date part
        nm_utc = nm.datetime()
        nm_cst = nm_utc.astimezone(CST)
        nm_date = nm_cst.date()
        if d_start <= nm_date <= d_end:
            new_moons.append(nm_date)
        observer.date = nm + 0.01  # advance past this new moon

    return new_moons


def compute_lunar_month_starts():
    """Find all lunar month starts belonging to lunar years START_YEAR..END_YEAR.

    Returns a list of dicts:
      {offset, solar_date, lunar_year, lunar_month, is_leap}

    A lunar month is included only if its lunar_year is within
    [START_YEAR, END_YEAR]. This excludes the 腊月 of the previous lunar year
    that begins in early January of START_YEAR (it belongs to lunar year
    START_YEAR-1), matching the Segment34.CN reference table convention.
    """
    # New moons from late 2025 (catch first lunar month of lunar year 2026)
    # through early 2057 (catch last lunar month of lunar year 2056).
    nm_start = date(START_YEAR - 1, 11, 1)
    nm_end = date(END_YEAR + 1, 2, 28)

    new_moons = find_new_moons(nm_start, nm_end)
    print(f"  Found {len(new_moons)} new moons", file=sys.stderr)

    month_starts = []
    for nm in new_moons:
        lunar = Converter.Solar2Lunar(Solar(nm.year, nm.month, nm.day))

        # If the new-moon day is not lunar day 1, the lunar month actually
        # starts the following day (new moon late in the day).
        if lunar.day != 1:
            nm2 = nm + timedelta(days=1)
            lunar2 = Converter.Solar2Lunar(Solar(nm2.year, nm2.month, nm2.day))
            if lunar2.day == 1:
                lunar = lunar2
                nm = nm2
            else:
                continue

        # Only include months belonging to lunar years START_YEAR..END_YEAR.
        if lunar.year < START_YEAR or lunar.year > END_YEAR:
            continue

        offset = (nm - START_DATE).days
        month_starts.append({
            'offset': offset,
            'solar_date': nm,
            'lunar_year': lunar.year,
            'lunar_month': lunar.month,
            'is_leap': lunar.isleap,
        })

    return month_starts


def find_leap_month_indices(month_starts):
    """Find indices in month_starts that are leap months."""
    leap_indices = []
    for i, ms in enumerate(month_starts):
        if ms['is_leap']:
            leap_indices.append(i)
    return leap_indices


# ─────────────────────────────────────────────────────────────────────────────
# Verification against Segment34.CN reference data (2026-2030)
# ─────────────────────────────────────────────────────────────────────────────

REF_LUNAR_OFFSETS = [
    47, 77, 106, 136, 165, 194, 224, 253, 282, 312, 342, 372,
    401, 431, 461, 490, 520, 549, 578, 608, 637, 666, 696, 726,
    755, 785, 815, 845, 874, 904, 933, 962, 992, 1021, 1050, 1080, 1110,
    1139, 1169, 1199, 1228, 1258, 1287, 1317, 1346, 1376, 1405, 1434, 1464,
    1494, 1523, 1553, 1582, 1612, 1642, 1671, 1701, 1730, 1760, 1789, 1819,
]
REF_SOLAR_OFFSETS = [
    4, 19, 34, 48, 63, 78, 94, 109, 124, 140, 155, 171, 187, 203, 218, 234, 249, 265, 280, 295, 310, 325, 340, 355,
    369, 384, 399, 414, 429, 444, 459, 474, 490, 505, 521, 536, 552, 568, 584, 599, 615, 630, 645, 660, 675, 690, 705, 720,
    735, 749, 764, 779, 794, 809, 824, 839, 855, 870, 886, 902, 917, 933, 949, 964, 980, 995, 1011, 1026, 1041, 1056, 1070, 1085,
    1100, 1115, 1129, 1144, 1159, 1174, 1189, 1205, 1220, 1236, 1251, 1267, 1283, 1298, 1314, 1330, 1345, 1361, 1376, 1391, 1406, 1421, 1436, 1450,
    1465, 1480, 1495, 1509, 1524, 1539, 1555, 1570, 1585, 1601, 1616, 1632, 1648, 1664, 1679, 1695, 1710, 1726, 1741, 1756, 1771, 1786, 1801, 1816,
]


def verify(month_starts, solar_offsets):
    """Verify computed values against the Segment34.CN reference."""
    ok = True

    # Check lunar offsets (first 61 entries)
    computed_lunar = [ms['offset'] for ms in month_starts[:len(REF_LUNAR_OFFSETS)]]
    if computed_lunar == REF_LUNAR_OFFSETS:
        print("  ✓ Lunar month starts match reference (2026-2030)", file=sys.stderr)
    else:
        print("  ✗ Lunar month starts MISMATCH!", file=sys.stderr)
        for i, (c, r) in enumerate(zip(computed_lunar, REF_LUNAR_OFFSETS)):
            if c != r:
                print(f"    [{i}] computed={c} reference={r}", file=sys.stderr)
        ok = False

    # Check solar term offsets (first 120 entries)
    computed_solar = solar_offsets[:len(REF_SOLAR_OFFSETS)]
    if computed_solar == REF_SOLAR_OFFSETS:
        print("  ✓ Solar term offsets match reference (2026-2030)", file=sys.stderr)
    else:
        print("  ✗ Solar term offsets MISMATCH!", file=sys.stderr)
        for i, (c, r) in enumerate(zip(computed_solar, REF_SOLAR_OFFSETS)):
            if c != r:
                print(f"    [{i}] computed={c} reference={r}", file=sys.stderr)
        ok = False

    return ok


# ─────────────────────────────────────────────────────────────────────────────
# C code generation
# ─────────────────────────────────────────────────────────────────────────────

def emit_c_tables(month_starts, leap_indices, solar_offsets):
    """Generate C code for the lookup tables."""
    lines = []
    lines.append("/*")
    lines.append(" * Auto-generated lunar calendar lookup tables (2026-2056).")
    lines.append(" * Source: tools/gen_lunar_tables.py")
    lines.append(" *")
    lines.append(f" * Lunar month starts: {len(month_starts)} entries")
    lines.append(f" * Leap months: {leap_indices}")
    lines.append(f" * Solar terms: {len(solar_offsets)} entries ({(END_YEAR-START_YEAR+1)*24} = {(END_YEAR-START_YEAR+1)} years x 24)")
    lines.append(" */")
    lines.append("")

    # LUNAR_OFFSET_DAYS
    lines.append(f"/* Day offsets from {START_YEAR}-01-01 to the 1st day of each lunar month.")
    lines.append(f" * {len(month_starts)} entries covering {START_YEAR}-{END_YEAR}. */")
    lines.append("static const uint16_t LUNAR_OFFSET_DAYS[] = {")
    for i in range(0, len(month_starts), 8):
        chunk = month_starts[i:i+8]
        vals = ", ".join(f"{ms['offset']:>5d}" for ms in chunk)
        comma = "," if i + 8 < len(month_starts) else ""
        lines.append(f"    {vals}{comma}")
    lines.append("};")
    lines.append("")

    # LEAP_MONTH_OFFSETS
    leap_vals = ", ".join(str(i) for i in leap_indices)
    lines.append(f"/* Indices in LUNAR_OFFSET_DAYS that are leap months. */")
    lines.append(f"static const uint8_t LEAP_MONTH_OFFSETS[] = {{{leap_vals}}};")
    lines.append("")

    # SOLAR_TERMS_OFFSETS
    lines.append(f"/* Day offsets from {START_YEAR}-01-01 to each solar term.")
    lines.append(f" * {len(solar_offsets)} entries ({END_YEAR-START_YEAR+1} years x 24 terms). */")
    lines.append("static const uint16_t SOLAR_TERMS_OFFSETS[] = {")
    for i in range(0, len(solar_offsets), 8):
        chunk = solar_offsets[i:i+8]
        vals = ", ".join(f"{v:>5d}" for v in chunk)
        comma = "," if i + 8 < len(solar_offsets) else ""
        lines.append(f"    {vals}{comma}")
    lines.append("};")
    lines.append("")

    return "\n".join(lines)


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main():
    print("Computing lunar month starts (new moons)...", file=sys.stderr)
    month_starts = compute_lunar_month_starts()
    print(f"  Found {len(month_starts)} lunar month starts", file=sys.stderr)

    print("Computing solar terms...", file=sys.stderr)
    solar_offsets = compute_all_solar_terms()
    print(f"  Found {len(solar_offsets)} solar terms", file=sys.stderr)

    leap_indices = find_leap_month_indices(month_starts)
    print(f"  Leap months at indices: {leap_indices}", file=sys.stderr)

    print("\nVerification:", file=sys.stderr)
    if verify(month_starts, solar_offsets):
        print("\n✓ All verification passed!", file=sys.stderr)
    else:
        print("\n✗ Verification FAILED - check output", file=sys.stderr)

    # Print sample
    print("\n=== Sample lunar months ===", file=sys.stderr)
    for ms in month_starts[:5]:
        leap_str = " (leap)" if ms['is_leap'] else ""
        print(f"  offset={ms['offset']:>5d}  {ms['solar_date']}  lunar={ms['lunar_year']}-{ms['lunar_month']}{leap_str}", file=sys.stderr)
    print("  ...", file=sys.stderr)
    for ms in month_starts[-3:]:
        leap_str = " (leap)" if ms['is_leap'] else ""
        print(f"  offset={ms['offset']:>5d}  {ms['solar_date']}  lunar={ms['lunar_year']}-{ms['lunar_month']}{leap_str}", file=sys.stderr)

    # Print all leap months
    print("\n=== All leap months ===", file=sys.stderr)
    for idx in leap_indices:
        ms = month_starts[idx]
        print(f"  index={idx}  offset={ms['offset']:>5d}  {ms['solar_date']}  lunar={ms['lunar_year']}-{ms['lunar_month']} (leap)", file=sys.stderr)

    # Generate C code
    print("\n=== Generated C tables ===\n")
    c_code = emit_c_tables(month_starts, leap_indices, solar_offsets)
    print(c_code)

    # Also write to file
    with open("/tmp/lunar_tables_generated.c", "w") as f:
        f.write(c_code)
    print(f"\n(Written to /tmp/lunar_tables_generated.c)", file=sys.stderr)


if __name__ == "__main__":
    main()
