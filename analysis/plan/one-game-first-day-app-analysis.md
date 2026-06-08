# APP 端注册当天只玩 1 局用户分析方案

> 本文档设计 APP 端新用户注册当天只玩 1 局的共性特征分析方案。分析重点是首日有对局用户中的 `1局用户` 与 `2局及以上用户` 的差异，`0局用户` 仅作为整体分布背景，不进入首局体验对比。

---

## 一、分析目标

### 1.1 核心问题

本次分析回答三个问题：

- APP 端注册用户中，注册当天只玩 1 局的人占比是否稳定，是否存在异常日期。
- 只玩 1 局用户相比 2 局及以上用户，在渠道、端类型、客户端语言、注册时段等画像上有什么明显偏向。
- 只玩 1 局用户的唯一一局是否存在可解释的体验特征，例如首局失败、输银过大、服务费压力、房间门槛不足、逃跑、高倍局等。

### 1.2 目标人群

| 人群 | 定义 | 用途 |
| ---- | ---- | ---- |
| 0局用户 | 注册当天真人对局数 = 0 | 背景分布，判断是否存在进入游戏前流失压力 |
| 1局用户 | 注册当天真人对局数 = 1 | 本次核心分析对象 |
| 2局及以上用户 | 注册当天真人对局数 >= 2 | 主对照组，代表首局后继续玩的用户 |

本次主分析只比较 `1局用户` 与 `2局及以上用户`。`0局用户` 没有游戏行为，不参与首局体验分析。

---

## 二、数据口径

### 2.1 数据表

| 表名 | 用途 | 关键字段 |
| ---- | ---- | ---- |
| `tcy_temp.dws_dq_app_daily_reg` | APP 端注册用户画像 | `uid`、`reg_date`、`reg_datetime`、`reg_group_id`、`reg_app_code`、`reg_channel_id`、`channel_category_name`、`first_day_login_cnt` |
| `tcy_temp.dws_ddz_firstday_game` | 注册当天对局明细 | `uid`、`dt`、`game_datetime`、`resultguid`、`play_mode`、`room_id`、`timecost`、`result_id`、`start_money`、`end_money`、`diff_money_pre_tax`、`room_fee`、`room_currency_lower`、`magnification`、`real_magnification`、`cut` |

### 2.2 基础过滤条件

```sql
r.app_id = 1880053
AND r.is_login_log_missing = 0
AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
AND r.reg_date BETWEEN '${start_date}' AND '${end_date}'
```

对局表过滤：

```sql
g.app_id = 1880053
AND g.robot != 1
AND g.dt BETWEEN '${start_date}' AND '${end_date}'
```

### 2.3 分组口径

按用户注册当天真人对局数分组：

```sql
CASE
    WHEN first_day_game_cnt = 0 THEN '0局'
    WHEN first_day_game_cnt = 1 THEN '1局'
    ELSE '2局及以上'
END AS game_cnt_group
```

对局数使用 `COUNT(DISTINCT resultguid)`，避免同一局多人行或重复日志造成计数膨胀。

---

## 三、分析路径

### 3.1 Step 1：每日首日局数分布

先建立背景基线，观察每天 APP 注册用户的首日局数结构：

- 注册人数
- 0局人数与占比
- 1局人数与占比
- 2局及以上人数与占比
- 1局用户在首日有对局用户中的占比

重点判断：

- 1局占比是否稳定。
- 是否有个别日期异常升高。
- 异常日期是否与渠道、端类型或数据更新有关。

### 3.2 Step 2：用户画像差异

仅比较 `1局用户` 与 `2局及以上用户`，看哪些画像维度在 1局用户中明显偏高。

建议维度：

| 维度 | 字段 | 分组方式 |
| ---- | ---- | ---- |
| 平台 | `reg_group_id` | Android / iOS |
| 客户端语言 | `reg_app_code` | `zgda` / `zgdx` / 其他 |
| 渠道分类 | `channel_category_name` | 按渠道分类 |
| 具体渠道 | `reg_channel_id` | Top 渠道 |
| 注册小时 | `reg_datetime` | 0-23 点 |
| 注册时段 | `reg_datetime` | 凌晨 / 上午 / 下午 / 晚间 |
| 首日登录次数 | `first_day_login_cnt` | 1次 / 2-5次 / 5次以上 |

对每个维度输出：

- 1局用户人数
- 2局及以上用户人数
- 1局用户占比
- 2局及以上用户占比
- `lift = 1局占比 / 2局及以上占比`

