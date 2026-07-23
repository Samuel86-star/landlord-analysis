#!/usr/bin/env python3
"""通用按天回填工具，供各 batch_insert_*.py 复用。

使用模式：
    from backfill_runner import run_backfill

    run_backfill(
        table_label="dws_xxx",
        delete_template="DELETE FROM ... WHERE ... = '{dt}'",
        insert_template="INSERT INTO ... WHERE ... = '{dt}' ...",
        check_template="SELECT COUNT(*) AS cnt FROM ... WHERE ... = '{dt}'",
        default_app_id=1880053,
        depends_on=("dws_xxx", "dws_yyy"),
    )

模板里可用的占位符：{dt}（YYYY-MM-DD）、{dt_int}（YYYYMMDD 整数）、{dt_next_int}（次日 YYYYMMDD 整数，供跨天扫描用）、{app_id}。
"""
import argparse
import sys
import time
from datetime import datetime, timedelta

from sr_exec import StarRocksClient


def parse_dt(s):
    s = s.strip()
    for fmt in ("%Y%m%d", "%Y-%m-%d"):
        try:
            return datetime.strptime(s, fmt).date()
        except ValueError:
            continue
    raise argparse.ArgumentTypeError(
        "日期格式不支持: {}（请使用 YYYYMMDD 或 YYYY-MM-DD）".format(s)
    )


def daterange(start, end):
    for n in range((end - start).days + 1):
        yield start + timedelta(n)


def parse_args(table_label, default_app_id):
    parser = argparse.ArgumentParser(
        description="按日期批量回填 {}".format(table_label)
    )
    parser.add_argument("--start", type=parse_dt, help="起始日期 YYYYMMDD 或 YYYY-MM-DD")
    parser.add_argument("--end", type=parse_dt, help="截止日期 YYYYMMDD 或 YYYY-MM-DD（含）")
    parser.add_argument(
        "--app-id",
        type=int,
        default=default_app_id,
        help="应用 ID（默认 {}）".format(default_app_id),
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="只打印将要执行的 SQL，不实际执行"
    )
    args = parser.parse_args()

    if args.start is None:
        args.start = parse_dt(input("请输入起始日期 (YYYYMMDD 或 YYYY-MM-DD): "))
    if args.end is None:
        args.end = parse_dt(input("请输入截止日期 (YYYYMMDD 或 YYYY-MM-DD): "))
    if args.end < args.start:
        parser.error("截止日期不能早于起始日期")
    return args


def run_backfill(
    table_label,
    delete_template,
    insert_template,
    check_template,
    default_app_id=1880053,
    depends_on=(),
):
    args = parse_args(table_label, default_app_id)

    all_dates = list(daterange(args.start, args.end))
    total = len(all_dates)
    print("=" * 60)
    print("{} backfill".format(table_label))
    print(
        "Range: {} to {}, {} days, app_id={}".format(
            args.start, args.end, total, args.app_id
        )
    )
    if depends_on:
        print("依赖：{} 对应日期需已回填".format("、".join(depends_on)))
    if args.dry_run:
        print("(dry-run 模式：仅打印 SQL，不执行)")
    print("=" * 60)

    if args.dry_run:
        for d in all_dates:
            dt = d.strftime("%Y-%m-%d")
            dt_int = int(d.strftime("%Y%m%d"))
            dt_next_int = int((d + timedelta(days=1)).strftime("%Y%m%d"))
            print("-- {} --".format(dt))
            print(delete_template.format(dt=dt, dt_int=dt_int, dt_next_int=dt_next_int, app_id=args.app_id) + ";")
            print(insert_template.format(dt=dt, dt_int=dt_int, dt_next_int=dt_next_int, app_id=args.app_id) + ";")
        return

    client = StarRocksClient()
    client.login().connect()

    success = 0
    for i, d in enumerate(all_dates, 1):
        dt = d.strftime("%Y-%m-%d")
        dt_int = int(d.strftime("%Y%m%d"))
        dt_next_int = int((d + timedelta(days=1)).strftime("%Y%m%d"))
        t0 = time.time()
        try:
            client.execute_write(
                delete_template.format(dt=dt, dt_int=dt_int, dt_next_int=dt_next_int, app_id=args.app_id)
            )
            client.execute_write(
                insert_template.format(dt=dt, dt_int=dt_int, dt_next_int=dt_next_int, app_id=args.app_id)
            )
            check_result = client.execute(
                check_template.format(dt=dt, dt_int=dt_int, dt_next_int=dt_next_int, app_id=args.app_id)
            )
            rows = (
                check_result.get("results", [{}])[0]
                .get("resultSet", {})
                .get("rows", [["?"]])[0][0]
            )
            elapsed = time.time() - t0
            print(
                "[{:3d}/{}] {}  OK  ({} rows, {:.1f}s)".format(
                    i, total, dt, rows, elapsed
                )
            )
            success += 1
        except Exception as e:
            elapsed = time.time() - t0
            print("[{:3d}/{}] {}  FAIL  ({:.1f}s)".format(i, total, dt, elapsed))
            print("  Error: {}".format(str(e)[:300]))
            print("Stopping due to error.")
            sys.exit(1)

    print("=" * 60)
    print("All done! {}/{}".format(success, total))
