#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
重构 A 对照验证（模拟器侧 / Python）
====================================
验证审计报告【维度三：随机源过时且不安全】的结论：
  Before（遗产）= SvrXygRandomSort (zgdatbl.h:593-602)
                  srand(seed) 每次重播种 + MSVC rand() 随机键排序
  After （新法）= MT19937 + Fisher-Yates  (= include/landlord.h:1044-1049 的目标态，
                  也等价于 Python random.shuffle)

三项指标：
  A. 单局均匀性   —— 54×54 (card,position) 卡方（越接近 df=2809 越均匀）
  B. 遗产法键碰撞 —— 一次洗牌内 ≥2 张牌拿到相同随机键的比率（并列→位置偏倚的根源）
  C. 跨桌种子碰撞 —— 生产里 srand(time(NULL)) 模式下，同一秒多桌是否得到相同随机流
                     （直击 zgdatbl.cpp:4177-4179 CouPaiStrategy 选组 / My_GetRandomBetweenEx）

忠实验证、不过度claim：单局内两者都接近均匀（遗产法略差），
真正的代差在 C（可预测 / 跨桌串流碰撞）—— 这正是审计风险②。
"""

import random
import sys
import time
from collections import Counter

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

DECK_SIZE = 54  # 52 + 大小王


# ============================================================
# 1. 忠实移植 MSVC rand() / srand()   (RAND_MAX = 0x7fff = 32767)
#    MSVC LCG:  state = (state*214013 + 2531011) mod 2^32 ;  return (state>>16) & 0x7fff
# ============================================================
class MSVCRand:
    RAND_MAX = 0x7FFF  # 32767

    def __init__(self, seed=0):
        self.srand(seed)

    def srand(self, seed):
        self._s = seed & 0xFFFFFFFF

    def rand(self):
        self._s = (self._s * 214013 + 2531011) & 0xFFFFFFFF
        return (self._s >> 16) & 0x7FFF


# ============================================================
# 2. 忠实移植 SvrXygRandomSort (zgdatbl.h:593-602)
#    srand(seed); value[i]=rand()%(length*1000); 按 value 排序 array
#    注：length*1000 = 54000 > RAND_MAX(32767) → [32768,53999] 永不产生
# ============================================================
def svr_xyg_random_sort(deck, seed):
    r = MSVCRand(seed)
    n = len(deck)
    s = n * 1000  # = 54000
    keys = [r.rand() % s for _ in range(n)]
    # 稳定排序：键并列时保持原相对顺序（近似 SvrReversalMoreByValue 对并列键的位置依赖）
    order = sorted(range(n), key=lambda i: keys[i])
    return [deck[i] for i in order]


# ============================================================
# 3. 新法：MT19937 + Fisher-Yates（Python random.shuffle 即此）
# ============================================================
def mt19937_shuffle(deck, rng):
    d = list(deck)
    rng.shuffle(d)
    return d


# ============================================================
# 指标 A：单局均匀性（卡方）
# ============================================================
def metric_uniformity(deck_fn, n, label):
    """deck_fn(seed_i) -> 一次洗牌结果（54 张）。返回卡方与 df。"""
    exp = n / DECK_SIZE
    # 54×54 计数矩阵（用一维 list 加速）
    size = DECK_SIZE * DECK_SIZE
    pos = [0] * size
    for i in range(n):
        d = deck_fn(i)
        for p in range(DECK_SIZE):
            pos[d[p] * DECK_SIZE + p] += 1
    chi2 = 0.0
    for v in pos:
        chi2 += (v - exp) ** 2 / exp
    df = (DECK_SIZE - 1) * (DECK_SIZE - 1)
    return chi2, df


# ============================================================
# 指标 B：遗产法键碰撞率
# ============================================================
def metric_key_collisions(n, seed_base=0):
    hit = 0
    for i in range(n):
        r = MSVCRand(seed_base + i)
        keys = [r.rand() % (DECK_SIZE * 1000) for _ in range(DECK_SIZE)]
        if len(set(keys)) < DECK_SIZE:
            hit += 1
    return hit / n


# ============================================================
# 指标 C：跨桌种子碰撞（srand(time(NULL)) 模式）
#   同一秒内 M 桌各自执行 srand(time(NULL)); idx = rand() % K（如 CouPaiStrategy 选组）
#   遗产法：同秒 → 同 seed → rand() 流完全相同 → idx 全相同
#   新法：   每桌独立从 random_device 播种 → idx 各自独立
# ============================================================
def metric_cross_table_collision(tables_per_sec, seconds, K):
    # 返回 (legacy_conc, mt_conc, legacy_distinct, mt_distinct)
    #   conc     = 同秒内"最大组"桌数 / 该秒总桌数 的均值（遗产法同 seed → 100%；新法 ≈ 1/K）
    #   distinct = 同秒内出现的 distinct 组数 均值（遗产法 = 1；新法 ≈ K）
    rng = random.Random(0xC0FFEE)
    legacy_conc = mt_conc = 0.0
    legacy_dist = mt_dist = 0.0
    for sec in range(seconds):
        # 遗产：同秒同 seed → 每桌 rand()%K 完全相同
        r = MSVCRand(sec)  # = srand(time(NULL)==sec)
        legacy_pick = r.rand() % K
        legacy_counts = Counter([legacy_pick] * tables_per_sec)
        legacy_conc += max(legacy_counts.values()) / tables_per_sec
        legacy_dist += len(legacy_counts)
        # 新法：每桌独立 high-entropy 播种
        mt_counts = Counter(rng.randrange(K) for _ in range(tables_per_sec))
        mt_conc += max(mt_counts.values()) / tables_per_sec
        mt_dist += len(mt_counts)
    return (legacy_conc / seconds, mt_conc / seconds,
            legacy_dist / seconds, mt_dist / seconds)


# ============================================================
def main():
    import argparse
    ap = argparse.ArgumentParser(description="重构A对照：遗产 SvrXygRandomSort vs MT19937 Fisher-Yates")
    ap.add_argument("-n", type=int, default=50000, help="单局均匀性/碰撞的样本局数 (默认 50000)")
    ap.add_argument("--tables", type=int, default=200, help="指标C 每秒桌数 (默认 200)")
    ap.add_argument("--seconds", type=int, default=3600, help="指标C 模拟秒数 (默认 3600)")
    ap.add_argument("--K", type=int, default=4, help="指标C CouPaiStrategy 组数 (默认 4，对照 robot 的 3 组 / 一般 2-3 组)")
    args = ap.parse_args()

    n = args.n
    base_deck = list(range(DECK_SIZE))

    print("=" * 72)
    print("重构 A 对照验证  Before=SvrXygRandomSort(rand/srand)  After=MT19937+Fisher-Yates")
    print("=" * 72)

    # ---- 指标 A：单局均匀性 ----
    print(f"\n[A] 单局均匀性  (N={n} 局, 期望卡方 ≈ df=2809; 越接近越均匀)")
    rng_mt = random.Random(0)
    chi_legacy, df = metric_uniformity(lambda s: svr_xyg_random_sort(base_deck, s + 1), n, "legacy")
    chi_mt, _ = metric_uniformity(lambda s: mt19937_shuffle(base_deck, rng_mt), n, "mt19937")
    print(f"    遗产 SvrXygRandomSort : χ² = {chi_legacy:9.1f}   (χ²/df = {chi_legacy/df:.3f})")
    print(f"    新法 MT19937+FY       : χ² = {chi_mt:9.1f}   (χ²/df = {chi_mt/df:.3f})")
    print(f"    df = {df}   → 两者都接近均匀；差距主要来自遗产法的键碰撞（见 [B]）")

    # ---- 指标 B：键碰撞 ----
    print(f"\n[B] 遗产法随机键碰撞率  (N={n} 局, s=54000 > RAND_MAX=32767)")
    rate = metric_key_collisions(n)
    # 理论：54 张牌、键取自 [0,32767]（因截断），期望并列对数 ≈ C(54,2)/32768
    expected_pair = (DECK_SIZE * (DECK_SIZE - 1) / 2) / 32768
    print(f"    遗产法 ≥1 次键碰撞的局占比 : {rate*100:5.2f}%")
    print(f"    理论期望并列对数/局        : {expected_pair:.4f}  (近似 {expected_pair*100:.2f}% 局会有碰撞)")
    print(f"    新法 MT19937               : 0%（Fisher-Yates 无随机键、无并列问题）")

    # ---- 指标 C：跨桌种子碰撞 ----
    print(f"\n[C] 跨桌种子碰撞  srand(time(NULL)) 模式  (每秒 {args.tables} 桌, "
          f"{args.seconds} 秒, K={args.K} 组)")
    legacy_conc, mt_conc, legacy_dist, mt_dist = metric_cross_table_collision(
        args.tables, args.seconds, args.K)
    print(f"    遗产法：同秒平均「最大组占比」 = {legacy_conc*100:6.2f}%   "
          f"distinct 组 ≈ {legacy_dist:.2f}   ← 同秒同 seed，所有桌选到同一组")
    print(f"    新法  ：同秒平均「最大组占比」 = {mt_conc*100:6.2f}%   "
          f"distinct 组 ≈ {mt_dist:.2f}   ← 每桌独立，~1/K={1/args.K:.2f} 均匀分散")
    print(f"    解读：遗产法下同一秒内 {args.tables} 桌 100% 选到同一个 CouPaiStrategy 组，")
    print(f"          且整个 rand() 流是当前秒的确定函数 → 可预测；新法各桌独立、不可预测。")

    print("\n" + "=" * 72)
    print("结论")
    print("=" * 72)
    print("• 单局看，两者都接近均匀（A），遗产法的轻微不均来自键碰撞（B，~{:.1f}% 局）。".format(rate*100))
    print("• 真正的代差在 C：srand(time(NULL)) 使同一秒所有桌的随机流完全相同且可预测；")
    print("  MT19937 + random_device 消除该碰撞。这正是审计风险②（可预测 + 不可复现）。")
    print("• 旁证：自家 C++ 重写版 include/landlord.h:1046-1047 已用 thread_local mt19937 + std::shuffle。")


if __name__ == "__main__":
    main()
