# 农历与节气数据文档

> 本文档讲农历查表数据的**生成、校验、扩展**。
> 农历 API 用法和 `lunar_calendar.c` 架构见 [PROJECT.md](PROJECT.md) §8。

---

## 1. 概览

`lunar_calendar.c` 用三张静态表把公历日期转换成农历日期和节气，
**不依赖任何天文运算库**，在 MCU 上纯查表执行。

| 表 | 数组名 | 项数 | 说明 |
|----|--------|------|------|
| 农历月首偏移 | `LUNAR_OFFSET_DAYS[]` | 383 | 每个农历月1日相对 2026-01-01 的天数 |
| 闰月索引 | `LEAP_MONTH_OFFSETS[]` | 11 | `LUNAR_OFFSET_DAYS` 里哪些下标是闰月 |
| 节气偏移 | `SOLAR_TERMS_OFFSETS[]` | 744 | 31 年 × 24 节气相对 2026-01-01 的天数 |

覆盖范围：**2026-01-01 ~ 2056-12-31**（超出返回空串）。

---

## 2. 数据生成

### 2.1 依赖

```bash
cd ~/zephyr-project && source .venv/bin/activate
pip install ephem lunarcalendar
```

| 库 | 用途 |
|----|------|
| `ephem` | 天文精确朔望（新月时刻）+ 太阳黄经计算 |
| `lunarcalendar` | 农历年/月编号和闰月判断 |

### 2.2 运行生成器

```bash
cd ~/zephyr-project && source .venv/bin/activate
python3 lgvl_watchUi/tools/gen_lunar_tables.py
```

脚本会把三张表直接写进 `app/src/components/lunar_calendar.c`（覆盖原有内容）。
同时会在终端打印内置的快速校验结果（对比 Segment34.CN 参考数据 2026-2030）。

### 2.3 算法

**农历月首（`LUNAR_OFFSET_DAYS`）**

用 `ephem` 找 2025-11-01 至 2057-02-28 之间所有新月时刻（UTC+8），
得到每个农历月1日的公历日期；再用 `lunarcalendar` 判断每月属于哪个农历年、
是否为闰月，筛出属于 2026-2056 农历年的月份共 383 项（含 11 个闰月）。

**节气（`SOLAR_TERMS_OFFSETS`）**

24 节气从小寒开始，对应太阳黄经 285° 起每 15° 一个节气（共 360°）。
以寿星通用公式算初估日期，再用 `ephem` 精确迭代（二分搜索太阳黄经），
精度优于半天，远高于节气日期所需的 1 天精度。

### 2.4 关键常量（`gen_lunar_tables.py` 开头）

```python
START_YEAR = 2026
END_YEAR   = 2056
START_DATE = date(START_YEAR, 1, 1)   # 所有偏移量的基准日
CST = timezone(timedelta(hours=8))    # 北京时间（朔望以 UTC+8 为准）
```

要扩展年份范围，修改 `END_YEAR` 后重新运行即可。

---

## 3. 数据校验

### 3.1 内置快速校验（生成时自动执行）

`gen_lunar_tables.py` 的 `verify()` 函数把生成的表与 Segment34.CN 参考数据
（2026-2030 年部分农历月首和节气）逐项对比，任何偏差都会打印错误并以非零退出。

### 3.2 独立逐日校验

```bash
cd ~/zephyr-project && source .venv/bin/activate
python3 lgvl_watchUi/tools/verify_lunar_tables.py
```

脚本用 `lunarcalendar` 把 2026-01-01 至 2056-12-31 共 11315 天逐日换算，
与 `lunar_calendar.c` 的查表结果对比，输出不一致的条目数。
全部通过则打印 `All OK`。

---

## 4. 查表 C API

```c
#include "lunar_calendar.h"

lunar_date_t ld;
lunar_calendar_convert(2026, 7, 25, &ld);

/* 结果：
 *   ld.year_name  = "丙午年"
 *   ld.month_name = "六月"
 *   ld.day_name   = "初二"
 *   ld.jieqi      = "大暑"     // 当天无节气则为 ""
 */
```

超出 2026-2056 范围时，所有字段返回空串 `""`。

### `lunar_date_t` 字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `year_name` | `const char *` | 天干地支年名，如 "丙午年" |
| `month_name` | `const char *` | 农历月名，如 "六月"（闰月加前缀 "闰"） |
| `day_name` | `const char *` | 农历日名，如 "初二"、"二十"、"初一" |
| `jieqi` | `const char *` | 当天节气名；无节气返回 `""` |

---

## 5. 与月相的关系

**月相不走这张表**。`watchface.c` 的 `get_moon_phase()` 用儒略日 +
朔望月整数运算独立计算，精度比参考实现高一个数量级：
2026-2056 共 11315 天中，误差 ≤0.6%（参考实现约 12.2%）。

农历月首表和月相计算是**独立**的两套逻辑，互不干扰。

---

## 6. 已知限制

| 项 | 说明 |
|----|------|
| 覆盖年限 | 2026-2056（31 年）；扩展见 §2.4 |
| 节气精度 | 日级别（精确到哪一天，不精确到时刻） |
| 时区假设 | 全部以 UTC+8（北京时间）为准，与中国农历通用标准一致 |
| 农历正确性 | 依赖 `lunarcalendar` 库；极少数边界月份（闰月附近）可能与民间历书差 1 天 |
