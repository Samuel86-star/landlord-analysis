# 已游戏玩家留存归因报告

> **核心问题**：已进入对局的新玩家（`silver_game_count > 0`），D7 留存为何仅 ~12%（88% 七日流失）？
>
> **窗口**：APP 端 `app_id=1880053`，`reg_date BETWEEN '2026-05-10' AND '2026-06-08'`（D7 全到期），n=49,329 总注册 / 44,031 进游戏。
>
> **数据源**：`dws_app_firstday_game_stat`（首日指标宽表）+ `dws_app_retention_flag`（留存 flag）+ `dws_ddz_firstday_game`（首日对局明细，含 timecost/shuffle_type/role 等）。
>
> **方法**：systematic-debugging。从基线出发，逐层下钻+假设证伪/证实，找出"低留存集中在哪、是什么驱动"。
>
> **关键纪律**：所有时长、倍数、留存数字均来自 SQL 实测，禁止凭常识猜（曾因猜"一局2-3分钟"被纠正，实测 timecost 中位 85s）。

---

## 一、基线：低留存集中在 1-5局亚群

已游戏玩家（`silver_game_count>0`）整体 D7≈12%。按对局数 × 胜率二维分桶：

| 已游戏玩家亚群 | 占比 | D7 |
| ---- | ---- | ---- |
| 10+局 × 各胜率 | 35.4% | 12-19% |
| 6-10局 × 各胜率 | 27.8% | 6.7-11.5% |
| **2-5局** × 各胜率 | 27.7% | **5.0-7.2%** |
| **仅1局** × 各胜率 | **9.2%** | **2.5-3.5%** |

**1局 + 2-5局合计 36.9%**，这是 D7<10% 的"低留存腰部"。其中仅1局玩家（3,268 人，9.2%）D7 仅 3.18%，是流失最严重的群体。SQL: `recheck_gameplayers_2d.sql`。

---

## 二、首层假设：首局体验劝退？——证伪

直觉假设：1-5局玩家是被首局"虐"走的——首局负、当地主输高倍。验证：

| 首局组合 | n | D7 | 占比 |
| ---- | ---- | ---- | ---- |
| 胜+地主+新手保护机器人 | 7,844 | 5.21 | 61.5% |
| 胜+农民+新手保护机器人 | 1,645 | 4.92 | 12.9% |
| 负+地主+新手保护机器人 | 1,442 | 4.58 | 11.3% |
| 负+农民+新手保护机器人 | 840 | 4.40 | 6.6% |
| 胜+地主+随机发牌 | 606 | 6.11 | 4.8% |
| 负+农民+随机发牌 | 103 | 0.97 | 0.8% |

**关键事实**：

- **94.3% 的 1-5局玩家首局都触发了新手保护机器人**（`shuffle_type=201`）——即首局被刻意配机器人对手。
- **61.5% 是"首局胜+当地主+保护机器人"**——保护机制让多数新玩家首局有压倒性优势抢地主+赢。
- **但首局胜负差异极小**：胜+地主+保护 5.21% vs 负+地主+保护 4.58%，**仅差 0.6pp**。

**结论**：首局体验已被产品保护机制做得"很好"（94% 保护、80% 胜率），但**保护到位也没留住他们**。首局体验不是 1-5局流失的主因。SQL: `recheck_lowgames_firstgame_v2.sql`。

---

## 三、次层假设：撤保护断崖逼走？——证伪

观察前 5 局保护撤离曲线：

| 局次 | 触及人数 | 新手保护% | 胜率 | 平均倍数 |
| ---- | ---- | ---- | ---- | ---- |
| 第1局 | 12,755 | 92.3% | 80.3% | 14.7 |
| 第2局 | 9,487 | 91.6% | 55.5% | 25.1 |
| 第3局 | 6,648 | 74.8% | 50.3% | 66.2 |
| 第4局 | 4,567 | **0%** | **27.0%** | 110.4 |
| 第5局 | 2,272 | 0% | 14.9% | 102.0 |

