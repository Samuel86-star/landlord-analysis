import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
from sweep import run_one
from concurrent.futures import ThreadPoolExecutor, as_completed

# 用户新配置: b13s17 + CouPaiStrategy[[4,5,3,6],[4,5,3,6],[4,5,3,6,13]]
# 每局 rand()%3 → 2/3 用 [4,5,3,6], 1/3 用 [4,5,3,6,13]（炸码兜底）
# 期望 = 混合 2/3 + 1/3（每局独立选策略 → 线性混合精确成立）
cands = [
    ("nopair13", {"type": 1, "coupai": [4, 5, 3, 6], "begin": 13, "select": 17, "tv": 999, "tr": 10, "label": "b13s17 nopair"}),
    ("bomb13",   {"type": 1, "coupai": [4, 5, 3, 6, 13], "begin": 13, "select": 17, "tv": 999, "tr": 10, "label": "b13s17 +bombcode"}),
]
res = {}
with ThreadPoolExecutor(max_workers=2) as ex:
    futs = {ex.submit(run_one, c, 20000): name for name, c in cands}
    for f in as_completed(futs):
        c, m = f.result(); res[futs[f]] = m
        print("done:", futs[f], file=sys.stderr, flush=True)

ks = ["hands", "singles", "bigcards", "bomb_held", "bomb_occ", "occ_real",
      "table_3x17", "table_real", "landlord_bomb20", "head_start", "resist", "delta_ratio", "hit"]
print("\nconfig    | " + " | ".join(ks))
print("-" * 130)
for name in ["nopair13", "bomb13"]:
    print(f"{name} | " + " | ".join(f"{res[name][k]:.3f}" for k in ks))
print("\ndens / land_dens:")
for name in ["nopair13", "bomb13"]:
    print(name, [round(x, 3) for x in res[name]["dens"]], [round(x, 3) for x in res[name]["land_dens"]])

# 混合 (2/3, 1/3)
mix = {k: (2 * res["nopair13"][k] + res["bomb13"][k]) / 3 for k in ks}
dens_mix = [(2 * a + b) / 3 for a, b in zip(res["nopair13"]["dens"], res["bomb13"]["dens"])]
ldens_mix = [(2 * a + b) / 3 for a, b in zip(res["nopair13"]["land_dens"], res["bomb13"]["land_dens"])]
print("\nMIX 2/3+1/3 | " + " | ".join(f"{mix[k]:.3f}" for k in ks))
print("mix dens/land_dens:", [round(x, 3) for x in dens_mix], [round(x, 3) for x in ldens_mix])
