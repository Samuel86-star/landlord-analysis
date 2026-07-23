#!/usr/bin/env python3
"""retention 域回填调度器：daily_allgame_stat + retention_flag + firstday_game_stat，按 reg_date 重算。

默认回扫 35 天（today-36 .. today-1）的 reg_date，确保中途漏跑也能刷新到位——
retention 依赖的 game_active / daily_login 是 DWS 按天分区表、不会过期，重算天然正确。
end 默认为昨天而非今天：今天的注册用户 d1 还没到期、且上游可能未跑完，强行跑只会算错。

三层（按 reg_date 循环，层内串行）：
    A1 daily_allgame_stat   依赖 allgame_stat 当天（reg_date），uid×dt 降维
    A2 retention_flag       依赖 app_daily_reg + game_active + daily_login
                            （计算 d1/d3/d7/d14/d30 留存 flag，需要 reg_date+N 天的上游数据已就位）
    B  firstday_game_stat   依赖 app_daily_reg + silvergame/scoregame/daily_allgame_stat
                            （首日游戏指标，注册当日只写一次，但回扫幂等无副作用）

设计：firstday_game_stat 只含首日游戏指标（写入后不变），retention_flag 独立放留存 flag
     （按到期日逐步回填 NULL/0/1 三态）。分析时 firstday_game_stat LEFT JOIN retention_flag。

用法：
    py -3 -u .\\daily_retention.py                       # 默认回扫 today-36..today-1（不含今天）
    py -3 -u .\\daily_retention.py --start 20260501 --end 20260519   # 指定 reg_date 区间
    py -3 -u .\\daily_retention.py --window 60           # 自定义回扫窗口天数
    py -3 -u .\\daily_retention.py --dry-run             # 只看计划不写库
"""
import argparse
import os
import re
import subprocess
import sys
from datetime import date, datetime, timedelta

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_WINDOW = 35

# 三层，按 reg_date 循环
# A1 daily_allgame_stat   依赖 allgame_stat 当天（reg_date），uid×dt 降维
# A2 retention_flag       依赖 dws_dq_app_daily_reg + game_active + daily_login，reg_date+1/3/7/14/30 flag 降维
# B  firstday_game_stat   依赖 app_daily_reg + silver/score/daily_allgame_stat，首日游戏指标宽表（无 flag）
LAYERS = [
    ("A1", ["batch_insert_daily_allgame_stat"]),
    ("A2", ["batch_insert_retention_flag"]),
    ("B",  ["batch_insert_firstday_game_stat"]),
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
    """subprocess 调用一个 batch 脚本。Windows 子进程 stdout 默认 GBK，这里强制 UTF-8。"""
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
    parser = argparse.ArgumentParser(description="retention 域回填调度器（daily_allgame_stat + retention_flag + firstday_game_stat）")
    parser.add_argument("--start", type=parse_dt, help="reg_date 起始（不指定则用 today-window）")
    parser.add_argument("--end", type=parse_dt, help="reg_date 截止（不指定则用 today-1，即昨天）")
    parser.add_argument("--window", type=int, default=DEFAULT_WINDOW, help="默认回扫窗口天数（默认 {}）".format(DEFAULT_WINDOW))
    parser.add_argument("--dry-run", action="store_true", help="只打印计划与子脚本 SQL，不写库")
    args = parser.parse_args()

    today = date.today()
    if args.end is None:
        # 默认 end = 昨天。今天的 reg_date 即便回填了 app_daily_reg，d1 也未到期，flag 全 NULL 无意义；
        # 而且今天的上游表（app_daily_reg 等）可能还没跑完 daily_backfill，强行跑会基于不完整数据算错。
        args.end = today - timedelta(days=1)
    if args.start is None:
        args.start = args.end - timedelta(days=args.window)
    if args.end < args.start:
        parser.error("截止日期不能早于起始日期")

    all_dates = list(daterange(args.start, args.end))
    total = len(all_dates) * sum(len(s) for _, s in LAYERS)

    print("=" * 60)
    print("daily_retention reg_date {} -> {} ({} day, window={})".format(
        args.start, args.end, len(all_dates), args.window))
    print("layers: {}".format(" -> ".join(l for l, _ in LAYERS)))
    print("注意：retention_flag 的 d1/d3/d7/d14/d30 依赖 reg_date+N 天的上游数据已就位")
    if args.dry_run:
        print("(dry-run：仅打印子脚本 SQL，不写库)")
    print("=" * 60)

    log_dir = os.path.join(HERE, "logs")
    os.makedirs(log_dir, exist_ok=True)
    log_file = None if args.dry_run else os.path.join(log_dir, "retention_{}.log".format(today.strftime("%Y%m%d")))
    if log_file:
        with open(log_file, "w", encoding="utf-8") as f:
            f.write("daily_retention reg_date {} -> {}\n".format(args.start, args.end))
            f.write("=" * 60 + "\n")

    global_idx = 0
    zero_warn = []
    failed = False
    day_ok = 0
    for d in all_dates:
        dt_str = d.strftime("%Y-%m-%d")
        for layer_no, scripts in LAYERS:
            for script in scripts:
                global_idx += 1
                label = "reg={} L{}".format(dt_str, layer_no)
                if args.dry_run:
                    print("[{:<28}] {}".format(label, script))
                    run_script(script, dt_str, True, None)
                    continue
                t0 = datetime.now()
                ok, rows, full = run_script(script, dt_str, False, log_file)
                elapsed = (datetime.now() - t0).total_seconds()
                if ok:
                    rows_str = "{} rows".format(rows) if rows is not None else "?"
                    if rows == 0:
                        zero_warn.append((dt_str, script))
                        rows_str += "  ⚠️ WARNING: 0 rows"
                    print("[{:<28}] {:<32} OK ({} {:.1f}s)".format(label, script, rows_str, elapsed))
                    day_ok += 1
                else:
                    print("[{:<28}] {:<32} FAIL ({:.1f}s)".format(label, script, elapsed))
                    for line in full.strip().splitlines()[-6:]:
                        print("    " + line)
                    print("Stopping due to error. 已跑到 {}/{}。".format(global_idx, total))
                    failed = True
                    break
            if failed:
                break
        if failed:
            break

    print("=" * 60)
    if args.dry_run:
        print("dry-run 完成，未写库。")
    elif failed:
        print("已停止（有失败）。")
    else:
        print("全部完成：{}/{} OK。".format(day_ok, total))
    if zero_warn and not args.dry_run:
        print("⚠️ WARNING 0-row tables（请人工确认）：")
        for dt_str, script in zero_warn:
            print("  {} {}".format(dt_str, script))
    if not args.dry_run:
        print("日志见 logs/retention_YYYYMMDD.log")
    print("=" * 60)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