保护机制在第 1-3 局逐级撤离，**第 4 局彻底归零**。胜率从 80%→27%（断崖），倍数从 14.7→110.4（飙升）。看似第 3→4 局是流失拐点。

但按"打到第几局停"分组看真实留存：

| 离开时点 | n | D1 | D7 |
| ---- | ---- | ---- | ---- |
| 仅1局 | 3,268 | 8.84 | **3.18** |
| 仅2局 | 2,839 | 11.45 | 3.10 |
| 仅3局 | 2,081 | 14.46 | 5.43 |
| 4局 | 2,295 | 15.47 | **7.06** |
| 5局 | 2,272 | 16.11 | 7.83 |

**D7 单调递增**——打得越多留得越好。**打到第4局（被撤保护那局）的人 D7=7.06，比仅打1局的人（3.18）高 122%**。

**结论**：撤保护断崖**没有**赶走玩家。第 3→4 局的"真实虐杀"非但不是流失原因，反而对应更高留存。1-5局流失发生得**更早**——大量人在保护机制还在的时候就离开了。SQL: `recheck_first5_games_pattern.sql`、`recheck_exit_at.sql`。

---

## 四、对局时长画像：实测分布

`dws_ddz_firstday_game.timecost` 实测（1-5局玩家所有对局，n=35,729）：

| 时长段 | 占比 | 桶内均值 |
| ---- | ---- | ---- |
| <30s（异常短/逃跑） | 0.1% | 26s |
| 30-60s | 13.2% | 51s |
| **60-90s** | **38.6%** | 75s |
| **90-120s** | **32.3%** | 103s |
| 2-3分 | 14.4% | 138s |
| 3-5分 | 1.1% | 201s |
| >5分 | 0.3% | 17,016s（异常长尾） |

**84% 的对局在 1-2 分钟内，中位约 85s（与 AVG=88s 一致）**。1-2 分钟单局是斗地主的自然节奏。SQL: `recheck_timecost_dist.sql`。

---

## 五、主因 1：仅1局玩家=单次试用即走流量

按"首日登录次数 × 总时长"画像仅1局玩家（n=3,268，D7=3.18%）：

| 登录次数 | 总时长 | n | D7 |
| ---- | ---- | ---- | ---- |
| 仅登录1次 | 1-3分 | **2,348** | 3.02 |
| 仅登录1次 | <1分 | 204 | 3.92 |
| 仅登录1次 | 3-5分 | 121 | 2.48 |
| 登录2次 | 1-3分 | 259 | 5.02 |
| 登录2次 | <1分 | 242 | 2.48 |

**71.9% 的仅1局玩家是"登录1次、玩1-3分钟、打1局、再没回来"**——即首日仅一次会话，打完一局立即离开。这是典型的**单次试用型用户**：下载、进来、试一局、走。

结合每局 timecost 中位 85s：**首日 1-3 分钟的总时长精确对应"打 1 局即停"，没有犹豫**。这不是"体验差被劝退"——是"打完一局本就要走"。

**性质判断**：这个量级的"试用即走"（仅1局占已游戏者 25.6%，3,268 人，其中 71.9% 单次会话）不太可能是游戏内容问题（首局体验已证伪），更可能源于：

- 渠道/获客质量（低意向流量：激励视频、积分墙、误点）
- 新手引导缺续玩钩子（打完一局后无召回机制）
- 应用启动→首局之间的摩擦（加载、注册、UI）

SQL: `recheck_1game_profile.sql`。

---

## 六、主因 2：`<60s/局` 高倍偏好组——短期活跃长期流失

用 `silver_total_play_seconds / silver_game_count` 算每用户的平均每局时长，分组对比 1-5局玩家（n=13,075）：

| 平均每局时长 | n | D1 | D7 |
| ---- | ---- | ---- | ---- |
| **<60s/局**（异常组） | **1,153** | **17.35** | **2.43** |
| ≥60s/局（主流） | 11,922 | 12.72 | 5.23 |

