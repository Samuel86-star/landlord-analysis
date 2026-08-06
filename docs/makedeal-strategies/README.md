# 发牌策略目录（按玩法 / 房间）

> 每个房间一个文件，记录它的**发牌策略**（`makedeal.json` 里的配置）、现状指标、改造建议。
> **真值来源（2026-08 重构）**：`algorithm/native/extracted/harness.exe`（C++，发牌/配牌/洗牌管线 1:1 复刻线上）+ **搜索式全局最优拆牌** `optimal_split.h`（min-combo → max-Σscore，非贪心）。TOP 排名见 [`../../algorithm/native/extracted/top20_report.md`](../../algorithm/native/extracted/top20_report.md)，可落地 JSON 见 `top20_configs.json`。
> 源码逆向 [`./742-420-reverse-analysis.md`](./742-420-reverse-analysis.md)、代码质量审计 [`./makedeal-code-quality-audit.md`](./makedeal-code-quality-audit.md)。

> ⚠️ 口径已统一（详见各房间文件）：炸弹=**持有**（物理四张/王炸）为主、拆牌炸弹为辅；手数=**人均最优手数**；首叫诱导/抗衡用归一化牌力 P=sigmoid(V/40)。
> ⚠️ `docs/tech/classic-makedeal-config-topn.md`、`docs/tech/classic-makedeal-debomb-plan.md` 仍是**旧贪心 Python 模拟**口径（手数=t0拆、炸弹=贪心拆牌数），与本目录新口径不一致；TOP 排名以 `extracted/top20_report.md` 为准，待后续重写。

## 目录约定

```text
docs/makedeal-strategies/
  classic/         # 经典玩法，old2 / new / new2 策略
    <roomid>.md
  no-shuffle/      # 不洗牌，b1 / level4 策略
    <roomid>.md
  _template.md     # 复制这个填
```

- **文件名** = 房间 ID，如 `742.md`、`4484.md`。
- **玩法子文件夹**：按玩法分（经典玩法 / 不洗牌 / 四人斗地主 / …）。玩法归属见下表，**带 ⚠️ 的是推断、待确认**。
- 每个文件用统一 frontmatter + 章节（见 `_template.md`）。

## 全房间索引（源自 `algorithm/native/previous/makedeal.json`）

### 经典玩法（推断）

| 房间 | 当前策略 | MakeDealType | 说明 |
|---|---|---|---|
| 742 | `new` | 1 | `[4,5,3,6]` b11 sel15；已上线减炸（见 [`classic/742.md`](classic/742.md)） |
| 420 | `new2` | 1 | `[4,6,5,2,3]` b14 sel17；改造中（见 [`classic/420.md`](classic/420.md)） |
| 11167 / 4483 / 4484 / 1124 / 1126 | `old2` | 0 | 新手/初级房 |
| 12074 / 6314 / 11168 | `old2` | 0 | |
| 10336 / 16445 | `old2` | 0 | |
| 13176 / 13177 / 13178 | `old2` | 0 | |
| 11534 / 14238 / 15458 | `old2` | 0 | |

> ⚠️ 以上 old2 房间均按"经典玩法"推断（`old2` 主要用于经典）；个别可能是其他玩法，按 `dq_game_room_config` 校正。
> old2 现状（harness + 最优拆牌，N=20000）：**持有炸 0.42 / 单局炸率 0.70 / 人均手 5.93**（多炸，待降）；推荐改造见 [`../../algorithm/native/extracted/top20_report.md`](../../algorithm/native/extracted/top20_report.md)。

### 不洗牌（NoShuff，推断）

| 房间 | 当前策略 | MakeDealType | 备注 |
|---|---|---|---|
| 22039 / 421 / 1125 | `b1` | — | 不洗牌房，NoShuffProbability=100 |
| 22040 / 22041 / 22042 | `level4` | 1 | 不洗牌，CouPaiStrategy 含炸码 13 |
| 159 ⚠️ | — | — | 只出现在 NoShuff 表，MakeDeal 无显式映射 |

> ⚠️ 不洗牌走单独的 NoShuff 机制（`NoShuffStrategy`/`NoShuffCardValue`/`NoShuff2KProbability`），与本仓库 Type0/Type1 harness 不是同一套；这些房间文件主要记录 NoShuff 参数。

## 怎么补

1. 复制 `_template.md` → `<玩法文件夹>/<roomid>.md`（如 `classic/420.md`）。
2. 填 frontmatter（room_id / 玩法 / 当前策略 / MakeDealType）。
3. 填「当前发牌参数」（从 `makedeal.json` 对应策略抄）、「现状指标」（harness 跑）、「改造建议」「备注」。
4. 跑模拟器（C++ 真值，三家同策略）：
   ```bash
   # 纯随机基线
   algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
       --pure-random -n 20000 --seed 1 --reals 3
   # Type1 注入任意候选
   algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
       --type1 --coupai 4,5,3,6 --begin 11 --select 15 --tv 999 --tr 10 \
       -n 20000 --seed 1 --reals 3
   # Type0 注入任意候选
   algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
       --type0 --bmn 12 --bigcards-to 2 --first-hc 5 --first-bomb 2 --first-big 4 \
       --other-hc 6 --other-bomb 2 --other-big 3 -n 20000 --seed 1 --reals 3
   ```
   聚合统计：`algorithm/native/extracted/anchor_check.py <jsonl...>`。
   全量 TOP20 扫描：`py -3 algorithm/native/extracted/sweep.py --coarse-n 3000 --final-n 20000 --base-n 20000`（改权重后加 `--rerank` 秒重排）。