建议只重点解读满足以下条件的特征：

- 1局用户人数足够大，避免小样本误判。
- `lift >= 1.2` 或 `lift <= 0.8`。
- 连续多天或多个相邻日期稳定出现。

### 3.3 Step 3：首局体验差异

对 `1局用户` 取唯一一局，对 `2局及以上用户` 取首局，比较首局体验差异。

核心指标：

| 类型 | 指标 | 解释 |
| ---- | ---- | ---- |
| 首局选择 | `play_mode`、`room_id` | 判断是否集中在某些玩法或房间 |
| 对局耗时 | `timecost` 的均值、中位数、P90、P95、最大值、长局占比 | 过短可能表示异常、逃跑；过长可能表示卡顿、托管超时、断线重连或匹配挂起；必须识别是否由极端长尾拉高均值 |
| 胜负结果 | `result_id` | 首局失败是否显著更容易只玩 1 局 |
| 经济变化 | `diff_money_pre_tax - room_fee` | 近似首局净收益 |
| 首局后余额 | `end_money` | 判断是否还有继续游戏能力 |
| 房间门槛 | `end_money < room_currency_lower` | 首局后是否低于本房间继续准入门槛 |
| 服务费压力 | `room_fee / start_money` | 服务费占初始资产比例 |
| 倍数压力 | `magnification`、`real_magnification` | 是否经历高倍局 |
| 逃跑行为 | `cut < 0` | 是否存在逃跑或异常退出 |

建议派生指标：

```sql
CASE WHEN result_id = 1 THEN 1 ELSE 0 END AS is_win,
diff_money_pre_tax - room_fee AS net_money_change,
CASE WHEN end_money < room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold,
CASE WHEN start_money > 0 THEN room_fee * 1.0 / start_money END AS fee_pressure,
CASE WHEN cut < 0 THEN 1 ELSE 0 END AS is_escape,
CASE WHEN timecost > 200 THEN 1 ELSE 0 END AS is_long_timecost
```

耗时异常需要按房间拆开看，不能只看整体均值，也不能只看平均值。判断逻辑：

- 对局耗时优先同时看 `avg_first_timecost`、`p50_first_timecost`、`p90_first_timecost`、`p95_first_timecost`、`p99_first_timecost`、`max_first_timecost`、`long_200_timecost_rate`、`long_300_timecost_rate` 和 `long_600_timecost_rate`。
- 如果平均值显著偏高，但中位数接近正常，仅 P95、最大值或 `timecost > 200秒` 占比异常，说明更可能是少量几千秒或几万秒极端局拉高均值，需要优先排查异常长尾明细。
- 如果中位数、P90、P95 同时偏高，才说明该组用户的典型首局体验本身就偏慢。
- 如果某房间 `1局用户` 首局耗时远高于 `2局及以上用户`，但同房间多局用户耗时正常，优先怀疑特定用户链路上的卡顿、断线重连、托管超时或匹配挂起。
- 如果同房间两组用户耗时都高，才优先怀疑房间日志口径或玩法天然耗时更长。
- 对 `timecost > 200秒` 且首日无后续对局的用户，应输出明细用于客户端和服务端日志排查。

建议额外补充耗时异常占比，而不是只看均值。重点看以下分层：

| 指标 | 解释 |
| ---- | ---- |
| `timecost > 200秒` 占比 | 判断异常耗时是不是偶发。 |
| `timecost > 300秒` 占比 | 判断是否已从偶发转为低频甚至中频。 |
| `timecost > 600秒` 占比 | 判断是否存在极端挂起或长时间托管。 |
| 95分位 / 99分位耗时 | 避免均值被少量超长局拉高，辅助判断尾部风险。 |

如果 `1局用户` 的异常耗时占比明显高于 `2局及以上用户`，且在 4484、22039 等房间持续出现，就更像中高频问题；如果只集中在少量日期、少量渠道或极少数用户，则更像偶发问题。

### 3.4 Step 4：高风险组合

在单维度差异之后，再识别组合特征。优先看以下组合：

| 组合 | 可能解释 |
| ---- | ---- |
| 首局失败 + 首局净亏损 + 首局后低于房间门槛 | 首局挫败叠加资产不足 |
| 首局失败 + 地主角色 + 高倍局 | 地主高方差体验导致快速退出 |
| 高服务费压力 + 初始资产低 | 新手资产与房间成本不匹配 |
| 只登录 1 次 + 只玩 1 局 | 低意愿或首局后立即离开 |
| 某渠道 + 某客户端语言 + 1局占比高 | 流量质量或客户端体验问题 |