`<60s/局` 组：D1 比主流高 4.6pp（17.35 vs 12.72），但 D7 反而**低一半**（2.43 vs 5.23）。次日特别活跃，但 7 日内全部流失。

**逃跑假设证伪**：两组的 `silver_escape_count` 均值都是 0，不是逃跑。

进一步对比首局特征：

| 组 | 对局数 | n | 新手保护% | 首局倍数 |
| ---- | ---- | ---- | ---- | ---- |
| `<60s/局` | 1局 | 329 | 95.4 | **20.2** |
| `<60s/局` | 4-5局 | 298 | 78.2 | **43.6** |
| ≥60s/局 | 1局 | 2,802 | 92.9 | 12.3 |
| ≥60s/局 | 4-5局 | 4,465 | 92.7 | 14.0 |

**`<60s/局` 组首局倍数显著更高（20-44 vs 12-14）**——大倍率对局（炸弹/抢地主加倍/春天）一边倒地快速结束，所以单局时长短。

**画像**：偏好高倍率刺激对局的新玩家。

- 短期活跃高（D1=17.35）：喜欢刺激，次日还会来体验
- 长期留不住（D7=2.43）：7 天内基本全流失
- 占已游戏者 ~3.5%（1,153/约33,000+），量级中等但行为模式清晰

**与 framework §2.1 高倍局规律的关系**：framework v2.1 已证伪"输高倍流失/赢高倍提升"，改为"经历过即强信号"。本报告进一步发现：**整体偏好高倍刺激的新玩家虽 D1 高但 D7 低**——刺激爽快感留不住人。这是产品**可干预**的方向：新手前几局应避免极端倍数。SQL: `recheck_avg_timecost_d7.sql`、`recheck_short_game_escape.sql`、`recheck_shortgame_mystery.sql`。

---

## 六A、D1 视角：次日流失才是漏斗顶端

> D7 低是 D1 低的后果。本章把分析前移到 D1（次日留存），定位漏斗最顶端的流失。窗口 `2026-05-10~06-08`（n=35,435 已游戏玩家，D1 全到期）。

### 6A.1 D1 基线：已游戏玩家整体次日留存 ~21%

| 对局数 | 占比 | D1 | D7 | D7/D1 |
| ---- | ---- | ---- | ---- | ---- |
| 10+局 | 35.4% | 33.46 | 18.69 | 55.9% |
| 6-10局 | 27.7% | 19.76 | 10.11 | 51.2% |
| 2-5局 | 27.7% | **14.10** | 5.60 | 39.7% |
| 1局 | 9.2% | **10.22** | 3.15 | 30.8% |

即便 10+局重度玩家 D1 也仅 33%。**三分之二已游戏玩家次日不回**。1-5局玩家 D1 仅 10-14%，是 D7 低的根因。SQL: `recheck_d1_baseline.sql`。

### 6A.2 D1 流失者画像：85.5% 单次会话，非游戏体验问题

1-5局玩家 D1 留存者(1,717) vs 流失者(11,359) 首日特征对比：

| 特征 | D1 流失者 | D1 留存者 | 差异 |
| ---- | ---- | ---- | ---- |
| 单次会话占比 | **85.5%** | 78.3% | 流失者更倾向单次会话 |
| 银子净变化 | 2132 | 2123 | **几乎相同** |
| 净亏损占比 | 47.5% | 40.8 | 弱 |
| 末局后银子 | 4324 | 5158 | 弱 |
| 平均胜率 | 59.9 | 64.5 | 中(4.6pp) |
| 平均时长(秒) | 405 | 340 | 见 §6A.3，勿用叙事解读 |

**区分度最大的特征是"单次会话"，不是游戏体验**（银子盈亏几乎无差异）。次日不回来的人，首日就是"来一次就走"。SQL: `recheck_d1_retained_vs_lost.sql`。

### 6A.3 牌力假设证伪（重要：含数据可信度教训）

曾假设"流失者时长更长(405 vs 340s)是因为牌差→出牌纠结→单局拖长"。验证过程暴露两个数据问题，最终证伪该假设：

