#!/usr/bin/env python3
"""首次经典初级房玩家前 3 局手牌分析 — 主编排。

跑 sql/01_cohort_first3_detail.sql 分页拉前 3 局明细，本地 pandas 完成 6 模块聚合，
结果落 output/*.csv。

模块：A cohort 基线 / B 局序概览 / C 牌力分布 / D 配牌机制 / E 持有炸弹 / F 牌力-胜负一致性。
原框架第七章手牌结构因 hand_cards 数仓全历史缺失（已查证），改用 bomb_cnt/bomb_final
的「持有炸弹分析」替代。

用法:
    py -3 -u py/first-classic-beginner/run_analysis.py
"""
import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))  # 让 import sr_exec 生效
from sr_exec import StarRocksClient  # noqa: E402

OUTPUT = HERE / "output"
OUTPUT.mkdir(exist_ok=True)
BEGINNER_ROOMS = (4484, 12074)
NUMERIC_COLS = [
    "uid", "first_room_id", "game_seq", "room_id", "play_mode", "player_role",
    "result_id", "timecost", "room_base", "room_fee", "start_money", "end_money",
    "game_outcome_money", "magnification", "real_magnification", "shuffle_type",
    "card_id", "card_power", "card_power_final", "cost_time", "shuffle_times",
    "user_attr_bout", "bomb_cnt", "bomb_final",
]


def load_detail() -> pd.DataFrame:
    """分页拉前 3 局明细 + 类型清洗。"""
    sql = (HERE / "sql" / "01_cohort_first3_detail.sql").read_text(encoding="utf-8")
    client = StarRocksClient()
    client.login().connect()
    df = client.query_paged(sql, page_size=5000)
    if df is None or len(df) == 0:
        raise SystemExit("FATAL: detail query empty - SQL likely failed silently in CloudBeaver")
    for c in NUMERIC_COLS:
        if c in df.columns:
            df[c] = pd.to_numeric(df[c], errors="coerce")
    df["is_pass"] = df["is_pass"].astype(str).str.strip().str.lower() == "true"
    df["game_datetime"] = pd.to_datetime(df["game_datetime"], errors="coerce")
    df["reg_date"] = pd.to_datetime(df["reg_date"], errors="coerce")
    return df


def _save(df: pd.DataFrame, name: str) -> None:
    df.to_csv(OUTPUT / name, index=False, encoding="utf-8-sig")
    print(f"  -> {name}  ({len(df)} rows)")


def _pct(num, den):
    return round(num * 100.0 / den, 2) if den else np.nan


def _win_rate(s):
    return _pct((s == 1).sum(), s.notna().sum())


# ---------- 模块 A：cohort 基线 ----------
def module_a(df: pd.DataFrame) -> None:
    per_user = (
        df.groupby(["uid", "reg_date", "first_room_id", "channel_category_name"], as_index=False)
          .agg(max_seq=("game_seq", "max"))
    )
    n_total = per_user["uid"].nunique()

    room = per_user.groupby("first_room_id").agg(user_count=("uid", "nunique")).reset_index()
    room["user_pct"] = (room["user_count"] / n_total * 100).round(2)
    _save(room, "01a_cohort_room.csv")

    dated = per_user.assign(reg_date=per_user["reg_date"].dt.date) \
        .groupby("reg_date").agg(user_count=("uid", "nunique")).reset_index()
    _save(dated, "01b_cohort_date.csv")

    ch = per_user.groupby("channel_category_name").agg(user_count=("uid", "nunique")).reset_index()
    ch["user_pct"] = (ch["user_count"] / n_total * 100).round(2)
    _save(ch.sort_values("user_count", ascending=False), "01c_cohort_channel.csv")

    reach = per_user.groupby("max_seq").agg(user_count=("uid", "nunique")).reset_index()
    reach["user_pct"] = (reach["user_count"] / n_total * 100).round(2)
    _save(reach, "01d_cohort_reachability.csv")

    print(f"  [A] cohort_total = {n_total}")
    print("  [A] reachability: " + " ; ".join(
        f">={m}局:{int((per_user['max_seq'] >= m).sum())}" for m in (1, 2, 3)))


# ---------- 模块 B：局序概览 ----------
def module_b(df: pd.DataFrame) -> None:
    g = df.groupby("game_seq")
    out = pd.DataFrame({
        "user_count": g["uid"].nunique(),
        "beginner_room_pct": g["room_id"].agg(lambda s: _pct(s.isin(BEGINNER_ROOMS).sum(), len(s))),
        "landlord_pct": g["player_role"].agg(lambda s: _pct((s == 1).sum(), s.notna().sum())),
        "win_rate": g["result_id"].agg(_win_rate),
        "avg_timecost": g["timecost"].mean().round(1),
        "avg_magnification": g["magnification"].mean().round(2),
        "avg_real_magnification": g["real_magnification"].mean().round(2),
        "avg_outcome_money": g["game_outcome_money"].mean().round(0),
        "avg_start_money": g["start_money"].mean().round(0),
        "avg_end_money": g["end_money"].mean().round(0),
    }).reset_index()
    _save(out, "02_game_seq_overview.csv")
    print("  [B] overview:")
    print(out.to_string(index=False))


# ---------- 模块 C：牌力分布 ----------
def module_c(df: pd.DataFrame, p25, p50, p75) -> None:
    agg = df.groupby(["game_seq", "card_power_bucket"]).agg(
        user_count=("uid", "nunique"),
        avg_card_power=("card_power", "mean"),
        avg_card_power_final=("card_power_final", "mean"),
    ).round(2).reset_index()
    _save(agg, "03_card_power_distribution.csv")

    by_role = df.groupby(["game_seq", "player_role", "card_power_bucket"]).agg(
        user_count=("uid", "nunique"),
        avg_card_power=("card_power", "mean"),
    ).round(2).reset_index()
    _save(by_role, "03b_card_power_by_role.csv")
    print(f"  [C] seq1 buckets:")
    print(agg[agg.game_seq == 1][["card_power_bucket", "user_count", "avg_card_power"]].to_string(index=False))