组合分析不要一次交叉过多维度，优先使用 2 到 3 个变量，避免样本被切得过碎。

### 3.5 Step 5：首局失败后继续行为

在整体首局体验对比之后，需要单独下钻首局失败用户，比较 `首局失败后停止` 与 `首局失败后继续第2局`。整体 `1局用户 vs 2局及以上用户` 会混入首局胜利用户，适合判断大方向，但不能回答“同样首局失败，为什么有人继续第 2 局”。

建议补跑维度：

| 维度 | 指标 | 解释 |
| ---- | ---- | ---- |
| 失败后资产门槛 | 首局失败后是否仍高于当前房间门槛 | 判断是否还具备继续同房间游戏能力 |
| 资产安全垫 | 首局失败后的剩余资产 / 房间门槛 | 判断继续用户是否有更厚资产缓冲 |
| 地主角色 | 首局失败是否为地主 | 地主失败可能带来更强挫败感和更大损失 |
| 高倍体验 | 首局失败是否高倍、炸弹、春天 | 判断失败强度是否影响继续意愿 |
| 继续速度 | 首局失败后到第 2 局的时间间隔 | 判断是否存在顺畅续局或中途卡点 |
| 第 2 局迁移 | 第 2 局是否换玩法、换房间或降档 | 判断系统是否把失败用户引导到更合适场次 |
| 第 2 局反馈 | 第 2 局胜率和净收益 | 判断继续用户是否被后续体验拉回 |

这一步的目标不是重复证明首局失败有风险，而是找到首局失败后还能留住用户的条件，例如“失败后仍高于门槛”“快速进入第 2 局”“第 2 局换到更低风险房间”等。

### 3.6 Step 6：注册赠送与失败破产容错模型

在本次口径中，首局 `start_money` 对应原始表 `olddeposit`，可视为注册赠送银子或新手指引资产。需要用服务费压力反推首局前资产，并结合房间底分、服务费和最低携银门槛，判断首局失败是否会在规则上直接导致破产锁定。

建议分析项：

| 分析项 | 计算方式 | 解释 |
| ---- | ---- | ---- |
| 首局前资产反推 | `room_fee / fee_pressure` | 用固定服务费和服务费压力反推注册赠送资产水平 |
| 失败安全边界 | `start_money - room_fee - room_currency_lower` | 扣除服务费后，首局最多还能亏多少且不低于当前房间门槛 |
| 农民破产倍数 | `FLOOR(安全边界 / room_base) + 1` | 农民失败达到多少倍会低于当前房间门槛 |
| 地主破产倍数 | `FLOOR(安全边界 / (room_base * 2)) + 1` | 地主失败达到多少倍会低于当前房间门槛 |
| 破产锁定解释 | `result_id = 2`、`role`、`magnification`、`end_money < room_currency_lower` | 判断首局失败后低于门槛是否由经济规则必然导致 |

这一步用于区分两类问题：

- **经济规则必然导致**：注册赠送资产、服务费、底分和准入门槛共同决定，首局失败后自然跌破门槛。
- **产品链路没有兜底**：跌破门槛后没有顺畅引导到低门槛房间、练习场或第 2 局保护，导致用户停止。

注意：`room_currency_lower` 必须使用数据中的房间实际门槛，不能用经验值替代。以最新数据看，经典初级房 `12074`、`4484` 的首局失败用户平均门槛约为 `2,500` 银子，而不是 `1,000` 银子；这会显著压缩失败安全边界。

---

## 四、SQL 产出清单

### 4.1 用户级基础宽表

生成每个 APP 注册用户的首日对局摘要，作为后续所有分析的公共 CTE。

输出字段：

- `uid`
- `reg_date`
- 注册画像字段
- `first_day_game_cnt`
- `game_cnt_group`
- `first_game_datetime`
- `minutes_to_first_game`

### 4.2 每日分布 SQL

按 `reg_date` 统计 0局、1局、2局及以上分布。

输出字段：

- `reg_date`
- `reg_user_cnt`
- `zero_game_user_cnt`
- `one_game_user_cnt`
- `multi_game_user_cnt`
- `zero_game_rate`
- `one_game_rate`
- `multi_game_rate`
- `one_game_rate_among_played`

### 4.3 画像维度差异 SQL

分别按平台、客户端语言、渠道分类、具体渠道、注册时段、首日登录次数输出差异。

输出字段：

