# 斗地主 App 新增用户留存分析框架

> 本文档为同城游平台·斗地主游戏的新增用户留存分析完整框架，涵盖业务背景、数据基础、指标定义、分析方法论、取数SQL及行动建议，旨在系统化地识别影响新用户留存的关键因子并指导产品优化。

---

## 目录

1. [业务背景与新手流程](#一业务背景与新手流程)
2. [数据基础](#二数据基础)
3. [留存基础指标体系](#三留存基础指标体系)
4. [游戏核心玩法指标](#四游戏核心玩法指标)
5. [经济系统指标](#五经济系统指标)
6. [用户行为指标](#六用户行为指标)
7. [分析方法论](#七分析方法论)
8. [取数SQL](#八取数sql)
9. [结论模板与行动建议框架](#九结论模板与行动建议框架)
10. [指标速查表](#十指标速查表)

---

## 一、业务背景与新手流程

### 1.1 产品定位

同城游是一款地方棋牌游戏平台，包含双扣、掼蛋等地方棋牌游戏及斗地主、川麻等全国通用游戏。斗地主作为平台内的独立游戏 App，通过同城游大厅唤起。

### 1.2 新手流程

```
用户通过信息流买量/应用市场等渠道下载同城游 APP
  → 完成账号注册与登录
    → 进入同城游游戏大厅（展示各游戏 icon）
      → 点击斗地主 icon，唤起斗地主游戏 App
        → 首次进入，服务端生成游戏用户信息表（等同游戏内注册）
          → 领取新手礼包（游戏货币 + 游戏道具）
            → 默认进入经典玩法，走新手配牌
              → 完成对局后，可选择继续游戏或返回游戏大厅
```

**关键节点：**
- 新增用户口径：首次进入斗地主 App 并在服务端生成游戏用户信息记录的用户。
- 新手配牌：首次对局使用特殊配牌逻辑，分析时需注意首局数据的特殊性。
- 新手礼包：包含初始游戏货币（银子）和道具，影响用户的经济起点。

### 1.3 玩法说明

**支持三种玩法：**

| 玩法 | 特点 |
|------|------|
| 经典玩法 | 标准斗地主规则，新手默认进入 |
| 不洗牌玩法 | 保留上局出牌顺序发牌，牌序有延续性 |
| 赖子玩法 | 存在万能牌（赖子），增加随机性和策略性 |

**对局流程与倍数机制：**

```
叫地主（固定3分）→ 抢地主 → 加倍/超级加倍 → 出牌对局 → 结算
```

| 倍数因子 | 作用范围 | 取值规则 |
|---------|---------|---------|
| 叫地主 | 公共 | 固定 3 分，不存在 1/2 分 |
| 抢地主 | 公共 | 每抢一次公共倍数 ×2；`grab_landlord_bet`：3=无人抢 / 6=1人抢 / 12=2人抢 |
| 加倍/超级加倍 | 仅对自己 | `magnification_stacked`：1=不加倍 / 2=加倍 / 4=超级加倍 |
| 炸弹 | 公共 | 每个炸弹公共倍数 ×2；`bomb_bet/2` = 本局炸弹个数 |
| 春天/反春 | 公共 | `complete_victory_bet`=2 时存在春天或反春，公共倍数 ×2 |

**倍数计算方式：**

- `magnification` = 该战绩玩家的个人理论总倍数，已包含所有公共因子（叫地主/抢地主/炸弹/春天）**以及**个人加倍因子（magnification_stacked）
- 本局公共倍数 = `magnification / magnification_stacked`（剥离个人加倍部分）
- 实际倍数 = `ABS(diff_money) / room_base`（因玩家携银可能不足以支付理论输赢金额，实际倍数可能低于理论倍数）
- 理论倍数与实际倍数的差异来源：当输家银子不够赔付时，赢家实际获得的银子 < 理论应得银子

---

## 二、数据基础

### 2.1 数据表概览

本分析基于以下两张核心日志表：

#### 表1：`fact_game_reg` — 游戏用户注册表

| 字段 | 类型 | 说明 |
|------|------|------|
| dt | STRING | 注册日期（分区字段） |
| uid | BIGINT | 玩家 ID |

#### 表2：`fact_game_combatgains_history` — 游戏对局日志表

| 字段                    | 类型     | 说明                                                                       |
| --------------------- | ------ | ------------------------------------------------------------------------ |
| dt                    | STRING | 对局日期（分区字段）                                                               |
| resultguid            | STRING | 本局战绩 ID（可用于局内排序）                                                         |
| timecost              | INT    | 对局耗时（秒）                                                                  |
| room                  | STRING | 房间号                                                                      |
| room_base             | INT    | 房间底分                                                                     |
| room_fee              | INT    | 房间服务费                                                                    |
| room_currency_lower   | BIGINT | 进入房间所需最少携银                                                               |
| room_currency_upper   | BIGINT | 进入房间最大携银                                                                 |
| uid                   | BIGINT | 玩家 ID                                                                    |
| robot                 | INT    | 1=机器人，其他=真人                                                              |
| role                  | INT    | 1=地主，2=农民                                                                |
| chairno               | INT    | 座位号（0/1/2），0号位优先叫地主                                                      |
| result_id             | INT    | 1=获胜，2=失败                                                                |
| start_money           | BIGINT | 对局前银子数量                                                                  |
| end_money             | BIGINT | 对局后银子数量                                                                  |
| diff_money            | BIGINT | 本局输赢银子（不含服务费）                                                            |
| cut                   | BIGINT | 逃跑罚没银子（<0 代表存在逃跑行为）                                                      |
| safebox_deposit       | BIGINT | 保险箱存银                                                                    |
| magnification         | INT    | 该战绩玩家的个人理论总倍数（含公共倍数 + 个人加倍），公共倍数 = magnification / magnification_stacked |
| grab_landlord_bet     | INT    | 抢地主倍数：3=无人抢 / 6=1人抢 / 12=2人抢                                             |
| magnification_stacked | INT    | 个人加倍：1=不加倍 / 2=加倍 / 4=超级加倍                                               |
| complete_victory_bet  | INT    | 春天/反春标记：2=存在春天或反春                                                        |
| bomb_bet              | INT    | 炸弹倍数，`bomb_bet/2` = 炸弹个数                                                 |

### 2.2 数据使用注意事项

1. **过滤机器人**：所有分析需过滤 `robot = 1` 的记录，仅保留真人玩家数据。
2. **逃跑局处理**：`cut < 0` 的对局为逃跑局，对局结果可能不完整，部分指标（如倍数、胜负）需谨慎使用。
3. **新手配牌**：首局使用特殊配牌，分析胜率时需单独标注首局数据。
4. **留存判定（游戏留存口径）**：因无独立登录日志，以用户在 `fact_game_combatgains_history` 中是否存在对局记录作为活跃/留存判定依据。分母为「当日新增**且注册当日有对局**的用户数」，分子为「第 N 日有对局的用户数」。
5. **magnification 字段含义**：`magnification` 记录的是该战绩玩家的**个人理论总倍数**（已包含公共倍数 + 个人加倍/超级加倍），**不是**公共倍数。公共倍数需通过 `magnification / magnification_stacked` 计算。理论倍数与实际倍数（`ABS(diff_money) / room_base`）的差异在于玩家携银可能不够支付全额输赢。
6. **保险箱银子**：`safebox_deposit` 中的银子不参与对局，分析实际可用银子时需排除。

---

## 三、留存基础指标体系

### 3.1 留存率定义

| 指标名称           | 定义                       | 计算公式                                 |
| -------------- | ------------------------ | ------------------------------------ |
| 游戏次日留存（Day1）   | 注册当日有对局的新用户中，次日仍有对局的比例   | `次日有对局的新用户数 / 当日新增且有对局的用户数 × 100%`   |
| 游戏3日留存（Day3）   | 注册当日有对局的新用户中，第3天仍有对局的比例  | `第3日有对局的新用户数 / 当日新增且有对局的用户数 × 100%`  |
| 游戏7日留存（Day7）   | 注册当日有对局的新用户中，第7天仍有对局的比例  | `第7日有对局的新用户数 / 当日新增且有对局的用户数 × 100%`  |
| 游戏14日留存（Day14） | 注册当日有对局的新用户中，第14天仍有对局的比例 | `第14日有对局的新用户数 / 当日新增且有对局的用户数 × 100%` |
| 游戏30日留存（Day30） | 注册当日有对局的新用户中，第30天仍有对局的比例 | `第30日有对局的新用户数 / 当日新增且有对局的用户数 × 100%` |

**口径说明：**

- **新增用户**：以 `fact_game_reg` 表中首次出现的 uid 为准，即首次进入斗地主 App 并在服务端生成注册记录的用户。
- **分母（游戏留存口径）**：因无独立登录日志表，留存统一采用**游戏留存口径**。分母为「注册当日**且在当日有真人对局记录**的新用户数」，而非全部注册用户数。注册但当日未参与任何对局的用户不纳入分母。
- **分子**：第 N 日在 `fact_game_combatgains_history` 中存在真人对局记录（`robot != 1`）的新用户数。
- **自然日**：以 dt 分区字段为准，北京时间自然日。

### 3.2 留存分层维度

| 分层维度 | 拆分方式 | 分析目的 |
|---------|---------|---------|
| 注册渠道 | 信息流买量 / 应用市场 / 社交分享 / 自然流量 | 评估各渠道获客质量，指导投放策略 |
| 设备类型 | iOS / Android | 不同平台用户行为差异 |
| 注册时段 | 凌晨 / 上午 / 下午 / 晚间 / 深夜 | 识别活跃时段与用户类型 |
| 首日对局数分组 | 1局 / 2-5局 / 6-10局 / 11-20局 / 20局+ | 对局投入度与留存关系 |
| 首日胜率分组 | 0-30% / 30-50% / 50-70% / 70%+ | 胜负体验与留存关系 |

### 3.3 留存趋势监控

- **日粒度留存曲线**：持续追踪每日新增 cohort 的留存衰减曲线，观察是否存在异常波动。
- **周均值平滑**：采用 7 日移动平均消除自然周期波动，观察留存趋势走向。
- **版本对比**：在 App 版本迭代前后，对比同一时期 cohort 的留存差异，量化版本改动效果。

---

## 四、游戏核心玩法指标

> 斗地主的核心体验围绕「叫地主 → 抢地主 → 加倍 → 出牌对抗 → 倍数结算」展开，以下指标直接影响玩家的情绪曲线和留存意愿。

### 4.1 倍数相关指标

倍数是斗地主最核心的博弈机制，高倍局带来的刺激感与风险感直接影响新用户的留存体验。

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
|---------|-------------|---------|---------|----------------|
| 首日平均理论倍数 | `AVG(magnification)`，magnification 已含个人加倍 | magnification | 分区间（3 / 6 / 12 / 24 / 48+） | 适中的倍数体验可能对应最优留存 |
| 首日最大理论倍数 | `MAX(magnification)` | magnification | 分区间（≤6 / 6-24 / 24-96 / 96+） | 经历超高倍局可能产生两极分化 |
| 首日平均公共倍数 | `AVG(magnification / magnification_stacked)`，剥离个人加倍 | magnification, magnification_stacked | 分区间（3 / 6 / 12 / 24+） | 公共倍数反映牌局本身的激烈程度 |
| 首日平均实际倍数 | `AVG(ABS(diff_money) / room_base)` | diff_money, room_base | 分区间 | 实际倍数受携银限制可能低于理论倍数 |
| 理论与实际倍数差异率 | `AVG(1 - ABS(diff_money) / (room_base * magnification))` | 计算字段 | 百分比 | 差异大说明携银不足局面频繁，经济压力大 |
| 低倍局占比 | 首日 `magnification <= 6` 的局数占比 | magnification | 百分比 | 低倍局多说明对局波动小，稳健 |
| 中倍局占比 | 首日 `magnification > 6 AND <= 24` 的局数占比 | magnification | 百分比 | 中倍局兼顾刺激与风险 |
| 高倍局占比 | 首日 `magnification > 24` 的局数占比 | magnification | 百分比 | 高倍局过多可能导致银子快速耗尽 |
| 高倍局胜负与留存 | 高倍局（magnification>24）中的胜率 | magnification, result_id | 赢高倍 vs 输高倍 | 赢高倍提升留存；输高倍是流失高危信号 |
| 抢地主发生率 | `grab_landlord_bet > 3` 的局数占比 | grab_landlord_bet | 百分比 | 抢地主频率反映对局的竞争激烈度 |
| 玩家加倍率 | `magnification_stacked > 1` 的局数占比 | magnification_stacked | 百分比 | 主动加倍反映新手的风险偏好 |
| 超级加倍率 | `magnification_stacked = 4` 的局数占比 | magnification_stacked | 百分比 | 超级加倍是极端风险行为 |
| 春天/反春发生率 | `complete_victory_bet = 2` 的局数占比 | complete_victory_bet | 百分比 | 春天是极端体验，赢/输春天对情绪影响显著 |
| 平均炸弹数 | `AVG(bomb_bet / 2)` | bomb_bet | 均值 | 炸弹多的局通常倍数更高、波动更大 |

**分析要点：**

- `magnification` 记录的是玩家个人理论总倍数（含加倍/超级加倍），分析公共倍数时需除以 `magnification_stacked`。
- 将新用户按「首日平均理论倍数」分为低倍组（magnification 均值 ≤6）/ 中倍组（6-24）/ 高倍组（>24），对比各组 Day1 ~ Day7 留存曲线。
- 重点分析「首日经历过高倍局（magnification>24）且输掉」的用户群体，预期其流失概率最高。
- 关注「理论与实际倍数差异率」，差异率高说明携银不足的情况频繁，玩家在承受超出自身经济能力的对局。
- 区分主动倍数行为（玩家加倍 magnification_stacked）与被动倍数（炸弹 bomb_bet、春天 complete_victory_bet），分析新手对主动加倍的使用是否过于激进。
- 公共倍数的最小值为 3（叫地主固定 3 分，无人抢无炸无春天且不加倍时 magnification = 3）。

### 4.2 胜负相关指标

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
|---------|-------------|---------|---------|----------------|
| 首日胜率 | `SUM(result_id=1) / COUNT(*) × 100%` | result_id | 分区间（0-30% / 30-50% / 50-70% / 70%+） | 胜率低于 30% 的新用户流失风险极高 |
| 首 N 局胜率 | 前 3 局 / 前 5 局 / 前 10 局的胜率 | result_id（按 resultguid 排序取前 N） | 与首日胜率交叉对比 | 前几局体验对「第一印象」影响最大 |
| 首局胜负 | 按 resultguid 排序的第一局是否获胜 | result_id | 胜 vs 负 | 首局获胜（含新手配牌加持）应显著提升次留 |
| 连胜最大长度 | 首日最长连续获胜局数 | result_id 序列 | 分组（0 / 1-2 / 3-5 / 5+） | 连胜体验带来正向情绪累积 |
| 连败最大长度 | 首日最长连续失败局数 | result_id 序列 | 分组（0-1 / 2-3 / 4-5 / 5+） | 连败 ≥3 局是关键流失预警信号 |
| 当地主胜率 | `SUM(role=1 AND result_id=1) / SUM(role=1)` | role, result_id | 对比农民胜率 | 地主胜率低意味着叫牌能力不足或匹配难度高 |
| 当农民胜率 | `SUM(role=2 AND result_id=1) / SUM(role=2)` | role, result_id | 对比地主胜率 | 农民胜率相对稳定 |
| 角色分布 | 当地主次数 / 当农民次数 | role | 对比预期值（约 1:2） | 偏差过大影响公平体验感 |
| 0号位占比 | `chairno = 0` 的局数占比 | chairno | 百分比 | 0号位优先叫地主，占比应接近 1/3 |

**分析要点：**

- 首局使用新手配牌，首局胜率预期偏高。需单独分析「首局胜率」和「去除首局后的胜率」。
- 寻找「连败耐受阈值」——连败多少局后流失概率急剧上升（通过分组对比法）。
- 地主胜率与农民胜率的差值可反映匹配系统对新手的友好程度。

### 4.3 场次与时长指标

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
|---------|-------------|---------|---------|----------------|
| 首日对局数 | 注册当日完成的总对局数 | COUNT(*) | 分区间（1 / 2-5 / 6-10 / 11-20 / 20+） | 存在最优区间，过少未形成习惯，过多可能疲劳 |
| 首日总对局时长 | `SUM(timecost)` 秒 | timecost | 分区间（<5min / 5-15min / 15-30min / 30min+） | 总投入时间是沉浸度的直接体现 |
| 平均单局时长 | `AVG(timecost)` 秒 | timecost | 分区间（<120s / 120-300s / 300-480s / 480s+） | 过短可能是秒退/逃跑，过长可能是卡顿 |
| 首局耗时 | 第一局的 timecost | timecost（首局） | 分区间 | 首局耗时反映新手配牌局的体验节奏 |

**分析要点：**

- 找到「最优首日对局数区间」——对局数在该区间内的用户留存率最高。
- 将平均单局时长异常短（<60s）的对局与逃跑行为（cut<0）交叉分析。

### 4.4 场次类型指标

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
|---------|-------------|---------|---------|----------------|
| 游戏模式偏好 | 经典 / 不洗牌 / 赖子各模式的局数占比 | room（需映射到模式） | 按模式分组对比留存 | 不同模式的节奏和随机性不同 |
| 底分房间分布 | 按 `room_base` 分组的局数分布 | room_base | 低分场 / 中分场 / 高分场 | 新手进入高分场可能被快速淘汰 |
| 房间切换次数 | 首日在不同 room 间切换的次数 | COUNT(DISTINCT room) | 分组（1 / 2-3 / 4+） | 频繁切换可能是体验不佳的信号 |
| 最高底分房间 | 首日进入过的最高 `room_base` | MAX(room_base) | 分组 | 选择过高底分是冲动行为信号 |
| 房间入门银子利用率 | `start_money / room_currency_lower` | start_money, room_currency_lower | 分区间 | 携银刚好达到门槛的用户风险更高 |

**分析要点：**

- 新手默认进入经典玩法，分析「首日是否尝试其他玩法」与留存的关系。
- 分析新手选择底分房间是否合理——携银 / 房间最低携银的比值过小（如 < 3 倍）意味着高风险。

---

## 五、经济系统指标

> 银子是斗地主的核心经济资源。新手礼包提供初始银子，后续通过对局赢取或亏损，银子的变动直接影响玩家情绪和持续游玩能力。

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
|---------|-------------|---------|---------|----------------|
| 初始银子数 | 首局的 `start_money`（含新手礼包） | 首局 start_money | 绝对值 | 反映新手经济起点 |
| 首日末尾银子数 | 末局的 `end_money` | 末局 end_money | 绝对值 | 首日结束时的经济状态 |
| 首日银子净变化 | `末局 end_money - 首局 start_money` | 首末局 | 大幅亏损 / 小幅亏损 / 持平 / 盈利 | 净亏损用户预期留存更低 |
| 银子峰值 | `MAX(end_money)` | end_money | 相对初始银子的增长倍数 | 体验过「暴富」感可能增强留存 |
| 银子谷值 | `MIN(start_money)` 或 `MIN(end_money)` | start_money / end_money | 相对初始银子的下降比例 | 低于初始值 50% 是预警信号 |
| 银子波动幅度 | `(峰值 - 谷值) / 初始银子 × 100%` | 计算字段 | 低 / 中 / 高波动 | 高波动带来刺激但也增加焦虑 |
| 是否疑似破产 | `MIN(end_money) < MIN(room_currency_lower)` | end_money, room_currency_lower | 是 / 否 | 银子不足以进入最低房间是强烈负面体验 |
| 疑似破产次数 | 对局结束后 `end_money` 低于最低房间门槛的次数 | end_money | 0 / 1 / 2 / 3+ | 多次破产几乎不可能留存 |
| 总服务费支出 | `SUM(room_fee)` | room_fee | 绝对值 | 服务费是隐性消耗，占总支出比例过高影响体验 |
| 银子消耗速度 | `SUM(ABS(diff_money)) / COUNT(*)` | diff_money | 分区间 | 每局平均输赢额反映房间选择是否匹配 |
| 单局最大亏损 | `MIN(diff_money)` | diff_money | 绝对值 | 单局巨亏是强烈负面事件 |
| 单局最大盈利 | `MAX(diff_money)` | diff_money | 绝对值 | 单局大赢是正向激励事件 |
| 逃跑罚没总额 | `SUM(ABS(cut)) WHERE cut < 0` | cut | 绝对值 | 逃跑罚没加速银子耗尽 |
| 保险箱存银 | 末局 `safebox_deposit` | safebox_deposit | 有 / 无 | 使用保险箱说明用户有资产管理意识 |

**分析要点：**

- 绘制新用户的「银子生命线」（每局结束后的银子余额曲线），识别典型的流失模式。
- 破产（银子不足以进入最低房间）后直接不再对局的用户比例是经济系统健康度的核心指标。
- 分析「首日银子净变化」与「次留率」的关系曲线，找到最优的经济体验区间。
- 服务费占比（`SUM(room_fee) / SUM(ABS(diff_money))`）过高可能让玩家感觉「赢不了」。

---

## 六、用户行为指标

> 基于现有对局日志表，以下行为指标可从对局数据中提取。更多维度（如签到、社交、付费）需结合其他业务表补充。

### 6.1 可从对局日志提取的行为指标

| 指标名称 | 定义/计算公式 | 数据来源 | 与留存的关联假设 |
|---------|-------------|---------|----------------|
| 逃跑次数 | `SUM(CASE WHEN cut < 0 THEN 1 ELSE 0 END)` | cut | 逃跑行为反映挫败感，与流失强相关 |
| 逃跑率 | `逃跑次数 / 总对局数 × 100%` | cut | 逃跑率高的用户留存预期极低 |
| 首次逃跑时间点 | 第几局首次出现逃跑 | cut, resultguid 排序 | 越早逃跑说明体验越差 |
| 逃跑后是否继续 | 逃跑后是否还有后续对局 | cut, resultguid 排序 | 逃跑后不再对局是流失确认信号 |
| 末局结果 | 首日最后一局的胜负 | result_id（末局） | 末局输可能是「带着负面情绪离开」 |
| 末局是否逃跑 | 首日最后一局是否逃跑 | cut（末局） | 以逃跑收尾是极端负面体验 |
| 保险箱使用 | `safebox_deposit > 0` 的比例 | safebox_deposit | 主动管理银子的用户更成熟、留存更高 |

### 6.2 需补充数据源的行为指标

以下指标对留存分析有重要价值，但需要对应的业务日志表支持：

| 指标名称 | 所需数据源 | 与留存的关联假设 |
|---------|-----------|----------------|
| 新手引导完成率 | 引导步骤日志表 | 完成引导的用户对游戏理解更充分 |
| 签到行为 | 签到日志表 | 签到说明用户意识到长期收益机制 |
| 社交行为（表情/聊天/好友） | 社交行为日志表 | 社交关系链是最强留存锚点 |
| 首日是否付费 | 充值日志表 | 付费行为是强留存信号（沉没成本效应） |
| 功能探索深度 | 页面访问日志表 | 探索越多说明兴趣越广 |
| 推送通知开启 | 设备权限表 | 开启推送的用户可被召回 |
| 账号绑定 | 账号信息表 | 绑定降低设备流失风险 |

---

## 七、分析方法论

### 7.1 分组对比法

**适用场景：** 快速验证单一指标对留存的影响方向和强度。

**操作方法：**

1. 选定目标指标（如首日平均倍数 `magnification`）。
2. 按分位数或业务逻辑将用户分为 3-5 组。
3. 计算各组的 Day1、Day3、Day7 留存率。
4. 使用卡方检验或 Z 检验验证组间差异的统计显著性。

**示例（倍数维度）：**

| 首日平均公共倍数 | 用户数 | Day1 留存率 | Day7 留存率 |
|----------------|-------|-----------|-----------|
| 3（仅叫地主，无抢无炸） | - | -% | -% |
| 3-6（偶有抢地主） | - | -% | -% |
| 6-24（常有抢地主或炸弹） | - | -% | -% |
| 24+（高倍对局为主） | - | -% | -% |

### 7.2 相关性分析

**适用场景：** 量化各指标与留存的关联强度，筛选关键变量。

**操作方法：**

1. 将留存结果编码为二值变量（次日是否留存：0/1）。
2. 计算各数值指标与留存的 Pearson / Spearman 相关系数。
3. 对相关系数绝对值排序，筛选 |r| > 0.1 的指标。
4. 绘制相关性热力图，观察指标间共线性。

### 7.3 决策树/随机森林

**适用场景：** 自动发现影响留存的关键分裂变量及阈值。

**操作方法：**

1. 以 Day1 留存（0/1）为目标变量，首日所有指标为特征。
2. 训练 CART 决策树（max_depth=4-6）确保可解释性。
3. 输出特征重要性排序，提取关键阈值。
4. 用随机森林做稳健性验证。

### 7.4 用户分群画像（聚类分析）

**适用场景：** 发现新用户的自然分群，理解不同类型用户的行为模式。

**典型分群示例：**

| 用户类型 | 特征描述 | 预期留存 |
|---------|---------|---------|
| 浅尝辄止型 | 仅 1-2 局、低倍、未切换房间 | 低 |
| 稳健成长型 | 5-10 局、中倍、胜率 40%+、无逃跑 | 高 |
| 激进冒险型 | 高倍偏好、频繁加倍/超级加倍、银子波动大 | 受胜负影响大 |
| 挫败逃跑型 | 连败多、逃跑率高、银子大幅亏损 | 极低 |

### 7.5 漏斗分析

**漏斗步骤定义（基于现有数据）：**

```
游戏注册（fact_game_reg）
  → 完成首局对局
    → 完成第 3 局
      → 完成第 5 局
        → 完成第 10 局
          → 次日有对局（Day1 留存）
            → 第 7 日有对局（Day7 留存）
```

### 7.6 生存分析

**适用场景：** 分析不同特征用户的「存活时间」分布。

**操作方法：**

1. 以用户「最后一次对局日期距注册日的天数」为存活时间。
2. 使用 Kaplan-Meier 估计绘制生存曲线。
3. 按关键分群变量（如是否破产、首日胜率区间）分组对比。
4. 使用 Log-rank 检验验证分组间生存差异的显著性。

---

## 八、取数SQL

> 以下 SQL 基于 StarRocks 语法编写。参数化注册日期如 `'20260210'`。
> 分析时间段：**20260210 至 20260408**。
> 过滤条件：仅限 APP 端（`group_id` IN (6, 66, 8, 88, 33, 44, 77, 99)），排除积分/比赛房（`room_id` NOT IN (11534, 14238, 15458)）。

### 8.1 基础 DWS 中间表构建（性能优化）

为保证在 StarRocks 中的查询性能，避免在所有分析 SQL 中反复对千万级原始日志表进行扫描和计算，我们构建三层 DWS 结构。

**数据依赖说明：**

- `olap_tcy_userapp_d_p_login1st`：仅含 `uid, app_id, first_login_ts, dt`，**无** `group_id`/`channel_id`，因此无法直接过滤分端，需借助战绩表中的 `group_id` 实现 APP 用户识别。
- `dwd_game_combat_si`：直接包含 `group_id`、`channel_id` 以及所有倍数字段（`grab_landlord_bet`、`magnification_stacked`、`complete_victory_bet`、`bomb_bet` 均为**独立列**，无需 JSON 解析）。

#### 8.1.1 DWS Step1：新增用户基础信息表

> 完整建表设计见 [`dws/dws_ddz_app_new_user_reg.md`](../dws/dws_ddz_app_new_user_reg.md)

```sql
-- 依赖：tcy_temp.dws_channel_category_map
-- 产出：每个新用户一行，包含 reg_date / group_id / device_type / channel_category
CREATE TABLE tcy_temp.dws_ddz_app_new_user_reg
DISTRIBUTED BY HASH(uid) BUCKETS 16
PROPERTIES("replication_num" = "1")
AS
WITH
new_user_base AS (
    SELECT uid, dt AS reg_date
    FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
    WHERE app_id = 1880053
      AND dt BETWEEN 20260210 AND 20260408
),
first_day_dims AS (
    SELECT c.uid, c.group_id, c.channel_id, COUNT(*) AS game_cnt
    FROM tcy_dwd.dwd_game_combat_si c
    INNER JOIN new_user_base n ON c.uid = n.uid AND c.dt = n.reg_date
    WHERE c.game_id = 53
      AND c.robot != 1
      AND c.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)  -- APP 端
      AND c.room_id NOT IN (11534, 14238, 15458)
    GROUP BY c.uid, c.group_id, c.channel_id
),
dims_dedup AS (
    -- 每用户取对局次数最多的 (group_id, channel_id) 组合
    SELECT uid, group_id, channel_id, game_cnt
    FROM first_day_dims
    QUALIFY ROW_NUMBER() OVER (PARTITION BY uid ORDER BY game_cnt DESC) = 1
),
first_day_total AS (
    SELECT uid, SUM(game_cnt) AS first_day_game_cnt
    FROM first_day_dims
    GROUP BY uid
)
SELECT
    n.uid,
    n.reg_date,
    d.group_id,
    CASE WHEN d.group_id IN (8, 88) THEN 'iOS' ELSE 'Android' END AS device_type,
    d.channel_id,
    COALESCE(chn.channel_category_name, '未知')   AS channel_category,
    COALESCE(chn.channel_category_tag_id, -1)     AS channel_category_tag_id,
    t.first_day_game_cnt
FROM new_user_base n
INNER JOIN dims_dedup d      ON n.uid = d.uid   -- INNER JOIN：天然过滤注册当日无 APP 对局的用户
LEFT JOIN  tcy_temp.dws_channel_category_map chn ON d.channel_id = chn.channel_id
LEFT JOIN  first_day_total t ON n.uid = t.uid;
```

#### 8.1.2 DWS Step2：每日活跃用户表

> 完整建表设计见 [`dws/dws_ddz_app_daily_active.md`](../dws/dws_ddz_app_daily_active.md)

```sql
-- 时间范围覆盖注册期 + Day30 观测期（20260210 ~ 20260508）
-- 产出：uid × dt 去重，用于所有留存 flag 计算
CREATE TABLE tcy_temp.dws_ddz_app_daily_active
DISTRIBUTED BY HASH(uid) BUCKETS 32
PROPERTIES("replication_num" = "1")
AS
SELECT uid, dt
FROM tcy_dwd.dwd_game_combat_si
WHERE dt BETWEEN 20260210 AND 20260508
  AND game_id = 53
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
  AND room_id NOT IN (11534, 14238, 15458)
GROUP BY uid, dt;
```

#### 8.1.3 分析宽表：新增用户首日对局特征宽表

```sql
-- 依赖：8.1.1 dws_ddz_app_new_user_reg、8.1.2 dws_ddz_app_daily_active
-- 产出：每个新用户一行，包含首日全量行为特征 + 留存标记（核心分析数据集）
CREATE TABLE tcy_temp.ddz_user_first_day_features
DISTRIBUTED BY HASH(uid) BUCKETS 16
PROPERTIES("replication_num" = "1")
AS
WITH
-- 1. 从 DWS 基础表获取新用户清单（已完成 APP 过滤、渠道归因、device_type）
new_user_reg AS (
    SELECT uid, reg_date, group_id, device_type,
           channel_id, channel_category, channel_category_tag_id, first_day_game_cnt
    FROM tcy_temp.dws_ddz_app_new_user_reg
),
-- 2. 提取新用户注册当日的原始战绩（字段名与 dwd_game_combat_si 保持一致）
first_day_games_raw AS (
    SELECT
        c.uid,
        c.resultguid,
        c.timecost,
        c.room_id,
        c.role,
        c.chairno,
        c.result_id,
        c.cut,
        c.magnification,
        c.magnification_stacked,
        c.grab_landlord_bet,        -- 独立列，直接使用
        c.complete_victory_bet,     -- 独立列，直接使用
        c.bomb_bet,                 -- 独立列，直接使用
        c.room_base,                -- 正确字段名
        c.room_fee,                 -- 正确字段名
        c.start_money,              -- 正确字段名
        c.end_money,                -- 正确字段名
        c.diff_money,               -- 正确字段名（不含服务费）
        ROW_NUMBER() OVER (PARTITION BY c.uid ORDER BY c.time_unix)      AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY c.uid ORDER BY c.time_unix DESC) AS game_seq_desc
    FROM tcy_dwd.dwd_game_combat_si c
    INNER JOIN new_user_reg r ON c.uid = r.uid AND c.dt = r.reg_date
    WHERE c.game_id = 53
      AND c.robot != 1
      AND c.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND c.room_id NOT IN (11534, 14238, 15458)
),
-- 3. 连胜/连败计算（gaps-and-islands）
max_streaks AS (
    SELECT uid,
           MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
           MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT uid, result_id, COUNT(*) AS streak_len
        FROM (
            SELECT uid, result_id, game_seq,
                   game_seq - ROW_NUMBER() OVER (PARTITION BY uid, result_id ORDER BY game_seq) AS grp
            FROM first_day_games_raw
        ) t GROUP BY uid, result_id, grp
    ) t2 GROUP BY uid
),
-- 4. 留存 flag（只看注册日之后的活跃，a.dt > r.reg_date）
--    Day3 = 注册后第3天，Day7 = 注册后第7天，以此类推
day_flags AS (
    SELECT r.uid,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY),  '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day1,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 3 DAY),  '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day3,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 7 DAY),  '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day7,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 14 DAY), '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day14,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 30 DAY), '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day30
    FROM new_user_reg r
    LEFT JOIN tcy_temp.dws_ddz_app_daily_active a ON r.uid = a.uid AND a.dt > r.reg_date
    GROUP BY r.uid
)
-- 5. 聚合最终宽表
SELECT
    r.uid,
    r.reg_date,
    r.group_id,
    r.device_type,
    r.channel_category,
    r.channel_category_tag_id,
    r.first_day_game_cnt                                             AS game_count,
    SUM(g.timecost)                                                  AS total_play_seconds,
    ROUND(AVG(g.timecost), 1)                                        AS avg_game_seconds,

    -- 首末局特征
    MAX(CASE WHEN g.game_seq = 1 THEN g.timecost END)                AS first_game_duration,
    MAX(CASE WHEN g.game_seq = 1 THEN g.result_id END)               AS first_game_result,
    MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END)             AS initial_money,
    MAX(CASE WHEN g.game_seq = 1 THEN g.magnification END)           AS first_game_magnification,
    MAX(CASE WHEN g.game_seq_desc = 1 THEN g.end_money END)          AS final_money,
    MAX(CASE WHEN g.game_seq_desc = 1 THEN g.result_id END)          AS last_game_result,
    MAX(CASE WHEN g.game_seq_desc = 1 THEN (CASE WHEN g.cut < 0 THEN 1 ELSE 0 END) END) AS last_game_escaped,

    -- 胜负指标
    SUM(CASE WHEN g.result_id = 1 THEN 1 ELSE 0 END)                 AS win_count,
    SUM(CASE WHEN g.result_id = 2 THEN 1 ELSE 0 END)                 AS lose_count,
    ROUND(SUM(CASE WHEN g.result_id = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS win_rate,
    MAX(ms.max_win_streak)                                           AS max_win_streak,
    MAX(ms.max_lose_streak)                                          AS max_lose_streak,

    -- 倍数指标
    ROUND(AVG(g.magnification), 2)                                   AS avg_magnification,
    MAX(g.magnification)                                             AS max_magnification,
    ROUND(AVG(g.magnification * 1.0 / NULLIF(g.magnification_stacked, 0)), 2) AS avg_public_multi,
    SUM(CASE WHEN g.magnification <= 6  THEN 1 ELSE 0 END)           AS low_multi_games,
    SUM(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 ELSE 0 END) AS mid_multi_games,
    SUM(CASE WHEN g.magnification > 24  THEN 1 ELSE 0 END) AS high_multi_games,
    ROUND(AVG(g.grab_landlord_bet), 2) AS avg_grab_bet,
    SUM(CASE WHEN g.grab_landlord_bet > 3 THEN 1 ELSE 0 END) AS games_with_grab,
    SUM(CASE WHEN g.magnification_stacked > 1 THEN 1 ELSE 0 END) AS games_player_doubled,
    SUM(CASE WHEN g.magnification_stacked = 4 THEN 1 ELSE 0 END) AS games_player_super_doubled,
    SUM(CASE WHEN g.complete_victory_bet = 2 THEN 1 ELSE 0 END) AS games_with_spring,
    SUM(CAST(g.bomb_bet / 2 AS INT)) AS total_bomb_count,
    SUM(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 ELSE 0 END) AS high_multi_wins,
    SUM(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 ELSE 0 END) AS high_multi_losses,
    ROUND(AVG(ABS(g.diff_money) * 1.0 / NULLIF(g.room_base, 0)), 2) AS avg_realized_multi,

    -- 经济系统指标
    MAX(g.end_money) AS money_peak,
    MIN(g.end_money) AS money_valley,
    MAX(g.end_money) - MIN(g.end_money) AS money_swing,
    SUM(g.diff_money) AS total_diff_money,
    SUM(CASE WHEN g.diff_money < 0 THEN ABS(g.diff_money) ELSE 0 END) AS total_money_lost,
    SUM(CASE WHEN g.diff_money > 0 THEN g.diff_money ELSE 0 END) AS total_money_won,
    MIN(g.diff_money) AS worst_single_game,
    MAX(g.diff_money) AS best_single_game,
    SUM(g.room_fee) AS total_fee_paid,
    ROUND(SUM(ABS(g.diff_money)) * 1.0 / COUNT(*), 0) AS avg_money_swing_per_game,

    -- 逃跑与房间
    SUM(CASE WHEN g.cut < 0 THEN 1 ELSE 0 END) AS escape_count,
    COUNT(DISTINCT g.room_id) AS distinct_rooms,

    -- 留存标记
    MAX(COALESCE(df.is_retained_day1, 0)) AS is_retained_day1,
    MAX(COALESCE(df.is_retained_day3, 0)) AS is_retained_day3,
    MAX(COALESCE(df.is_retained_day7, 0)) AS is_retained_day7,
    MAX(COALESCE(df.is_retained_day14, 0)) AS is_retained_day14,
    MAX(COALESCE(df.is_retained_day30, 0)) AS is_retained_day30
FROM first_day_games_raw g
INNER JOIN new_user_reg r ON g.uid = r.uid
LEFT JOIN  max_streaks ms ON g.uid = ms.uid
LEFT JOIN  day_flags df   ON g.uid = df.uid
GROUP BY r.uid, r.reg_date, r.group_id, r.device_type, r.channel_category, r.channel_category_tag_id, r.first_day_game_cnt;
```

### 8.2 多维留存分析模型 SQL

> 所有分析直接基于极简的 `tcy_temp.ddz_user_first_day_features` 宽表执行。极大地提升了 StarRocks 的运行效率。

#### 8.2.1 新增用户多日留存趋势（按渠道类别）
```sql
SELECT
    reg_date,
    channel_category,
    COUNT(DISTINCT uid) AS new_active_users,
    ROUND(SUM(is_retained_day1) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day3) * 100.0 / COUNT(*), 2) AS day3_rate,
    ROUND(SUM(is_retained_day7) * 100.0 / COUNT(*), 2) AS day7_rate,
    ROUND(SUM(is_retained_day14) * 100.0 / COUNT(*), 2) AS day14_rate,
    ROUND(SUM(is_retained_day30) * 100.0 / COUNT(*), 2) AS day30_rate
FROM tcy_temp.ddz_user_first_day_features
GROUP BY reg_date, channel_category
ORDER BY reg_date, channel_category;
```

#### 8.2.2 倍数维度留存（加入渠道对照）
```sql
SELECT
    channel_category,
    CASE
        WHEN avg_magnification <= 3  THEN 'A: =3 (不加倍)'
        WHEN avg_magnification <= 6  THEN 'B: 3-6 (低频倍数)'
        WHEN avg_magnification <= 12 THEN 'C: 6-12 (中频叠加)'
        WHEN avg_magnification <= 24 THEN 'D: 12-24 (高倍组合)'
        ELSE                              'E: 24+ (极高倍)'
    END AS multi_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_user_first_day_features
GROUP BY channel_category,
    CASE
        WHEN avg_magnification <= 3  THEN 'A: =3 (不加倍)'
        WHEN avg_magnification <= 6  THEN 'B: 3-6 (低频倍数)'
        WHEN avg_magnification <= 12 THEN 'C: 6-12 (中频叠加)'
        WHEN avg_magnification <= 24 THEN 'D: 12-24 (高倍组合)'
        ELSE                              'E: 24+ (极高倍)'
    END
ORDER BY channel_category, multi_group;
```

#### 8.2.3 经济维度（按绝对差额计算，体现渠道货币差异）
```sql
SELECT
    channel_category,
    CASE
        WHEN final_money - initial_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN final_money - initial_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN final_money - initial_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN final_money - initial_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN final_money - initial_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                           'F: 巨赚 (>5万)'
    END AS net_money_change_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_user_first_day_features
GROUP BY channel_category,
    CASE
        WHEN final_money - initial_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN final_money - initial_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN final_money - initial_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN final_money - initial_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN final_money - initial_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                           'F: 巨赚 (>5万)'
    END
ORDER BY channel_category, net_money_change_group;
```

#### 8.2.4 高倍局经历与留存
```sql
SELECT
    channel_category,
    CASE
        WHEN high_multi_games = 0 THEN 'A: 未经历高倍局'
        WHEN high_multi_wins > 0 AND high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN high_multi_wins = 0 AND high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END AS high_multi_exp,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_user_first_day_features
GROUP BY channel_category,
    CASE
        WHEN high_multi_games = 0 THEN 'A: 未经历高倍局'
        WHEN high_multi_wins > 0 AND high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN high_multi_wins = 0 AND high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END
ORDER BY channel_category, high_multi_exp;
```

#### 8.2.5 设备类型（iOS vs Android）留存对比
```sql
-- 分析目的：APP 端 iOS/Android 用户在首日行为和留存上的差异
-- 可结合 channel_category 交叉分析，区分"渠道 Android"与"官方 iOS"等细分群体
SELECT
    device_type,
    channel_category,
    COUNT(*)                                                         AS user_count,
    ROUND(AVG(game_count), 1)                                        AS avg_game_count,
    ROUND(AVG(win_rate), 2)                                          AS avg_win_rate,
    ROUND(AVG(avg_magnification), 2)                                 AS avg_magnification,
    ROUND(SUM(is_retained_day1) * 100.0 / COUNT(*), 2)               AS day1_rate,
    ROUND(SUM(is_retained_day3) * 100.0 / COUNT(*), 2)               AS day3_rate,
    ROUND(SUM(is_retained_day7) * 100.0 / COUNT(*), 2)               AS day7_rate,
    ROUND(SUM(is_retained_day14) * 100.0 / COUNT(*), 2)              AS day14_rate,
    ROUND(SUM(is_retained_day30) * 100.0 / COUNT(*), 2)              AS day30_rate
FROM tcy_temp.ddz_user_first_day_features
GROUP BY device_type, channel_category
ORDER BY device_type, channel_category;
```

---

## 九、结论模板与行动建议框架


### 9.1 分析结论输出模板

每一项分析发现按以下结构化模板输出，确保从数据到行动的完整闭环：

```
┌──────────────────────────────────────────────────────────┐
│ 分析发现卡片                                              │
├──────────────────────────────────────────────────────────┤
│ 【指标】首日最大连败长度                                    │
│ 【发现】连败 ≥ 3 局的新用户次留率为 22%，                   │
│        显著低于连败 < 3 局用户的 41%（p < 0.01）            │
│ 【假设】连续失败破坏了新手的游戏信心，                       │
│        导致负面情绪累积引发流失                              │
│ 【验证】A/B 测试：连败 3 局后触发匹配保护                   │
│       （匹配更弱的 AI/新手对手），观察留存变化               │
│ 【建议】上线新手连败保护机制，                              │
│        连败 3 局后降低匹配难度或赠送安慰银子                 │
└──────────────────────────────────────────────────────────┘
```

### 9.2 常见优化方向

#### 方向一：新手保护机制

| 策略 | 具体措施 | 预期效果 | 监控指标 |
|-----|---------|---------|---------|
| 新手配牌优化 | 前 N 局配牌保障胜率（当前仅首局） | 延长正向体验窗口 | 前N局胜率、Day1 留存 |
| 倍数限制 | 前 5 局限制理论倍数上限（如封顶 magnification ≤ 12） | 减少银子剧烈波动 | 首日银子净变化、破产率 |
| 连败干预 | 连败 3 局后匹配更弱对手或切换新手配牌 | 打断连败降低流失 | 连败长度分布、流失节点 |
| 抢地主/加倍引导 | 新手前 N 局提示「不建议抢地主/加倍」 | 减少新手主动拉高倍数导致的大额亏损 | 新手加倍率、超级加倍率 |

#### 方向二：银子经济调控

| 策略 | 具体措施 | 预期效果 | 监控指标 |
|-----|---------|---------|---------|
| 破产兜底 | 提升破产救济银子数额 | 减少因无银子退出的用户 | 破产后退出率 |
| 新手礼包加厚 | 增加初始银子数量 | 延长新手游戏生命周期 | 首日对局数、破产时间 |
| 房间引导 | 根据银子余额推荐合适的底分房间，避免进入高消耗房间 | 合理匹配银子与房间 | 房间选择分布 |
| 服务费减免 | 新手前 N 局免收或减半服务费 | 降低隐性消耗 | 服务费占支出比 |

#### 方向三：首局体验优化

| 策略 | 具体措施 | 预期效果 | 监控指标 |
|-----|---------|---------|---------|
| 首局引导强化 | 首局增加出牌提示和规则说明 | 降低新手门槛 | 首局完成率、首局胜率 |
| 首局逃跑预防 | 首局中检测到长时间无操作时弹出提示 | 减少首局流失 | 首局逃跑率 |
| 首局倍数控制 | 首局不触发抢地主/加倍/超级加倍 | 保护首局体验 | 首局银子变化 |

#### 方向四：逃跑行为干预

| 策略 | 具体措施 | 预期效果 | 监控指标 |
|-----|---------|---------|---------|
| 逃跑罚没优化 | 新手前 N 局降低逃跑罚没比例 | 减少因罚没加速银子耗尽 | 逃跑率、逃跑后继续率 |
| 逃跑前挽留 | 检测到退出意图时弹窗提示当前局势 | 减少冲动逃跑 | 逃跑次数 |

### 9.3 优先级排序建议

根据「预期影响力 × 实施难度」评估：

1. **P0（立即验证）**：新手连败保护、破产兜底优化、首局配牌保障
2. **P1（短期迭代）**：新手倍数上限限制、加倍引导、房间智能推荐
3. **P2（中期规划）**：逃跑行为优化、服务费减免、社交激活策略
4. **P3（长期建设）**：匹配算法升级、AI 难度自适应、经济系统全局调优

---

## 十、指标速查表

### 留存基础指标

| 编号 | 指标名称 | 计算公式 / 数据来源 | 分析维度 |
|-----|---------|-------------------|---------|
| R01 | 游戏次日留存率 | Day1 有对局的新用户 / 当日新增且有对局的用户 | 渠道/设备/时段 |
| R02 | 游戏3日留存率 | Day3 有对局的新用户 / 当日新增且有对局的用户 | 同上 |
| R03 | 游戏7日留存率 | Day7 有对局的新用户 / 当日新增且有对局的用户 | 同上 |
| R04 | 游戏14日留存率 | Day14 有对局的新用户 / 当日新增且有对局的用户 | 同上 |
| R05 | 游戏30日留存率 | Day30 有对局的新用户 / 当日新增且有对局的用户 | 同上 |

### 倍数指标

| 编号 | 指标名称 | 计算公式 / 数据来源 | 分析维度 |
|-----|---------|-------------------|---------|
| G01 | 首日平均理论倍数 | `AVG(magnification)`（含个人加倍） | ≤3 / 3-6 / 6-12 / 12-24 / 24+ |
| G02 | 首日最大理论倍数 | `MAX(magnification)` | ≤6 / 6-24 / 24-96 / 96+ |
| G03 | 首日平均公共倍数 | `AVG(magnification/magnification_stacked)` | 分区间 |
| G04 | 首日平均实际倍数 | `AVG(ABS(diff_money)/room_base)` | 分区间 |
| G05 | 理论与实际倍数差异率 | `1 - 实际倍数/理论倍数`（携银不足指标） | 百分比 |
| G06 | 低倍局占比 | `magnification<=6` 的占比 | 百分比 |
| G07 | 高倍局占比 | `magnification>24` 的占比 | 百分比 |
| G08 | 高倍局胜负留存 | 高倍局胜率 × 对应留存 | 赢高倍 vs 输高倍 |
| G09 | 抢地主发生率 | `grab_landlord_bet>3` 占比 | 百分比 |
| G10 | 玩家加倍率 | `magnification_stacked>1` 占比 | 百分比 |
| G11 | 超级加倍率 | `magnification_stacked=4` 占比 | 百分比 |
| G12 | 春天/反春率 | `complete_victory_bet=2` 占比 | 百分比 |
| G13 | 平均炸弹数 | `AVG(bomb_bet/2)` | 均值 |

### 胜负指标

| 编号 | 指标名称 | 计算公式 / 数据来源 | 分析维度 |
|-----|---------|-------------------|---------|
| G14 | 首日胜率 | `SUM(result_id=1)/COUNT(*)` | <30% / 30-50% / 50-70% / 70%+ |
| G15 | 首N局胜率 | 前3/5/10局胜率 | 与首日胜率交叉 |
| G16 | 首局胜负 | 首局 result_id | 胜 vs 负 |
| G17 | 连胜最大长度 | gaps-and-islands 算法 | 0 / 1-2 / 3-5 / 5+ |
| G18 | 连败最大长度 | 同上 | 0-1 / 2-3 / 4-5 / 5+ |
| G19 | 当地主胜率 | `role=1` 时胜率 | 对比农民胜率 |
| G20 | 当农民胜率 | `role=2` 时胜率 | 对比地主胜率 |
| G21 | 角色分布 | 地主次数 / 农民次数 | 预期 1:2 |

### 场次与时长指标

| 编号 | 指标名称 | 计算公式 / 数据来源 | 分析维度 |
|-----|---------|-------------------|---------|
| G22 | 首日对局数 | `COUNT(*)` | 1 / 2-5 / 6-10 / 11-20 / 20+ |
| G23 | 首日总对局时长 | `SUM(timecost)` | <5min / 5-15min / 15-30min / 30min+ |
| G24 | 平均单局时长 | `AVG(timecost)` | <120s / 120-300s / 300-480s / 480s+ |
| G25 | 底分房间分布 | `room_base` 分布 | 低分 / 中分 / 高分 |
| G26 | 房间切换数 | `COUNT(DISTINCT room)` | 1 / 2-3 / 4+ |

### 经济系统指标

| 编号 | 指标名称 | 计算公式 / 数据来源 | 分析维度 |
|-----|---------|-------------------|---------|
| E01 | 初始银子 | 首局 `start_money` | 绝对值 |
| E02 | 银子净变化 | 末局 `end_money` - 首局 `start_money` | 亏损/持平/盈利 |
| E03 | 银子峰值 | `MAX(end_money)` | 相对初始倍数 |
| E04 | 银子谷值 | `MIN(end_money)` | 相对初始比例 |
| E05 | 银子波动幅度 | `(峰值-谷值)/初始` | 低/中/高 |
| E06 | 是否疑似破产 | `MIN(end_money) < MIN(room_currency_lower)` | 是/否 |
| E07 | 单局最大亏损 | `MIN(diff_money)` | 绝对值 |
| E08 | 单局最大盈利 | `MAX(diff_money)` | 绝对值 |
| E09 | 总服务费 | `SUM(room_fee)` | 绝对值 |
| E10 | 银子消耗速度 | `SUM(ABS(diff_money))/COUNT(*)` | 分区间 |

### 行为指标

| 编号 | 指标名称 | 计算公式 / 数据来源 | 分析维度 |
|-----|---------|-------------------|---------|
| B01 | 逃跑次数 | `SUM(cut<0)` | 0 / 1 / 2 / 3+ |
| B02 | 逃跑率 | 逃跑次数 / 总对局数 | 百分比 |
| B03 | 末局结果 | 末局 result_id | 胜/负/逃跑 |
| B04 | 保险箱使用 | `safebox_deposit > 0` | 有/无 |

---

> **文档版本**：v3.0
> **创建日期**：2026-03-23
> **更新说明**：
> - v2.0：整合同城游平台业务背景、斗地主具体玩法规则、数据表结构及 15 条取数 SQL
> - v2.1：修正 magnification 字段含义；留存口径统一为游戏留存
> - v3.0：重构 DWS 层架构（三层结构）；修正字段名（`room_base`/`start_money`/`diff_money` 等）；倍数相关字段（`grab_landlord_bet`/`complete_victory_bet`/`bomb_bet`）直接读列而非 JSON 解析；新增 `device_type`（iOS/Android）维度；留存 flag 区间修正（Day3=+3天，含 `a.dt > r.reg_date` 限制）；`dws_ddz_app_daily_active` 时间上限延至 20260508 覆盖 Day30 观测期
>
> **适用范围**：同城游·斗地主 App 新增用户留存专项分析
> **使用建议**：
> 1. 按顺序执行 8.1.1 → 8.1.2 → 8.1.3 构建三层 DWS 表（详见 `dws/` 目录）
> 2. 依次运行 8.2.1 ~ 8.2.5 进行各维度分组留存分析
> 3. 将分析结果按「发现卡片」模板输出，推动产品优化