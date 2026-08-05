# 发牌策略目录（按玩法 / 房间）

> 每个房间一个文件，记录它的**发牌策略**（`makedeal.json` 里的配置）、现状指标、改造建议。
> 配套：总览与 TOP 排名见 [`../tech/classic-makedeal-config-topn.md`](../tech/classic-makedeal-config-topn.md)，落地方案见 [`../tech/classic-makedeal-debomb-plan.md`](../tech/classic-makedeal-debomb-plan.md)，模拟器在 `algorithm/native/`。

## 目录约定

```
docs/makedeal-strategies/
  经典玩法/        # game_id=53 等，old2 / new 策略
    <roomid>.md
  不洗牌/          # NoShuff 房，b1 / level4 策略
    <roomid>.md
  _模板.md         # 复制这个填
```

- **文件名** = 房间 ID，如 `742.md`、`4484.md`。
- **玩法子文件夹**：按玩法分（经典玩法 / 不洗牌 / 四人斗地主 / …）。玩法归属见下表，**带 ⚠️ 的是推断、待你确认**。
- 每个文件用统一 frontmatter + 章节（见 `_模板.md`）。

## 全房间索引（源自 `algorithm/native/previous/makedeal.json`）

### 经典玩法（推断）

| 房间 | 当前策略 | MakeDealType | 说明 |
|---|---|---|---|
| 742 | `new` | 1 | `[4,5,3,6]` b11 sel15；已实测减炸（见 `742.md`） |
| 11167 / 4483 / 4484 / 1124 / 1126 | `old2` | 0 | 新手/初级房 |
| 420 / 12074 / 6314 / 11168 | `old2` | 0 | |
| 10336 / 16445 | `old2` | 0 | |
| 13176 / 13177 / 13178 | `old2` | 0 | |
| 11534 / 14238 / 15458 | `old2` | 0 | |

> ⚠️ 以上 old2 房间均按"经典玩法"推断（`old2` 主要用于经典）；个别可能是其他玩法，按 `dq_game_room_config` 校正。
> old2 现状模拟器预测 ≈ **0.43 炸/手 6.1**（多炸，待降）；推荐改造见落地方案 §二/§二B。

### 不洗牌（NoShuff，推断）

| 房间 | 当前策略 | MakeDealType | 备注 |
|---|---|---|---|
| 22039 / 421 / 1125 | `b1` | — | 不洗牌房，NoShuffProbability=100 |
| 22040 / 22041 / 22042 | `level4` | 1 | 不洗牌，CouPaiStrategy 含炸码 13 |
| 159 ⚠️ | — | — | 只出现在 NoShuff 表，MakeDeal 无显式映射 |

> ⚠️ 不洗牌走单独的 NoShuff 机制（`NoShuffStrategy`/`NoShuffCardValue`/`NoShuff2KProbability`），与本仓库 Type0/Type1 模拟器不是同一套；这些房间文件主要记录 NoShuff 参数。

## 怎么补

1. 复制 `_模板.md` → `<玩法>/<roomid>.md`。
2. 填 frontmatter（room_id / 玩法 / 当前策略 / MakeDealType）。
3. 填「当前发牌参数」（从 `makedeal.json` 对应策略抄）、「现状指标」（模拟器跑或线上日志）、「改造建议」「备注」。
4. 跑模拟器：`py -3 algorithm/native/old2_type1_sim.py run --coupai <名> --begin <N> --select <N> --tv <V> --tr <R>`（Type1）或 `old2_type0_sim.py run --fix <名> --bmn <N>`（Type0）。
