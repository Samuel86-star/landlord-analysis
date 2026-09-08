import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
from sweep import parse_metrics
from pathlib import Path

HERE = Path(__file__).resolve().parent
# 样本 jsonl 已外迁（默认 results/sweep_runs；本机大数据在 D:\analysis\sim-data\landlord-sim\sweep_runs）
RUNDIR = Path(os.environ.get("SWEEP_RUNS_DIR") or (HERE.parent / "results" / "sweep_runs"))

# new4 = [[4,5,3,6],[4,5,3,6],[4,5,3,6,13]] → 2/3 no-pair + 1/3 +bomb
nopair = parse_metrics(RUNDIR / "b13s17_nopair_n20000.jsonl")
bomb   = parse_metrics(RUNDIR / "b13s17_+bombcode_n20000.jsonl")

def mix(a, b, wa, wb):
    out = {}
    for k in a:
        if isinstance(a[k], list):
            out[k] = [(wa * x + wb * y) / (wa + wb) for x, y in zip(a[k], b[k])]
        elif isinstance(a[k], (int, float)):
            out[k] = (wa * a[k] + wb * b[k]) / (wa + wb)
        else:
            out[k] = a[k]
    return out

m = mix(nopair, bomb, 2, 1)

ks = ["hands", "singles", "bigcards", "bomb_held", "bomb_occ", "occ_real",
      "table_3x17", "table_real", "landlord_bomb20", "head_start", "resist", "delta_ratio", "hit"]

print("=" * 80)
print("new4 配牌数据（b13s17 + [[4,5,3,6],[4,5,3,6],[4,5,3,6,13]]，2万局/单策略，混合比 2:1）")
print("=" * 80)
print(f"{'指标':<18} {'nopair(2/3)':>14} {'+bomb(1/3)':>14} {'混合(new4)':>14}")
print("-" * 62)
for k in ks:
    print(f"{k:<18} {nopair[k]:>14.4f} {bomb[k]:>14.4f} {m[k]:>14.4f}")

print()
print("炸分布 3×17 (0/1/2/3+):")
print(f"  nopair:   {[round(x,4) for x in nopair['dens']]}")
print(f"  +bomb:    {[round(x,4) for x in bomb['dens']]}")
print(f"  mix(new4):{[round(x,4) for x in m['dens']]}")

print()
print("地主20张炸分布 (0/1/2/3+):")
print(f"  nopair:   {[round(x,4) for x in nopair['land_dens']]}")
print(f"  +bomb:    {[round(x,4) for x in bomb['land_dens']]}")
print(f"  mix(new4):{[round(x,4) for x in m['land_dens']]}")

print()
print(f"N(nopair) = {nopair['n']}, N(+bomb) = {bomb['n']}")
print(f"混合后等效 N ≈ {int((nopair['n'] + bomb['n']) * 3 / 4)}（加权混合）")
