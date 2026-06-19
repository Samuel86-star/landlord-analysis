#!/usr/bin/env python3
"""每日数据回填调度器：按依赖分层串行调用各 batch_insert_*.py。

覆盖 account + game + currency 三域共 15 张表（不含 retention 域，retention 见 daily_retention.py）。

分层（层内串行，层间串行，按天循环）：
    L1 无依赖     : daily_reg, daily_login, ddz_daily_game_raw, crazyddz_daily_game_raw, prop_log
    L2 依赖 L1    : app_daily_reg, ddz_daily_game, crazyddz_daily_game, dq_silver_logs
    L3 依赖 L2    : app_game_active, app_gamemode_active, silvergame_stat, scoregame_stat, allgame_stat, ddz_firstday_game

用法：
    py -3 -u .\\daily_backfill.py --start 20260617 --end 20260617
    py -3 -u .\\daily_backfill.py --start 20260617 --end 20260617 --layer 2   # 只跑第2层
    py -3 -u .\\daily_backfill.py --start 20260617 --end 20260617 --dry-run
    py -3 -u .\\daily_backfill.py --start 2026-06-01 --end 2026-06-08
"""
import argparse
import os
import re
import subprocess
import sys
from datetime import datetime, timedelta

HERE = os.path.dirname(os.path.abspath(__file__))

# 分层清单：(层号, [脚本名...])，层内顺序即执行顺序
LAYERS = [
    (1, [
        "batch_insert_daily_reg",
        "batch_insert_daily_login",
        "batch_insert_ddz_daily_game_raw",
        "batch_insert_crazyddz_daily_game_raw",
        "batch_insert_prop_log",
    ]),
    (2, [
        "batch_insert_app_daily_reg",
        "batch_insert_ddz_daily_game",
        "batch_insert_crazyddz_daily_game",
        "batch_insert_dq_silver_logs",
    ]),
    (3, [
        "batch_insert_app_game_active",
        "batch_insert_app_gamemode_active",
        "batch_insert_app_silvergame_stat",
        "batch_insert_app_scoregame_stat",
        "batch_insert_allgame_stat",
        "batch_insert_ddz_firstday_game",
    ]),
]

ROWS_RE = re.compile(r"OK\s+\((\d+)\s+rows")


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


def run_script(script_name, dt_str, dry_run, log_file):
    """subprocess 调用一个 batch 脚本，返回 (ok, rows_or_None, stdout)。

    Windows 下 Python 子进程的 stdout 默认是系统编码（GBK），但子脚本里有中文输出。
    通过 PYTHONIOENCODING=utf-8 强制子进程用 UTF-8 写 stdout；本地用 utf-8 读，
    再加 errors='replace' 防止个别异常字节让 reader thread 崩。
    """
    script_path = os.path.join(HERE, script_name + ".py")
    cmd = [sys.executable, "-u", script_path, "--start", dt_str, "--end", dt_str]
    if dry_run:
        cmd.append("--dry-run")
    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"
    proc = subprocess.run(
        cmd, capture_output=True, text=True,
        encoding="utf-8", errors="replace", env=env,
    )
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    full = stdout + ("\n" + stderr if stderr.strip() else "")
    if log_file:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write("$ {}\n".format(" ".join(cmd)))
            f.write(full)
            f.write("\n" + "-" * 60 + "\n")
    ok = proc.returncode == 0
    rows = None
    m = ROWS_RE.search(stdout)
    if m:
        rows = int(m.group(1))
    return ok, rows, full


def main():
    parser = argparse.ArgumentParser(description="每日数据回填调度器（15 张表，3 层）")
    parser.add_argument("--start", type=parse_dt, required=True, help="起始日期 YYYYMMDD 或 YYYY-MM-DD")
    parser.add_argument("--end", type=parse_dt, required=True, help="截止日期 YYYYMMDD 或 YYYY-MM-DD（含）")
    parser.add_argument("--layer", type=int, choices=[1, 2, 3], help="只跑指定层（1/2/3），不指定则跑全部")
    parser.add_argument("--dry-run", action="store_true", help="只打印执行计划与子脚本 SQL，不实际写库")
    args = parser.parse_args()
    if args.end < args.start:
        parser.error("截止日期不能早于起始日期")

    layers = [(n, scripts) for n, scripts in LAYERS if (args.layer is None or n == args.layer)]
    all_dates = list(daterange(args.start, args.end))

    total_tables = sum(len(s) for _, s in layers) * len(all_dates)
    print("=" * 60)
    print("daily_backfill {} -> {}, {} day(s), {} table(s)/day".format(
        args.start, args.end, len(all_dates), sum(len(s) for _, s in layers)))
    print("layers: {}".format(" -> ".join("L{}".format(n) for n, _ in layers)))
    if args.dry_run:
        print("(dry-run：仅打印子脚本 SQL，不写库)")
    print("=" * 60)

    log_dir = os.path.join(HERE, "logs")
    os.makedirs(log_dir, exist_ok=True)

    global_idx = 0
    zero_warn = []
    failed = False
    for d in all_dates:
        dt_str = d.strftime("%Y-%m-%d")
        log_file = None if args.dry_run else os.path.join(log_dir, "backfill_{}.log".format(dt_str))
        if log_file:
            with open(log_file, "w", encoding="utf-8") as f:
                f.write("daily_backfill {} (layer={})\n".format(dt_str, args.layer or "all"))
                f.write("=" * 60 + "\n")
        day_ok = 0
        for layer_no, scripts in layers:
            for j, script in enumerate(scripts, 1):
                global_idx += 1
                label = "{} L{} {}/{}".format(dt_str, layer_no, j, len(scripts))
                if args.dry_run:
                    print("[{}] {}".format(label, script))
                    run_script(script, dt_str, True, None)
                    continue
                t0 = datetime.now()
                ok, rows, full = run_script(script, dt_str, False, log_file)
                elapsed = (datetime.now() - t0).total_seconds()
                if ok:
                    rows_str = "{} rows".format(rows) if rows is not None else "?"
                    mark = "OK "
                    if rows == 0:
                        mark = "OK "
                        zero_warn.append((dt_str, script))
                        rows_str += "  ⚠️ WARNING: 0 rows"
                    print("[{:<22}] {:<32} {} ({} {:.1f}s)".format(label, script, mark, rows_str, elapsed))
                    day_ok += 1
                else:
                    print("[{:<22}] {:<32} FAIL ({:.1f}s)".format(label, script, elapsed))
                    tail = full.strip().splitlines()
                    for line in tail[-6:]:
                        print("    " + line)
                    print("Stopping due to error.")
                    print("已跑到第 {}/{} 个。修复后可用 --layer {} 续跑。".format(global_idx, total_tables, layer_no))
                    failed = True
                    break
            if failed:
                break
        if failed:
            break
        if not args.dry_run:
            print("{} done: {}/{} OK".format(dt_str, day_ok, sum(len(s) for _, s in layers)))

    print("=" * 60)
    if args.dry_run:
        print("dry-run 完成，未写库。")
    elif failed:
        print("已停止（有失败）。")
    else:
        print("全部完成。")
    if zero_warn and not args.dry_run:
        print("⚠️ WARNING 0-row tables（请人工确认是否符合预期）：")
        for dt_str, script in zero_warn:
            print("  {} {}".format(dt_str, script))
    if not args.dry_run:
        print("日志见 logs/backfill_YYYY-MM-DD.log")
    print("=" * 60)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