- `dimension_name`
- `dimension_value`
- `one_game_user_cnt`
- `multi_game_user_cnt`
- `one_game_share`
- `multi_game_share`
- `lift`

### 4.4 首局体验差异 SQL

比较 1局用户的唯一一局与 2局及以上用户的首局。

输出字段：

- `game_cnt_group`
- 首局玩法分布
- 首局房间分布
- 首局胜率
- 首局平均耗时、中位数耗时、P90/P95 耗时、最大耗时、长局占比
- 首局平均净收益
- 首局后低于房间门槛占比
- 服务费压力分布
- 高倍局占比
- 逃跑占比
- 耗时异常占比（如 `timecost > 200秒`、`timecost > 300秒`、`timecost > 600秒`）
- 耗时分位数（P95、P99）

补充检查：

- 按 `room_id` 对比 `avg_first_timecost`、`p50_first_timecost`、`p95_first_timecost`、`p99_first_timecost`、`long_200_timecost_rate`、`long_300_timecost_rate` 和 `long_600_timecost_rate`，识别特定房间耗时异常。
- 按 `room_id` 和 `game_cnt_group` 对比异常耗时占比，判断是偶发问题还是中高频问题。
- 对平均值明显高于中位数的房间，优先输出超长尾明细，避免把少量异常局误判成整体体验偏慢。
- 对 `timecost > 200秒` 且首日无后续对局的用户，输出明细用于日志排查。

### 4.5 高风险组合 SQL

输出 1局用户中高风险组合的人数、占比，并与 2局及以上用户对比。

建议组合：

- `首局失败 × 首局后低于房间门槛`
- `首局失败 × 高倍局`
- `首局失败 × 地主角色`
- `高服务费压力 × 首局净亏损`
- `渠道分类 × 客户端语言`

### 4.6 首局失败后继续行为 SQL

只分析首局失败用户，输出 `首局失败后停止` 与 `首局失败后继续第2局` 两组差异。

输出字段：

- `first_loss_continue_group`
- `user_count`
- `avg_first_timecost`
- `avg_first_net_money_change`
- `first_below_room_threshold_rate`
- `avg_first_end_money_to_threshold`
- `avg_first_fee_pressure`
- `first_high_magnification_rate`
- `first_landlord_rate`
- `first_has_bomb_rate`
- `first_has_spring_rate`
- `avg_seconds_to_second_game`
- `second_game_change_room_rate`
- `second_game_change_play_mode_rate`
- `second_game_win_rate`
- `avg_second_net_money_change`

### 4.7 经济容错模型 SQL

基于首局失败用户的房间、角色、底分、服务费、首局前资产和房间实际门槛，输出失败后破产锁定的规则解释。

输出字段：

- `game_cnt_group`
- `play_mode_name`
- `room_id`
- `role`
- `user_count`
- `avg_start_money`
- `avg_room_fee`
- `avg_fee_pressure`
- `avg_room_base`
- `avg_room_currency_lower`
- `avg_loss_tolerance`
- `already_below_safe_boundary_rate`
- `farmer_break_even_magnification`
- `landlord_break_even_magnification`
- `below_room_threshold_rate`

---

## 五、结论输出模板

最终报告建议按以下结构输出：

```text
1. 结论摘要
2. APP 新用户首日局数分布
3. 1局用户画像共性
4. 1局用户首局体验共性
5. 高风险组合人群
6. 可能原因判断
7. 产品 / 运营建议
8. 附录：SQL 与口径说明
```

结论需要区分三类判断：

- **用户来源问题**：集中在渠道、端类型、客户端语言。
- **首局体验问题**：集中在失败、亏损、高倍、逃跑、耗时异常。
- **经济门槛问题**：首局后资产不足、服务费压力过高、房间准入不匹配。

---

## 六、注意事项

- 不把 0局用户纳入首局体验分析，因为 0局用户没有完成对局。
- 对局数用 `COUNT(DISTINCT resultguid)`，避免重复日志影响分组。
- 主对照组使用 `2局及以上用户`，不要混入 0局用户，否则会稀释首局体验差异。
- 渠道维度需要同时看人数和占比，避免只因渠道规模大而误判。
- `lift` 只说明相关性，不直接说明因果。产品结论需要结合日期趋势、渠道变化和客户端版本发布节奏验证。
- 涉及日期字段时，`dws_dq_app_daily_reg.reg_date` 与 `dws_ddz_firstday_game.dt` 均为 `date` 类型，查询中保持日期格式一致。
