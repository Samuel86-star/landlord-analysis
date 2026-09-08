# anchor_check.py —— 锚点复核 + 新口径自然区间检查
# 用法: py -3 -u anchor_check.py anchor_pure.jsonl anchor_new.jsonl anchor_new2.jsonl anchor_old2.jsonl
import json, sys, math, statistics as st

def sigmoid(x): return 1.0/(1.0+math.exp(-x/40.0))

def summ(path):
    deals = [json.loads(l) for l in open(path, encoding="utf-8-sig") if l.strip()]
    seats = [s for r in deals for s in r["seats"]]
    n = len(deals)
    held = st.mean([s["bombs"] for s in seats])               # 人均持有炸弹(颗)
    occ = sum(1 for r in deals if any(s["bombs"]>=1 for s in r["seats"]))/n   # 单局炸弹率
    # 密度分布 K=Σseat held bombs
    dens = [0,0,0,0]
    for r in deals:
        k = sum(s["bombs"] for s in r["seats"])
        dens[3 if k>=3 else k] += 1
    dens = [d/n for d in dens]
    hands = st.mean([s["opt_hands"] for s in seats])          # 人均最优手数
    singles = st.mean([s["singles"] for s in seats])
    val = st.mean([s["val_f"] for s in seats])
    # 首叫诱导度/抗衡度
    hs=[]; rs=[]
    for r in deals:
        ps = sorted((sigmoid(s["val_f"]) for s in r["seats"]), reverse=True)
        pavg=sum(ps)/3.0
        if pavg>1e-9:
            hs.append(ps[0]/pavg)
            if ps[0]>1e-9: rs.append((ps[1]+ps[2])/ps[0])
    return dict(n=n, held=held, occ=occ, dens=dens, hands=hands, singles=singles,
                val=val, hs=st.mean(hs) if hs else 0, res=st.mean(rs) if rs else 0,
                ll20=_ll(deals))

def _ll(deals):
    """landlord-20 口径（仅当 harness 以 --landlord-bottom 跑时存在）。"""
    have = [r for r in deals if "landlord_bomb20" in r]
    if not have:
        return None
    lb = [r["landlord_bomb20"] for r in have]
    tr = [r["table_real_bombs"] for r in have]
    return dict(landlord_bomb20=st.mean(lb), table_real=st.mean(tr),
                occ_real=sum(1 for x in tr if x > 0)/len(tr))

for path in sys.argv[1:]:
    m = summ(path)
    tag = path.split('_')[-1].replace('.jsonl','')
    line = (f"[{tag:6}] N={m['n']:5d}  持有炸={m['held']:.4f}  单局炸率={m['occ']:.3f}  "
            f"密度[0/1/2/3+]=[{m['dens'][0]:.3f}/{m['dens'][1]:.3f}/{m['dens'][2]:.3f}/{m['dens'][3]:.3f}]  "
            f"人均手={m['hands']:.3f}  人均散={m['singles']:.3f}  牌力={m['val']:.2f}  "
            f"首叫诱导={m['hs']:.3f}  抗衡={m['res']:.3f}")
    if m['ll20']:
        L = m['ll20']
        line += (f"  | 地主20炸={L['landlord_bomb20']:.3f}  桌真实={L['table_real']:.3f}  "
                 f"有炸局(真实)={L['occ_real']*100:.1f}%")
    print(line)