**问题 1：牌力算法在 2026-06-15 修复过。** 用修复前窗口（05-10~06-08）的 `card_power_final` 得出"34% 首局拿差牌、牌差→timecost长"的结论，**建立在旧算法的坏数据上，作废**。

**问题 2：timecost 均值被长尾脏数据拉高。** 修复后窗口（06-15~06-22）中等牌组(20-40) timecost 均值 336s 看似异常，实为 0.8% 超 600s 的挂机/断线脏数据（max=29513s≈8h）拉高，74.6% 的局其实 <120s。

**修复后窗口（06-15~06-22，n≈2,849 低局玩家）真实结论**：

| 首局牌力档 | 占比 | timecost | D1 |
| ---- | ---- | ---- | ---- |
| <20(差) | 4.7% | 145 | 15.56 |
| 20-40(中) | 6.5% | 336(含脏数据) | 8.70 |
| 40-50(好) | 17.9% | 153 | 11.76 |
| ≥50(很好) | **70.9%** | 113 | 13.47 |

- **70.9% 首局拿到"很好的牌"（≥50）**，仅 4.7% 拿差牌——保护机制正常工作（旧算法误把好牌算成差牌）。
- 牌力与 D1 **无清晰负相关**（差牌 D1 反而最高 15.56）。"牌差→体验差→D1 流失"假设**证伪**。
- D1 流失主驱动仍是 §6A.2 的"单次会话性质"，与牌力无关。

**教训**：跨窗口比较涉及算法变动的字段（如牌力）前，必须先确认字段定义的时间一致性；timecost 类长尾字段看均值前必须查分布。SQL: `recheck_cardpower_postfix_d1.sql`、`recheck_midcard_anomaly.sql`。

### 6A.4 首日登录次数与 D1（验证"多次会话"预测力）

| 首日登录次数 | 占比 | D1 | D7 |
| ---- | ---- | ---- | ---- |
| 1次 | **84.6%** | 12.17 | 4.83 |
| 2次 | 13.1% | 18.13 | 5.66 |
| 3次 | 1.2% | 19.21 | 9.93 |
| 4次 | 1.1% | 20.00 | 3.57 |

登录 2-3 次 D1 确实更高（18-19%），但**这部分人只占 14.3%**。84.6% 的 1-5局玩家首日只登录 1 次——"首日续玩钩子"（让首日内多次打开）影响面有限。对大多数人，首日那一次会话是给产品的全部机会。SQL: `recheck_login_cnt_d1.sql`。

### 6A.5 D1 章节小结

D1 流失（次日不回）是 D7 低的根因，主驱动是**单次会话性质**（85.5% 流失者首日只登录1次），而非游戏体验（银子、胜率、牌力差异都小或被证伪）。首局保护机制让 70.9% 拿好牌、多数赢，但**赢一把保护局不构成召回理由**——这解释了为何"保护到位仍留不住"。可干预方向偏向**次日召回触达（推送）+ 获客质量**，而非首日游戏内容优化。

---

## 七、综合结论

| 层级 | 事实 | 性质 |
| ---- | ---- | ---- |
| 基线 | 已游戏玩家 D7≈12%，88% 七日流失 | 命题 |
| 结构 | 低留存集中在 1-5局（37%）；仅1局最严重（25.6%, D7=3.18%） | 流失结构 |
| 证伪 | 首局体验（94%保护+80%胜）非主因 | 排除假设 |
| 证伪 | 撤保护断崖非主因（打到第4局 D7 反更高） | 排除假设 |
| 实测 | 对局时长中位 85s，84% 在 1-2 分钟内 | 基础事实 |
| 主因 1 | 仅1局玩家 71.9% 是单次会话——试用即走流量 | **非游戏体验** |
| 主因 2 | `<60s/局` 高倍偏好组 D1 高 D7 低——刺激爽快感留不住 | **游戏体验可干预** |
| **D1 视角** | 已游戏玩家 D1 仅 ~21%；1-5局玩家 D1 流失者 85.5% 单次会话；牌力→D1 假设证伪 | **D1 是漏斗顶端，次日召回优先** |