# ---------- 模块 D：配牌机制 ----------
def module_d(df: pd.DataFrame) -> None:
    agg = df.groupby(["game_seq", "shuffle_group"]).agg(
        records=("uid", "count"),
        user_count=("uid", "nunique"),
        avg_card_power=("card_power", "mean"),
        reshuffle_pass_rate=("is_pass", "mean"),
    ).round(3).reset_index()
    seq_total = df.groupby("game_seq")["uid"].count().rename("seq_total").reset_index()
    agg = agg.merge(seq_total, on="game_seq")
    agg["pct_in_seq"] = (agg["records"] / agg["seq_total"] * 100).round(2)
    _save(agg, "04_shuffle_mechanism.csv")

    st = df.groupby(["game_seq", "shuffle_times"]).agg(records=("uid", "count")).reset_index()
    _save(st, "04b_shuffle_times.csv")
    print("  [D] seq1 shuffle_group:")
    print(agg[agg.game_seq == 1][["shuffle_group", "records", "pct_in_seq", "avg_card_power"]].to_string(index=False))


# ---------- 模块 E：持有炸弹分析（替代手牌结构） ----------
def module_e(df: pd.DataFrame) -> None:
    g = df.groupby("game_seq")
    out = pd.DataFrame({
        "user_count": g["uid"].nunique(),
        "bomb_hold_rate": g["has_bomb"].mean().round(4),
        "avg_bomb_cnt": g["bomb_cnt"].mean().round(3),
        "avg_bomb_final": g["bomb_final"].mean().round(3),
    }).reset_index()
    _save(out, "05a_bomb_hold_overview.csv")

    dist = df.groupby(["game_seq", "bomb_cnt"]).agg(records=("uid", "count")).reset_index()
    _save(dist, "05b_bomb_cnt_dist.csv")

    role = df.groupby(["game_seq", "player_role"]).agg(
        user_count=("uid", "nunique"),
        avg_bomb_cnt=("bomb_cnt", "mean"),
        avg_bomb_final=("bomb_final", "mean"),
        bomb_hold_rate=("has_bomb", "mean"),
    ).round(3).reset_index()
    _save(role, "05c_bomb_by_role.csv")
    print("  [E] bomb hold by seq:")
    print(out.to_string(index=False))


# ---------- 模块 F：牌力-胜负一致性 ----------
def module_f(df: pd.DataFrame) -> None:
    valid = df[df["card_power_bucket"] != "Z: 缺失"].copy()
    agg = valid.groupby(["game_seq", "card_power_bucket"]).agg(
        user_count=("uid", "nunique"),
        win_rate=("result_id", _win_rate),
        avg_outcome_money=("game_outcome_money", "mean"),
        avg_real_magnification=("real_magnification", "mean"),
    ).round(2).reset_index()
    _save(agg, "06_cardpower_result.csv")

    by_role = valid.groupby(["game_seq", "player_role", "card_power_bucket"]).agg(
        user_count=("uid", "nunique"),
        win_rate=("result_id", _win_rate),
    ).round(2).reset_index()
    _save(by_role, "06b_cardpower_result_by_role.csv")

    seq1 = valid[valid["game_seq"] == 1].copy()
    seq1["protect"] = np.where(seq1["shuffle_group"] == "A: 新手保护配牌", "新手保护", "非新手保护")
    by_prot = seq1.groupby(["protect", "card_power_bucket"]).agg(
        user_count=("uid", "nunique"),
        win_rate=("result_id", _win_rate),
    ).round(2).reset_index()
    _save(by_prot, "06c_cardpower_result_by_protect.csv")
    print("  [F] seq1 牌力桶 × 胜率:")
    print(agg[agg.game_seq == 1][["card_power_bucket", "user_count", "win_rate", "avg_outcome_money"]].to_string(index=False))


def main() -> None:
    df = load_detail()
    print(f"[load] detail shape = {df.shape}")

    # 牌力分桶（整体前 3 局分位数，供 C/F 用）
    q = df["card_power"].quantile([0.25, 0.5, 0.75])
    p25, p50, p75 = q[0.25], q[0.5], q[0.75]

    def _bucket(v):
        if pd.isna(v):
            return "Z: 缺失"
        if v < p25:
            return "A: 低牌力"
        if v < p50:
            return "B: 中低牌力"
        if v < p75:
            return "C: 中高牌力"
        return "D: 高牌力"

    df["card_power_bucket"] = df["card_power"].map(_bucket)
    # 配牌分组（框架 6.4）
    df["shuffle_group"] = np.where(
        df["shuffle_type"] == 201, "A: 新手保护配牌",
        np.where(df["card_id"] > 0, "B: 其他牌库配牌", "C: 随机/无牌库"))
    # 持有炸弹标记
    df["has_bomb"] = df["bomb_cnt"] >= 1
    print(f"[load] card_power P25/P50/P75 = {p25:.1f}/{p50:.1f}/{p75:.1f}")

    print("[A] cohort baseline");        module_a(df)
    print("[B] game seq overview");      module_b(df)
    print("[C] card power distribution"); module_c(df, p25, p50, p75)
    print("[D] shuffle mechanism");      module_d(df)
    print("[E] bomb hold (replaces handcard structure)"); module_e(df)
    print("[F] cardpower-result alignment"); module_f(df)
    print("[done] all CSVs ->", OUTPUT)


if __name__ == "__main__":
    main()