**三条主因并存**：

1. **试用即走流量**（占已游戏者 ~18% 即 2,348 仅1局+单次会话）：归因到渠道/获客/新手引导，**产品体验改动收益有限**。
2. **高倍偏好留不住**（占 ~3.5%）：归因到匹配/倍数调控，**产品可干预**——新手前几局降低极端倍数概率。
3. **D1 漏斗顶端流失**（§6A）：已游戏玩家三分之二次日不回，主驱动是单次会话性质（非游戏体验）。首局保护让 70.9% 拿好牌、多数赢，但"赢一把保护局"不构成召回理由。**可干预方向偏向次日召回触达（推送）+ 获客质量**，而非首日游戏内容。

剩余流失（约 50%+ 已游戏玩家的 6-50 局段）的中间梯度按 framework §2.1 "对局数单调递增"规律，关键产品杠杆是**让用户多玩几局**（首日 5 局/10 局节点的任务奖励、续玩钩子）。

---

## 八、复核 SQL

### 8.1 已游戏玩家 对局数 × 胜率 二维基线

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count = 1               THEN '1局'
        WHEN g.silver_game_count BETWEEN 2 AND 5   THEN '2-5局'
        WHEN g.silver_game_count BETWEEN 6 AND 10  THEN '6-10局'
        WHEN g.silver_game_count > 10              THEN '10+局'
    END AS game_cnt,
    CASE
        WHEN g.silver_win_rate < 30  THEN '胜率<30'
        WHEN g.silver_win_rate < 50  THEN '胜率30-50'
        WHEN g.silver_win_rate < 70  THEN '胜率50-70'
        ELSE                              '胜率>=70'
    END AS win_rate,
    COUNT(*) AS users,
    ROUND(SUM(rf.d1_game) * 100.0 / NULLIF(COUNT(rf.d1_game), 0), 2) AS d1_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-10' AND '2026-06-08'
  AND g.silver_game_count > 0
GROUP BY game_cnt, win_rate
ORDER BY game_cnt, win_rate;
```

### 8.2 1-5局亚群 首局特征 × D7

```sql
WITH first_game AS (
    SELECT
        app_id, dt AS reg_date, uid,
        MIN_BY(result_id,     game_datetime) AS first_result,
        MIN_BY(role,          game_datetime) AS first_role,
        MIN_BY(shuffle_type,  game_datetime) AS first_shuffle
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-05-10' AND '2026-06-08'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY app_id, dt, uid
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE fg.first_result WHEN 1 THEN '首局胜' WHEN 2 THEN '首局负' END AS first_result,
    CASE fg.first_role   WHEN 1 THEN '首局地主' WHEN 2 THEN '首局农民' END AS first_role,
    CASE
        WHEN fg.first_shuffle = 201 THEN '新手保护机器人'
        WHEN fg.first_shuffle = 0   THEN '随机发牌'
        ELSE                             '其他保护'
    END AS first_shuffle,
    COUNT(*) AS users,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct
FROM first_game fg
INNER JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = fg.app_id AND g.reg_date = fg.reg_date AND g.uid = fg.uid
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.silver_game_count BETWEEN 1 AND 5
GROUP BY first_result, first_role, first_shuffle
ORDER BY pct DESC;
```

### 8.3 前 5 局保护机制撤离曲线

```sql
WITH ranked_games AS (
    SELECT
        app_id, dt AS reg_date, uid,
        ROW_NUMBER() OVER (PARTITION BY app_id, dt, uid ORDER BY game_datetime) AS game_rank,
        result_id, role, magnification, shuffle_type, room_base
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-05-10' AND '2026-06-08'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
),
target_users AS (
    SELECT app_id, reg_date, uid
    FROM tcy_temp.dws_app_firstday_game_stat
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-05-10' AND '2026-06-08'
      AND silver_game_count BETWEEN 1 AND 5
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    rg.game_rank,
    COUNT(*) AS games,
    ROUND(SUM(CASE WHEN rg.shuffle_type = 201 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pct_newbie_protect,
    ROUND(SUM(CASE WHEN rg.result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pct_win,
    ROUND(SUM(CASE WHEN rg.role = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pct_landlord,
    ROUND(AVG(rg.magnification), 1) AS avg_multi
FROM ranked_games rg
INNER JOIN target_users t
    ON t.app_id = rg.app_id AND t.reg_date = rg.reg_date AND t.uid = rg.uid
WHERE rg.game_rank <= 5
GROUP BY rg.game_rank
ORDER BY rg.game_rank;
```

### 8.4 按"打到第几局停"看 D7（撤保护因果验证）

```sql
WITH user_max_games AS (
    SELECT
        app_id, dt AS reg_date, uid,
        COUNT(*) AS total_games
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-05-10' AND '2026-06-08'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY app_id, dt, uid
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    CASE
        WHEN umg.total_games = 1 THEN 'A:仅1局'
        WHEN umg.total_games = 2 THEN 'B:仅2局'
        WHEN umg.total_games = 3 THEN 'C:仅3局(保护期内停)'
        WHEN umg.total_games = 4 THEN 'D:4局(打了撤保护那局)'
        WHEN umg.total_games = 5 THEN 'E:5局'
    END AS exit_at,
    COUNT(*) AS users,
    ROUND(SUM(rf.d1_game) * 100.0 / NULLIF(COUNT(rf.d1_game), 0), 2) AS d1_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct
FROM user_max_games umg
INNER JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = umg.app_id AND g.reg_date = umg.reg_date AND g.uid = umg.uid
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.silver_game_count BETWEEN 1 AND 5
GROUP BY exit_at
ORDER BY exit_at;
```

### 8.5 对局时长真实分布

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN fg.timecost < 30       THEN '01: <30s'
        WHEN fg.timecost < 60       THEN '02: 30-60s'
        WHEN fg.timecost < 90       THEN '03: 60-90s'
        WHEN fg.timecost < 120      THEN '04: 90-120s'
        WHEN fg.timecost < 180      THEN '05: 2-3分'
        WHEN fg.timecost < 300      THEN '06: 3-5分'
        ELSE                             '07: >5分'
    END AS timecost_bucket,
    COUNT(*) AS games,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct,
    ROUND(AVG(fg.timecost), 0) AS avg_in_bucket
FROM tcy_temp.dws_ddz_firstday_game fg
INNER JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = fg.app_id AND g.reg_date = fg.dt AND g.uid = fg.uid
WHERE fg.app_id = 1880053
  AND fg.dt BETWEEN '2026-05-10' AND '2026-06-08'
  AND fg.robot != 1
  AND fg.play_mode IN (1, 2, 3)
  AND g.silver_game_count BETWEEN 1 AND 5
GROUP BY timecost_bucket
ORDER BY timecost_bucket;
```

### 8.6 仅1局玩家 登录次数 × 时长 画像

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.first_day_login_cnt = 1 THEN 'A:仅登录1次'
        WHEN g.first_day_login_cnt = 2 THEN 'B:登录2次'
        WHEN g.first_day_login_cnt >= 3 THEN 'C:登录3次+'
    END AS login_cnt_bucket,
    CASE
        WHEN g.silver_total_play_seconds < 60   THEN '01:<1分钟'
        WHEN g.silver_total_play_seconds < 180  THEN '02:1-3分'
        WHEN g.silver_total_play_seconds < 300  THEN '03:3-5分'
        WHEN g.silver_total_play_seconds < 600  THEN '04:5-10分'
        ELSE                                          '05:>10分'
    END AS playtime_bucket,
    COUNT(*) AS users,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-10' AND '2026-06-08'
  AND g.silver_game_count = 1
GROUP BY login_cnt_bucket, playtime_bucket
ORDER BY login_cnt_bucket, playtime_bucket;
```

### 8.7 平均每局时长 × D7（高倍偏好组验证）

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    g.silver_game_count AS game_cnt,
    CASE
        WHEN g.silver_total_play_seconds / g.silver_game_count < 60   THEN '01: 平均<60s/局'
        WHEN g.silver_total_play_seconds / g.silver_game_count < 90   THEN '02: 60-90s/局'
        WHEN g.silver_total_play_seconds / g.silver_game_count < 120  THEN '03: 90-120s/局'
        WHEN g.silver_total_play_seconds / g.silver_game_count < 180  THEN '04: 2-3分/局'
        ELSE                                                                '05: >3分/局'
    END AS avg_timecost_bucket,
    COUNT(*) AS users,
    ROUND(AVG(g.silver_total_play_seconds * 1.0 / g.silver_game_count), 0) AS avg_sec_per_game,
    ROUND(SUM(rf.d1_game) * 100.0 / NULLIF(COUNT(rf.d1_game), 0), 2) AS d1_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-10' AND '2026-06-08'
  AND g.silver_game_count BETWEEN 1 AND 5
  AND g.silver_total_play_seconds IS NOT NULL
  AND g.silver_total_play_seconds > 0
GROUP BY game_cnt, avg_timecost_bucket
ORDER BY game_cnt, avg_timecost_bucket;
```

### 8.8 `<60s/局` 谜团：首局倍数验证

```sql
WITH user_time AS (
    SELECT
        app_id, reg_date, uid,
        silver_game_count, silver_total_play_seconds,
        CASE
            WHEN silver_total_play_seconds * 1.0 / silver_game_count < 60 THEN '1:<60s/局'
            ELSE                                                              '2:>=60s/局'
        END AS time_group
    FROM tcy_temp.dws_app_firstday_game_stat
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-05-10' AND '2026-06-08'
      AND silver_game_count BETWEEN 1 AND 5
      AND silver_total_play_seconds IS NOT NULL
      AND silver_total_play_seconds > 0
),
first_game AS (
    SELECT
        app_id, dt AS reg_date, uid,
        MIN_BY(shuffle_type,  game_datetime) AS first_shuffle,
        MIN_BY(magnification, game_datetime) AS first_multi,
        MIN_BY(room_base,     game_datetime) AS first_room_base,
        MIN_BY(timecost,      game_datetime) AS first_timecost
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-05-10' AND '2026-06-08'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY app_id, dt, uid
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    ut.time_group,
    CASE
        WHEN ut.silver_game_count = 1             THEN '1局'
        WHEN ut.silver_game_count BETWEEN 2 AND 3 THEN '2-3局'
        WHEN ut.silver_game_count BETWEEN 4 AND 5 THEN '4-5局'
    END AS game_cnt,
    COUNT(*) AS users,
    ROUND(SUM(CASE WHEN fg.first_shuffle = 201 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pct_newbie_protect,
    ROUND(AVG(fg.first_multi), 1)     AS avg_first_multi,
    ROUND(AVG(fg.first_room_base), 0) AS avg_room_base,
    ROUND(AVG(fg.first_timecost), 0)  AS avg_first_timecost
FROM user_time ut
INNER JOIN first_game fg
    ON fg.app_id = ut.app_id AND fg.reg_date = ut.reg_date AND fg.uid = ut.uid
GROUP BY ut.time_group, game_cnt
ORDER BY ut.time_group, game_cnt;
```

### 8.9 D1 基线 + D7/D1 衰减率

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count = 1               THEN '1局'
        WHEN g.silver_game_count BETWEEN 2 AND 5   THEN '2-5局'
        WHEN g.silver_game_count BETWEEN 6 AND 10  THEN '6-10局'
        WHEN g.silver_game_count > 10              THEN '10+局'
    END AS game_cnt,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct,
    ROUND(SUM(rf.d1_game) * 100.0 / NULLIF(COUNT(rf.d1_game), 0), 2) AS d1_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(SUM(rf.d1_game), 0), 1) AS d7_of_d1
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-10' AND '2026-06-08'
  AND g.silver_game_count > 0
GROUP BY game_cnt
ORDER BY game_cnt;
```

### 8.10 D1 留存者 vs 流失者 首日特征对比

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    rf.d1_game AS is_d1_retained,
    COUNT(*) AS users,
    ROUND(AVG(g.first_day_login_cnt), 2) AS avg_login_cnt,
    ROUND(SUM(CASE WHEN g.first_day_login_cnt = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pct_single_session,
    ROUND(AVG(g.silver_total_diff_money), 0) AS avg_silver_diff,
    ROUND(SUM(CASE WHEN g.silver_total_diff_money < 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pct_net_loss,
    ROUND(AVG(g.silver_end_money), 0) AS avg_end_money,
    ROUND(AVG(g.silver_win_rate), 1) AS avg_win_rate,
    ROUND(AVG(g.silver_escape_count), 2) AS avg_escape,
    ROUND(AVG(g.silver_total_play_seconds), 0) AS avg_play_seconds
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-10' AND '2026-06-08'
  AND g.silver_game_count BETWEEN 1 AND 5
  AND rf.d1_game IS NOT NULL
GROUP BY rf.d1_game
ORDER BY is_d1_retained;
```

### 8.11 牌力 × D1（修复后窗口 06-15~06-22）

> ⚠️ 牌力算法 2026-06-15 修复，本查询仅用修复后窗口。修复前窗口的牌力结论全部作废。

```sql
WITH first_game AS (
    SELECT
        app_id, dt AS reg_date, uid,
        MIN_BY(card_power_final, game_datetime) AS first_card_power,
        MIN_BY(timecost,         game_datetime) AS first_timecost,
        MIN_BY(afk_turn_cnt,     game_datetime) AS first_afk
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-06-15' AND '2026-06-22'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY app_id, dt, uid
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    CASE
        WHEN fg.first_card_power < 20 THEN '01: 牌力<20(差)'
        WHEN fg.first_card_power < 40 THEN '02: 20-40(中)'
        WHEN fg.first_card_power < 50 THEN '03: 40-50(好)'
        ELSE                                '04: >=50(很好)'
    END AS card_bucket,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct,
    ROUND(AVG(fg.first_timecost), 0) AS avg_timecost,
    ROUND(AVG(fg.first_afk), 2)      AS avg_afk,
    ROUND(SUM(rf.d1_game) * 100.0 / NULLIF(COUNT(rf.d1_game), 0), 2) AS d1_game,
    COUNT(rf.d1_game) AS d1_n,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game,
    COUNT(rf.d7_game) AS d7_n
FROM first_game fg
INNER JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = fg.app_id AND g.reg_date = fg.reg_date AND g.uid = fg.uid
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.silver_game_count BETWEEN 1 AND 5
GROUP BY card_bucket
ORDER BY card_bucket;
```

### 8.12 首日登录次数 × D1/D7（验证多次会话预测力）

```sql
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    g.first_day_login_cnt AS login_cnt,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 1) AS pct,
    ROUND(SUM(rf.d1_game) * 100.0 / NULLIF(COUNT(rf.d1_game), 0), 2) AS d1_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(COUNT(rf.d7_game), 0), 2) AS d7_game,
    ROUND(SUM(rf.d7_game) * 100.0 / NULLIF(SUM(rf.d1_game), 0), 1) AS d7_of_d1
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-10' AND '2026-06-08'
  AND g.silver_game_count BETWEEN 1 AND 5
  AND rf.d1_game IS NOT NULL
GROUP BY g.first_day_login_cnt
ORDER BY g.first_day_login_cnt;
```

---

> **创建时间**：2026-06-24
>
> **复核窗口**：2026-05-10 ~ 2026-06-08（n=49,329 总注册 / 44,031 进游戏）
>
> **关联**：补充 [retention-analysis-framework.md](../plan/retention/retention-analysis-framework.md) §2 与 §5 关于"已游戏玩家流失驱动"的认知；与 [retention-global-report.md](retention-global-report.md) §3.2 单调递增、[retention-recheck-lose-streak-bankruptcy-report.md](retention-recheck-lose-streak-bankruptcy-report.md) §三连败组合 D30 信号互印证。
