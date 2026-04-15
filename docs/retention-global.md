# 斗地主 App 新增用户留存分析框架

> 本文档为同城游平台·斗地主游戏的新增用户留存分析完整框架，涵盖业务背景、数据基础、指标定义、分析方法论、取数SQL及行动建议，旨在系统化地识别影响新用户留存的关键因子并指导产品优化。

---

## 三层分析体系总览

斗地主新增用户留存分析按粒度分为三层，各层文档独立聚焦、互不重叠：

| 层级 | 文档 | 分析粒度 | 核心问题 |
|------|------|---------|---------|
| **全局层**（本文档） | `retention-global.md` | 全体新增用户 | 整体留存水平、各因子对留存的影响 |
| **分玩法层** | [`retention-by-mode.md`](retention-by-mode.md) | 经典 / 不洗牌 / 癞子 | 各玩法的留存差异、玩法内因子分析、玩法切换行为 |
| **分客户端语言层** | [`retention-by-client-lang.md`](retention-by-client-lang.md) | Cocos Creator / Cocos Lua | 不同客户端版本的留存差异、技术差异对体验的影响 |

**共享关系**：本文档的 **一~七章**（业务背景、数据基础、指标体系、分析方法论）为三层共享的基础设定，分玩法层和分客户端语言层仅包含各自的增量分析内容，不重复基础章节。

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
| ---- | ---- |
| 经典玩法 | 标准斗地主规则，新手默认进入 |
| 不洗牌玩法 | 保留上局出牌顺序发牌，牌序有延续性 |
| 赖子玩法 | 存在万能牌（赖子），增加随机性和策略性 |

**对局流程与倍数机制：**

```
叫地主（固定3分）→ 抢地主 → 加倍/超级加倍 → 出牌对局 → 结算
```

| 倍数因子 | 作用范围 | 取值规则 |
| ------- | ------- | ------- |
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

本分析基于以下 DWS 层中间表：

#### 表1：`dws_dq_app_daily_reg` — APP 端注册用户宽表

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| uid | BIGINT | 玩家唯一标识 |
| reg_date | INT | 注册日期（YYYYMMDD） |
| reg_datetime | DATETIME | 注册时间 |
| reg_group_id | INT | 首次登录分端 ID |
| reg_channel_id | BIGINT | 首次登录渠道号 |
| reg_app_code | string | 首次登录应用code |
| channel_category_id | INT | 渠道分类 ID |
| channel_category_name | STRING | 渠道分类名称 |
| channel_category_tag_id | INT | 渠道分类标签：1=官方，2=渠道，3=小游戏 |
| is_login_log_missing | INT | 是否登录日志缺失：1=缺失，0=正常 |
| first_day_login_cnt | BIGINT | 首日登录次数 |

#### 表2：`dws_dq_daily_login` — 每日登录聚合表

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| uid | BIGINT | 玩家唯一标识 |
| app_id | BIGINT | 应用 ID |
| login_date | DATE | 登录日期 |
| first_login_time | DATETIME | 当日首次登录时间 |
| first_app_code | string | 当日首次登录应用code |
| first_channel_id | BIGINT | 当日首次登录渠道号 |
| first_group_id | INT | 当日首次登录分端 ID |
| login_count | BIGINT | 当日总登录次数 |

#### 表3：`dws_ddz_daily_game` — 对局战绩统一字段表

| 字段                    | 类型     | 说明                                                                       |
| --------------------- | ------ | ------------------------------------------------------------------------ |
| dt                    | BIGINT | 对局日期（YYYYMMDD）                                                           |
| time_unix             | BIGINT | 对局时间戳（毫秒级）                                                              |
| resultguid            | STRING | 本局战绩 ID                                                                  |
| timecost              | INT    | 对局耗时（秒）                                                                  |
| room_id               | INT    | 房间号                                                                      |
| play_mode             | INT    | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房                                      |
| room_base             | INT    | 房间底分（统一字段）                                                               |
| room_fee              | INT    | 房间服务费（统一字段）                                                              |
| uid                   | BIGINT | 玩家 ID                                                                    |
| robot                 | INT    | 1=机器人，其他=真人                                                              |
| role                  | INT    | 1=地主，2=农民                                                                |
| result_id             | INT    | 1=获胜，2=失败                                                                |
| start_money           | BIGINT | 对局前货币数量（统一字段）                                                            |
| end_money             | BIGINT | 对局后货币数量（统一字段）                                                            |
| diff_money_pre_tax    | BIGINT | 还原服务费前的对局输赢（统一字段）                                                        |
| cut                   | BIGINT | 逃跑罚没货币（<0 代表存在逃跑行为）                                                      |
| magnification         | INT    | 个人理论总倍数                                                                  |
| magnification_stacked | INT    | 个人加倍：1=不加倍 / 2=加倍 / 4=超级加倍                                               |
| real_magnification    | DOUBLE | 本局实际输赢倍数（可能为负数，求平均需用 ABS）                                                |
| grab_landlord_bet     | INT    | 抢地主倍数：3=无人抢 / 6=1人抢 / 12=2人抢                                             |
| complete_victory_bet  | INT    | 春天/反春标记：2=存在春天或反春                                                        |
| bomb_bet              | INT    | 炸弹倍数，`bomb_bet/2` = 炸弹个数                                                 |
| app_code              | string | 应用code |

### 2.2 数据使用注意事项

1. **过滤机器人**：所有分析需过滤 `robot = 1` 的记录，仅保留真人玩家数据。
2. **逃跑局处理**：`cut < 0` 的对局为逃跑局，对局结果可能不完整，部分指标（如倍数、胜负）需谨慎使用。
3. **新手配牌**：首局使用特殊配牌，分析胜率时需单独标注首局数据。
4. **留存判定**：使用 `dws_dq_daily_login` 表判断用户是否有登录，分母为当日注册用户数，分子为第 N 日有登录的用户数。
5. **实际倍数**：`real_magnification` 字段已预计算，但可能为负数（输局），求平均时需使用 `ABS(real_magnification)`。
6. **货币统一**：`dws_ddz_daily_game` 已将不同玩法的货币字段统一为 `start_money`、`end_money`、`diff_money_pre_tax`。

---

## 三、留存基础指标体系

### 3.1 留存率定义

| 指标名称 | 定义 | 计算公式 |
| -------------- | ------------------------ | ------------------------------------ |
| 次日留存（注册后第 1 天） | 注册用户中，注册后第 1 天仍有登录的比例 | `注册后第 1 天有登录的注册用户数 / 当日注册用户数 × 100%` |
| 3 日留存（注册后第 2 天） | 注册用户中，注册后第 2 天仍有登录的比例 | `注册后第 2 天有登录的注册用户数 / 当日注册用户数 × 100%` |
| 7 日留存（注册后第 6 天） | 注册用户中，注册后第 6 天仍有登录的比例 | `注册后第 6 天有登录的注册用户数 / 当日注册用户数 × 100%` |
| 14 日留存（注册后第 13 天） | 注册用户中，注册后第 13 天仍有登录的比例 | `注册后第 13 天有登录的注册用户数 / 当日注册用户数 × 100%` |
| 30 日留存（注册后第 29 天） | 注册用户中，注册后第 29 天仍有登录的比例 | `注册后第 29 天有登录的注册用户数 / 当日注册用户数 × 100%` |

**口径说明：**

- **时间基准**：
  - Day 0 = 注册当天（第0天）
  - 次留 = 次日留存 = 注册后第1天
  - 3 留 = 3日留存 = 注册后第2天
  - 7 留 = 7日留存 = 注册后第6天
  - 14 留 = 14日留存 = 注册后第13天
  - 30 留 = 30日留存 = 注册后第29天
- **新增用户**：以 `dws_dq_app_daily_reg` 表中的 uid 为准，即首次进入斗地主 App 并在服务端生成注册记录的用户（仅 APP 端）。
- **分母**：当日注册的 APP 端用户数，不要求注册当日有对局行为。
- **分子**：第 N 日在 `dws_dq_daily_login` 中存在登录记录的用户数。
- **自然日**：以 dt 分区字段为准，北京时间自然日。

> **注意**：本文档分析的是「新增用户留存」，而非「游戏留存」。游戏留存的分母是"注册当日有对局的用户"，新增用户留存的分母是"注册用户"（不要求有对局）。

### 3.2 留存分层维度

| 分层维度 | 拆分方式 | 分析目的 |
| ------- | ------- | ------- |
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
| ------- | ----------- | ------- |-------- | -------------- |
| 首日平均理论倍数 | `AVG(magnification)`，magnification 已含个人加倍 | magnification | 分区间（3 / 6 / 12 / 24 / 48+） | 适中的倍数体验可能对应最优留存 |
| 首日最大理论倍数 | `MAX(magnification)` | magnification | 分区间（≤6 / 6-12 / 12-24 / 24-48 / 48-96 / 96+） | 经历超高倍局可能产生两极分化 |
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
- 将新用户按「首日平均理论倍数」分为低倍组（magnification 均值 ≤6）/ 中倍组（6-24）/ 高倍组（>24），对比各组 次留 ~ 7 留曲线。
- 重点分析「首日经历过高倍局（magnification>24）且输掉」的用户群体，预期其流失概率最高。
- 关注「理论与实际倍数差异率」，差异率高说明携银不足的情况频繁，玩家在承受超出自身经济能力的对局。
- 区分主动倍数行为（玩家加倍 magnification_stacked）与被动倍数（炸弹 bomb_bet、春天 complete_victory_bet），分析新手对主动加倍的使用是否过于激进。
- 公共倍数的最小值为 3（叫地主固定 3 分，无人抢无炸无春天且不加倍时 magnification = 3）。

### 4.2 胜负相关指标

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
| ------- | ----------- | ------- | ------- | -------------- |
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
| ------- | ----------- | ------- | ------- | -------------- |
| 首日对局数 | 注册当日完成的总对局数 | COUNT(*) | 分区间（1 / 2-5 / 6-10 / 11-20 / 20+） | 存在最优区间，过少未形成习惯，过多可能疲劳 |
| 首日总对局时长 | `SUM(timecost)` 秒 | timecost | 分区间（<5min / 5-15min / 15-30min / 30min+） | 总投入时间是沉浸度的直接体现 |
| 平均单局时长 | `AVG(timecost)` 秒 | timecost | 分区间（<120s / 120-300s / 300-480s / 480s+） | 过短可能是秒退/逃跑，过长可能是卡顿 |
| 首局耗时 | 第一局的 timecost | timecost（首局） | 分区间 | 首局耗时反映新手配牌局的体验节奏 |

**分析要点：**

- 找到「最优首日对局数区间」——对局数在该区间内的用户留存率最高。
- 将平均单局时长异常短（<60s）的对局与逃跑行为（cut<0）交叉分析。

### 4.4 场次类型指标

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
| ------- | ----------- | ------- | ------- | -------------- |
| 底分房间分布 | 按 `room_base` 分组的局数分布 | room_base | 低分场 / 中分场 / 高分场 | 新手进入高分场可能被快速淘汰 |
| 房间切换次数 | 首日在不同 room 间切换的次数 | COUNT(DISTINCT room) | 分组（1 / 2-3 / 4+） | 频繁切换可能是体验不佳的信号 |
| 最高底分房间 | 首日进入过的最高 `room_base` | MAX(room_base) | 分组 | 选择过高底分是冲动行为信号 |
| 房间入门银子利用率 | `start_money / room_currency_lower` | start_money, room_currency_lower | 分区间 | 携银刚好达到门槛的用户风险更高 |

**分析要点：**

- 分析新手选择底分房间是否合理——携银 / 房间最低携银的比值过小（如 < 3 倍）意味着高风险。

---

## 五、经济系统指标

> 银子是斗地主的核心经济资源。新手礼包提供初始银子，后续通过对局赢取或亏损，银子的变动直接影响玩家情绪和持续游玩能力。

| 指标名称 | 定义/计算公式 | 数据来源 | 分析维度 | 与留存的关联假设 |
| ------- | ----------- | ------- | ------- | -------------- |
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
| ------- | ----------- | ------- | -------------- |
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
| ------- | --------- | -------------- |
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
3. 计算各组的 次留、3 留、7 留率。
4. 使用卡方检验或 Z 检验验证组间差异的统计显著性。

**示例（倍数维度）：**

| 首日平均公共倍数 | 用户数 | 次留率 | 7 留率 |
| -------------- | ----- | --------- | --------- |
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

1. 以 次留（0/1）为目标变量，首日所有指标为特征。
2. 训练 CART 决策树（max_depth=4-6）确保可解释性。
3. 输出特征重要性排序，提取关键阈值。
4. 用随机森林做稳健性验证。

### 7.4 用户分群画像（聚类分析）

**适用场景：** 发现新用户的自然分群，理解不同类型用户的行为模式。

**典型分群示例：**

| 用户类型 | 特征描述 | 预期留存 |
| ------- | ------- | ------- |
| 浅尝辄止型 | 仅 1-2 局、低倍、未切换房间 | 低 |
| 稳健成长型 | 5-10 局、中倍、胜率 40%+、无逃跑 | 高 |
| 激进冒险型 | 高倍偏好、频繁加倍/超级加倍、银子波动大 | 受胜负影响大 |
| 挫败逃跑型 | 连败多、逃跑率高、银子大幅亏损 | 极低 |

### 7.5 漏斗分析

**漏斗步骤定义（基于现有数据）：**

```
APP 端注册（dws_dq_app_daily_reg）
  → 完成首局对局
    → 完成第 3 局
      → 完成第 5 局
        → 完成第 10 局
          → 次日有登录（次留）
            → 第 6 日有登录（7 留）
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
> 分析时间段：**20260210 至 20260414**。
> 过滤条件：仅限 APP 端银子玩法（`play_mode IN (1, 2, 3, 5)`），其中 5 为 APP 端比赛玩法。

### 8.1 基础 DWS 中间表构建

本分析依赖以下 DWS 中间表，详细设计见各表文档：

| 表名 | 说明 | 文档 |
| --- | --- | --- |
| `dws_dq_app_daily_reg` | APP 端注册用户宽表 | [dws_dq_app_daily_reg.md](../dws/dws_dq_app_daily_reg.md) |
| `dws_dq_daily_login` | 每日登录聚合表 | [dws_dq_daily_login.md](../dws/dws_dq_daily_login.md) |
| `dws_ddz_daily_game` | 对局战绩统一字段表 | [dws_ddz_daily_game.md](../dws/dws_ddz_daily_game.md) |
| `dws_ddz_appdaily_game_stat` | 用户每日游戏行为聚合表 | [dws_ddz_appdaily_game_stat.md](../dws/dws_ddz_appdaily_game_stat.md) |
| `dws_ddz_appdaily_game_stat_by_mode` | 用户每日游戏行为聚合表（按玩法拆分） | [dws_ddz_appdaily_game_stat_by_mode.md](../dws/dws_ddz_appdaily_game_stat_by_mode.md) |

### 8.2 新增用户留存分析 SQL

#### 8.2.1 按日期统计新增用户留存率

```sql
-- 新增用户留存率（按日期）
SELECT
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    -- 次留（注册后第1天）
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) AS day1_retained,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    -- 3 留（注册后第2天，即传统3日留存）
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 2 DAY) THEN r.uid END) AS day2_retained,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 2 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day2_rate,
    -- 7 留（注册后第6天，即传统7日留存）
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) AS day6_retained,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date
ORDER BY r.reg_date;
```

**查询结果（2026-02-10 至 2026-04-14）：**

数据文件：`[8.2.1 留存率数据](../../data/md/8.2.1.md)`

#### 8.2.2 按渠道分类统计留存率

```sql
-- 按渠道分类统计新增用户留存
SELECT    
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end as channel_category_name,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
  AND r.is_login_log_missing = 0
GROUP BY case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end, r.reg_date
ORDER BY case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end, r.reg_date desc
```

**查询结果（2026-02-10 至 2026-04-14）：**

数据文件：`[8.2.2 渠道留存率数据](../../data/md/8.2.2.md)`

#### 8.2.3 按 APP 端类型统计留存率

```sql
-- 按 Android/iOS 统计新增用户留存
SELECT
    CASE 
        WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
        WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END AS platform,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
  AND r.is_login_log_missing = 0
GROUP BY 
    CASE 
        WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
        WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END,
    r.reg_date
ORDER BY platform, r.reg_date desc;
```

**查询结果（2026-02-10 至 2026-04-14）：**

数据文件：`[8.2.3 平台留存率数据](../../data/md/8.2.3.md)`

### 8.3 首日游戏行为分析 SQL

> 以下 SQL 使用 `dws_ddz_appdaily_game_stat` 中间表，大幅提升查询效率。
> 分析对象：注册当日有对局的用户（用于理解游戏行为与留存的关系）。

#### 8.3.1 按首日对局数分析留存

```sql
-- 按首日对局数分组分析留存（0局和1局拆分）
SELECT
    CASE 
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'A: 0局'
        WHEN g.game_count = 1 THEN 'B: 1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
        ELSE 'E: 10局以上'
    END AS game_count_group,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY 
    CASE 
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'A: 0局'
        WHEN g.game_count = 1 THEN 'B: 1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
        ELSE 'E: 10局以上'
    END,
  r.reg_date
ORDER BY game_count_group, r.reg_date desc;
```

> **说明**：0 局用户（注册但未进入对局）与 1 局用户（仅体验首局）的流失原因不同——前者可能是引导/UI 层面流失，后者是首局体验驱动流失，拆分后可分别制定干预策略。

**查询结果：**

数据文件：`[8.3.1 首日对局数留存数据](../../data/md/8.3.1.md)`

#### 8.3.2 按首日胜率分析留存

```sql
-- 按首日胜率分组分析留存
SELECT
    r.reg_date,
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end as channel_category_name,
    CASE 
        WHEN g.win_rate < 30 OR g.win_rate IS NULL THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date between 20260210 and 20260414
  AND g.game_count > 0  
GROUP BY r.reg_date,     
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end,
    CASE 
        WHEN g.win_rate < 30 OR g.win_rate IS NULL THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END
ORDER BY r.reg_date, case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end, win_rate_group;
```

**查询结果：**

数据文件：`[8.3.2 首日胜率留存数据](../../data/md/8.3.2.md)`

#### 8.3.3 按首日倍数分组分析留存

```sql
-- 按首日平均倍数分组分析留存
SELECT
    r.reg_date,
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end as channel_category_name,
    CASE
        WHEN g.avg_magnification <= 6 OR g.avg_magnification IS NULL THEN 'A: <=6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END AS multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date between 20260210 and 20260414
GROUP BY r.reg_date, 
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end,
    CASE
        WHEN g.avg_magnification <= 6 OR g.avg_magnification IS NULL THEN 'A: <=6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END
ORDER BY r.reg_date, case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end, multi_group;
```

**查询结果：**

数据文件：`[8.3.3 首日倍数留存数据](../../data/md/8.3.3.md)`

#### 8.3.4 按高倍局经历分析留存

```sql
-- 分析高倍局经历对留存的影响
SELECT
    r.reg_date,
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end as channel_category_name,
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END AS high_multi_exp,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date between 20260210 and 20260414
GROUP BY r.reg_date, 
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end,
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END
ORDER BY r.reg_date, case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end, high_multi_exp;
```

**查询结果：**

数据文件：`[8.3.4 高倍局经历留存数据](../../data/md/8.3.4.md)`

#### 8.3.5 按首日经济变化分析留存

```sql
-- 分析首日经济变化对留存的影响
SELECT
    r.reg_date,
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end as channel_category_name,
    CASE
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN g.total_diff_money < 0 THEN 'C: 小亏 (-1万~0)'
        WHEN g.total_diff_money < 10000 THEN 'D: 小赚 (0~1万)'
        WHEN g.total_diff_money < 50000 THEN 'E: 大赚 (1万~5万)'
        ELSE 'F: 巨赚 (>5万)'
    END AS money_change_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date between 20260210 and 20260414
GROUP BY r.reg_date, 
    case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end,
    CASE
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN g.total_diff_money < 0 THEN 'C: 小亏 (-1万~0)'
        WHEN g.total_diff_money < 10000 THEN 'D: 小赚 (0~1万)'
        WHEN g.total_diff_money < 50000 THEN 'E: 大赚 (1万~5万)'
        ELSE 'F: 巨赚 (>5万)'
    END
ORDER BY r.reg_date, case when r.channel_category_name in ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') then r.channel_category_name else '其他' end, money_change_group;
```

**查询结果：**

数据文件：`[8.3.5 首日经济变化留存数据](../../data/md/8.3.5.md)`

#### 8.3.6 按首日连胜连败分析留存

```sql
-- 分析首日连胜连败对留存的影响
SELECT
    r.reg_date,
    CASE 
        WHEN g.max_win_streak >= 3 THEN 'A: 连胜3+'
        WHEN g.max_win_streak = 2 THEN 'B: 连胜2'
        WHEN g.max_win_streak = 1 THEN 'C: 连胜1'
        WHEN g.max_lose_streak >= 3 THEN 'D: 连败3+'
        WHEN g.max_lose_streak = 2 THEN 'E: 连败2'
        ELSE 'F: 无连胜连败'
    END AS streak_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date between 20260210 and 20260414
  AND g.game_count > 0
GROUP BY r.reg_date,
    CASE 
        WHEN g.max_win_streak >= 3 THEN 'A: 连胜3+'
        WHEN g.max_win_streak = 2 THEN 'B: 连胜2'
        WHEN g.max_win_streak = 1 THEN 'C: 连胜1'
        WHEN g.max_lose_streak >= 3 THEN 'D: 连败3+'
        WHEN g.max_lose_streak = 2 THEN 'E: 连败2'
        ELSE 'F: 无连胜连败'
    END
ORDER BY r.reg_date, streak_group;
```

**查询结果：**

数据文件：`[8.3.6 首日连胜连败留存数据](../../data/md/8.3.6.md)`

#### 8.3.7 按首日逃跑行为分析留存

```sql
-- 按首日逃跑行为分组分析留存
SELECT
    r.reg_date,
    CASE 
        WHEN g.escape_count IS NULL OR g.escape_count = 0 THEN 'A: 无逃跑'
        WHEN g.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN g.escape_count = 2 THEN 'C: 逃跑2次'
        ELSE 'D: 逃跑3+次'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
  AND g.game_count > 0
GROUP BY r.reg_date,
    CASE 
        WHEN g.escape_count IS NULL OR g.escape_count = 0 THEN 'A: 无逃跑'
        WHEN g.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN g.escape_count = 2 THEN 'C: 逃跑2次'
        ELSE 'D: 逃跑3+次'
    END
ORDER BY r.reg_date, escape_group;
```

**分析要点**：
- 逃跑是负面情绪的直接行为信号，与连败分析互补
- 关注逃跑率（`escape_count / game_count`）与留存的关系
- 逃跑用户的经济状态（逃跑罚没加速银子耗尽）

#### 8.3.8 按首局胜负分析留存

```sql
-- 首局胜负对留存的影响
-- 优化说明：先用注册表筛出目标 uid，再用 INNER JOIN 限定明细表范围，减少窗口函数的计算量
WITH target_users AS (
    -- 先确定注册用户范围，避免明细表全表扫描
    SELECT uid, reg_date
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE reg_date BETWEEN 20260210 AND 20260414
),
first_game AS (
    SELECT
        g.uid,
        g.dt,
        g.result_id AS first_game_result,
        g.role AS first_game_role,
        g.magnification AS first_game_magnification,
        g.diff_money_pre_tax AS first_game_diff_money,
        ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.time_unix ASC) AS rn
    FROM tcy_temp.dws_ddz_daily_game g
    INNER JOIN target_users t ON g.uid = t.uid AND g.dt = t.reg_date
    WHERE g.robot != 1
      AND g.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)  -- 仅 APP 端
      AND g.play_mode IN (1, 2, 3, 5)  -- 仅银子玩法
)
SELECT
    r.reg_date,
    CASE 
        WHEN fg.first_game_result = 1 THEN 'A: 首局胜'
        WHEN fg.first_game_result = 2 THEN 'B: 首局负'
        ELSE 'C: 无对局'
    END AS first_game_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN first_game fg ON r.uid = fg.uid AND r.reg_date = fg.dt AND fg.rn = 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE 
        WHEN fg.first_game_result = 1 THEN 'A: 首局胜'
        WHEN fg.first_game_result = 2 THEN 'B: 首局负'
        ELSE 'C: 无对局'
    END
ORDER BY r.reg_date, first_game_group;
```

**分析要点**：
- 首局有新手配牌加持，预期首局胜率偏高
- 验证首局胜 vs 首局负的留存率差异，量化新手配牌策略的效果
- 可进一步交叉分析首局角色（地主/农民）× 首局胜负 × 留存

#### 8.3.9 按首日疑似破产分析留存

```sql
-- 首日是否疑似破产对留存的影响
SELECT
    r.reg_date,
    CASE 
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'C: 无对局'
        WHEN g.money_valley <= 1000 THEN 'A: 疑似破产（谷值≤1000）'
        ELSE 'B: 未破产'
    END AS bankrupt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(COALESCE(g.total_diff_money, 0)), 0) AS avg_diff_money,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE 
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'C: 无对局'
        WHEN g.money_valley <= 1000 THEN 'A: 疑似破产（谷值≤1000）'
        ELSE 'B: 未破产'
    END
ORDER BY r.reg_date, bankrupt_group;
```

**分析要点**：
- `money_valley` 是当日货币谷值，低于最低房间门槛即无法继续游戏
- 此处使用 1000 作为近似破产线（可根据实际最低房间 `room_currency_lower` 调整）
- 破产用户是经济系统兜底机制优化的核心目标群体

#### 8.3.10 按首日对局时长分析留存

```sql
-- 按首日总对局时长分组分析留存
SELECT
    r.reg_date,
    CASE 
        WHEN g.total_play_seconds IS NULL OR g.total_play_seconds = 0 THEN 'A: 无对局'
        WHEN g.total_play_seconds < 300 THEN 'B: <5分钟'
        WHEN g.total_play_seconds < 900 THEN 'C: 5-15分钟'
        WHEN g.total_play_seconds < 1800 THEN 'D: 15-30分钟'
        ELSE 'E: 30分钟+'
    END AS duration_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE 
        WHEN g.total_play_seconds IS NULL OR g.total_play_seconds = 0 THEN 'A: 无对局'
        WHEN g.total_play_seconds < 300 THEN 'B: <5分钟'
        WHEN g.total_play_seconds < 900 THEN 'C: 5-15分钟'
        WHEN g.total_play_seconds < 1800 THEN 'D: 15-30分钟'
        ELSE 'E: 30分钟+'
    END
ORDER BY r.reg_date, duration_group;
```

**分析要点**：
- 总时长是沉浸度的直接体现，与对局数互补（同样 5 局，单局时长差异大）
- 极短时长（<5分钟）可能对应秒退/逃跑用户
- 可进一步分析 `avg_game_seconds` 异常短（<60s）的对局占比

#### 8.3.11 按注册时段分析留存

```sql
-- 按注册时段分组分析留存
SELECT
    r.reg_date,
    CASE 
        WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨(0-6)'
        WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午(6-12)'
        WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午(12-18)'
        WHEN HOUR(r.reg_datetime) BETWEEN 18 AND 21 THEN 'D: 晚间(18-22)'
        ELSE 'E: 深夜(22-24)'
    END AS reg_hour_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE 
        WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨(0-6)'
        WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午(6-12)'
        WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午(12-18)'
        WHEN HOUR(r.reg_datetime) BETWEEN 18 AND 21 THEN 'D: 晚间(18-22)'
        ELSE 'E: 深夜(22-24)'
    END
ORDER BY r.reg_date, reg_hour_group;
```

**分析要点**：
- 不同时段注册的用户可能对应不同的获客渠道和用户画像
- 凌晨注册用户可能是高粘性夜间玩家或低质量刷量用户
- 晚间（18-22）是游戏活跃高峰期，该时段注册用户的首日体验可能更好（匹配速度快、对手多样性高）

### 8.4 首日仅 1 局流失用户专项分析

> **分析背景**：首日仅完成 1 局的用户占比 9.3%，次留仅 9.6%。这部分用户进入了游戏但体验首局后即离开，是首局体验优化的核心目标群体。由于只有一条对局记录，可以精确分析这唯一一局的各个维度，定位流失原因。
>
> 以下 SQL 均从明细表 `dws_ddz_daily_game` 中取首日唯一一局的数据，与注册表和登录表关联分析留存。

#### 8.4.1 首局胜负 × 角色 对留存的影响

```sql
-- 1局用户：首局胜负 × 角色（地主/农民）对留存的影响
SELECT
    r.reg_date,
    CASE 
        WHEN g.role = 1 AND g.result_id = 1 THEN 'A: 地主-胜'
        WHEN g.role = 1 AND g.result_id = 2 THEN 'B: 地主-负'
        WHEN g.role = 2 AND g.result_id = 1 THEN 'C: 农民-胜'
        WHEN g.role = 2 AND g.result_id = 2 THEN 'D: 农民-负'
        ELSE 'E: 其他'
    END AS role_result_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE 
        WHEN g.role = 1 AND g.result_id = 1 THEN 'A: 地主-胜'
        WHEN g.role = 1 AND g.result_id = 2 THEN 'B: 地主-负'
        WHEN g.role = 2 AND g.result_id = 1 THEN 'C: 农民-胜'
        WHEN g.role = 2 AND g.result_id = 2 THEN 'D: 农民-负'
        ELSE 'E: 其他'
    END
ORDER BY r.reg_date, role_result_group;
```

**分析要点**：
- 首局有新手配牌，预期地主胜率偏高；如果地主负的留存极低，说明配牌效果不足
- 对比「地主胜 vs 农民胜」的留存差异，判断角色体验是否影响留存
- 首局输的用户如果留存显著低于赢的用户，说明新手对首局胜负非常敏感

#### 8.4.2 首局倍数对留存的影响

```sql
-- 1局用户：首局玩法 × 倍数对留存的影响
SELECT
    r.reg_date,
    CASE g.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '赖子'
        ELSE '其他'
    END AS play_mode_name,
    CASE
        WHEN g.magnification <= 6 THEN 'A: 低倍(<=6)'
        WHEN g.magnification <= 12 THEN 'B: 中低倍(6-12)'
        WHEN g.magnification <= 24 THEN 'C: 中倍(12-24)'
        ELSE 'D: 高倍(24+)'
    END AS multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.magnification), 1) AS avg_magnification,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE g.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '赖子'
        ELSE '其他'
    END,
    CASE
        WHEN g.magnification <= 6 THEN 'A: 低倍(<=6)'
        WHEN g.magnification <= 12 THEN 'B: 中低倍(6-12)'
        WHEN g.magnification <= 24 THEN 'C: 中倍(12-24)'
        ELSE 'D: 高倍(24+)'
    END
ORDER BY r.reg_date, play_mode_name, multi_group;
```

**分析要点**：
- **必须按玩法拆分看倍数**：赖子玩法有万能牌，炸弹概率远高于经典，天然高倍；不洗牌保留上局牌序，倍数分布也与经典不同。混合在一起会把玩法差异误读为倍数体验差异
- 同样是"高倍(24+)"，经典玩法的 24 倍局是相对罕见的极端体验，而赖子玩法可能是常态——对新手的心理冲击不同
- 首局低倍（<=6，即叫地主无人抢无炸无春天）可能缺乏刺激性，但需分玩法看是否合理
- 结合胜负交叉：同一玩法内，高倍胜 vs 高倍负的留存差异预期最大

#### 8.4.3 首局经济变化对留存的影响

```sql
-- 1局用户：首局经济变化对留存的影响
SELECT
    r.reg_date,
    CASE
        WHEN g.diff_money_pre_tax < -5000 THEN 'A: 大亏(<-5000)'
        WHEN g.diff_money_pre_tax < 0 THEN 'B: 小亏(-5000~0)'
        WHEN g.diff_money_pre_tax = 0 THEN 'C: 持平'
        WHEN g.diff_money_pre_tax <= 5000 THEN 'D: 小赚(0~5000)'
        ELSE 'E: 大赚(>5000)'
    END AS money_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.diff_money_pre_tax), 0) AS avg_diff,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE
        WHEN g.diff_money_pre_tax < -5000 THEN 'A: 大亏(<-5000)'
        WHEN g.diff_money_pre_tax < 0 THEN 'B: 小亏(-5000~0)'
        WHEN g.diff_money_pre_tax = 0 THEN 'C: 持平'
        WHEN g.diff_money_pre_tax <= 5000 THEN 'D: 小赚(0~5000)'
        ELSE 'E: 大赚(>5000)'
    END
ORDER BY r.reg_date, money_group;
```

**分析要点**：
- 1 局用户的经济变化完全由这一局决定，可以精确衡量单局输赢对留存的冲击
- 首局大亏（>5000 银子）可能产生强烈的"被坑"感
- 首局大赚是否能有效提升留存，验证"赢钱即粘性"假设

#### 8.4.4 首局是否逃跑对留存的影响

```sql
-- 1局用户：首局是否逃跑对留存的影响
SELECT
    r.reg_date,
    CASE 
        WHEN g.cut < 0 THEN 'A: 逃跑'
        ELSE 'B: 正常完成'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE 
        WHEN g.cut < 0 THEN 'A: 逃跑'
        ELSE 'B: 正常完成'
    END
ORDER BY r.reg_date, escape_group;
```

**分析要点**：
- 首局即逃跑是最极端的负面体验信号，预期该群体留存极低
- 逃跑还会产生罚没惩罚，加剧负面感受
- 关注首局逃跑用户的占比——如果占比显著，说明首局体验存在系统性问题

#### 8.4.5 首局时长对留存的影响

```sql
-- 1局用户：首局时长对留存的影响
SELECT
    r.reg_date,
    CASE
        WHEN g.timecost < 60 THEN 'A: <1分钟'
        WHEN g.timecost < 120 THEN 'B: 1-2分钟'
        WHEN g.timecost < 240 THEN 'C: 2-4分钟'
        WHEN g.timecost < 480 THEN 'D: 4-8分钟'
        ELSE 'E: 8分钟+'
    END AS duration_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.timecost), 0) AS avg_seconds,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE
        WHEN g.timecost < 60 THEN 'A: <1分钟'
        WHEN g.timecost < 120 THEN 'B: 1-2分钟'
        WHEN g.timecost < 240 THEN 'C: 2-4分钟'
        WHEN g.timecost < 480 THEN 'D: 4-8分钟'
        ELSE 'E: 8分钟+'
    END
ORDER BY r.reg_date, duration_group;
```

**分析要点**：
- <1 分钟的对局很可能是逃跑或被秒杀（春天），体验极差
- 时长过短可能说明新手还没理解规则就结束了
- 正常斗地主一局约 2-5 分钟，时长在此区间的用户体验应最完整

#### 8.4.6 首局玩法对留存的影响

```sql
-- 1局用户：首局玩法对留存的影响
SELECT
    r.reg_date,
    CASE g.play_mode
        WHEN 1 THEN 'A: 经典'
        WHEN 2 THEN 'B: 不洗牌'
        WHEN 3 THEN 'C: 赖子'
        WHEN 5 THEN 'D: 比赛'
        ELSE 'E: 其他'
    END AS play_mode_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.magnification), 1) AS avg_magnification,
    ROUND(AVG(g.diff_money_pre_tax), 0) AS avg_diff,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE g.play_mode
        WHEN 1 THEN 'A: 经典'
        WHEN 2 THEN 'B: 不洗牌'
        WHEN 3 THEN 'C: 赖子'
        WHEN 5 THEN 'D: 比赛'
        ELSE 'E: 其他'
    END
ORDER BY r.reg_date, play_mode_group;
```

**分析要点**：
- 新手默认进入经典玩法，如果首局即选择不洗牌/赖子，说明用户有主动探索行为，可能是有经验玩家
- 赖子玩法随机性更强（万能牌），倍数波动可能更大，对新手是否友好需验证
- 比赛玩法（play_mode=5）的用户画像可能与普通用户不同，留存特征也可能差异明显
- 各玩法的平均倍数和平均输赢差异可揭示不同玩法对新手经济体验的影响

#### 8.4.7 首局房间底分对留存的影响

```sql
-- 1局用户：首局房间底分对留存的影响
SELECT
    r.reg_date,
    CASE
        WHEN g.room_base <= 50 THEN 'A: 新手场(<=50)'
        WHEN g.room_base <= 200 THEN 'B: 低分场(50-200)'
        WHEN g.room_base <= 1000 THEN 'C: 中分场(200-1000)'
        ELSE 'D: 高分场(>1000)'
    END AS room_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.room_base), 0) AS avg_base,
    ROUND(AVG(ABS(g.diff_money_pre_tax)), 0) AS avg_abs_diff,
    ROUND(AVG(g.start_money), 0) AS avg_start_money,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE
        WHEN g.room_base <= 50 THEN 'A: 新手场(<=50)'
        WHEN g.room_base <= 200 THEN 'B: 低分场(50-200)'
        WHEN g.room_base <= 1000 THEN 'C: 中分场(200-1000)'
        ELSE 'D: 高分场(>1000)'
    END
ORDER BY r.reg_date, room_group;
```

**分析要点**：
- 底分直接决定单局最低输赢金额（底分 × 倍数），新手如果误入高底分房间可能一局即"破产"
- 关注 `avg_start_money / room_base` 的比值：如果携银仅刚好达到门槛（比值 < 3），说明用户在承受超出经济能力的风险
- 新手场（底分 ≤50）是默认引导目标，如果有 1 局用户首局就进了中高底分场，说明引导路径可能存在问题
- `avg_abs_diff`（平均单局绝对输赢）可衡量各房间对新手银子的实际冲击幅度

#### 8.4.8 首局综合画像（多维交叉）

```sql
-- 1局用户综合画像：胜负 × 倍数高低 × 经济变化幅度
SELECT
    r.reg_date,
    CASE WHEN g.result_id = 1 THEN '胜' ELSE '负' END AS result,
    CASE
        WHEN g.magnification <= 12 THEN '低中倍(<=12)'
        ELSE '高倍(>12)'
    END AS multi_level,
    CASE
        WHEN g.cut < 0 THEN '逃跑'
        ELSE '正常'
    END AS is_escape,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.diff_money_pre_tax), 0) AS avg_diff,
    ROUND(AVG(g.timecost), 0) AS avg_seconds,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_appdaily_game_stat s ON r.uid = s.uid AND r.reg_date = s.dt AND s.game_count = 1
INNER JOIN tcy_temp.dws_ddz_daily_game g ON r.uid = g.uid AND r.reg_date = g.dt AND g.robot != 1
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY r.reg_date,
    CASE WHEN g.result_id = 1 THEN '胜' ELSE '负' END,
    CASE WHEN g.magnification <= 12 THEN '低中倍(<=12)' ELSE '高倍(>12)' END,
    CASE WHEN g.cut < 0 THEN '逃跑' ELSE '正常' END
ORDER BY r.reg_date, result, multi_level, is_escape;
```

**分析要点**：
- 多维交叉可识别最差体验组合，如「负 + 高倍 + 逃跑」，预期留存接近 0
- 找到最佳体验组合（如「胜 + 中低倍 + 正常完成」），作为首局体验设计的目标模型
- 通过各组合的用户占比，判断当前首局体验设计是否让多数用户落入好的区间

> **按玩法拆分的留存分析**已移至专题文档 [`retention-by-mode.md`](retention-by-mode.md)，包含分玩法 × 胜率/倍数/经济变化/高倍局/连胜连败/破产等六个维度的交叉分析。

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
| 新手配牌优化 | 前 N 局配牌保障胜率（当前仅首局） | 延长正向体验窗口 | 前N局胜率、次留 |
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
| R01 | 游戏次日留存率 | 次留（注册后第 1 天）有对局的新用户 / 当日新增且有对局的用户 | 渠道/设备/时段 |
| R02 | 游戏 3 日留存率 | 3 留（注册后第 2 天）有对局的新用户 / 当日新增且有对局的用户 | 同上 |
| R03 | 游戏 7 日留存率 | 7 留（注册后第 6 天）有对局的新用户 / 当日新增且有对局的用户 | 同上 |
| R04 | 游戏 14 日留存率 | 14 留（注册后第 13 天）有对局的新用户 / 当日新增且有对局的用户 | 同上 |
| R05 | 游戏 30 日留存率 | 30 留（注册后第 29 天）有对局的新用户 / 当日新增且有对局的用户 | 同上 |

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

> **文档版本**：v5.0
> **创建日期**：2026-03-23
> **更新说明**：
> - v2.0：整合同城游平台业务背景、斗地主具体玩法规则、数据表结构及取数 SQL
> - v2.1：修正 magnification 字段含义；留存口径统一为游戏留存
> - v3.0：重构 DWS 层架构；修正字段名；倍数相关字段直接读列
> - v3.1：修复 StarRocks 日期函数；修正新用户分端口径；优化 Bucket 配置
> - v4.0：修正留存口径（从"游戏留存"改为"新增用户留存"）；更新数据源；新增字段；简化 SQL 结构
> - v4.1：补充 5 个分析维度（逃跑行为、首局胜负、疑似破产、对局时长、注册时段）
> - v4.2：新增 8.4 首日仅 1 局流失用户专项分析（8 个 SQL）
> - v4.3：新增 8.5 按玩法拆分的留存分析（6 个 SQL）
> - **v5.0**：**三层解耦重构** — 新增"三层分析体系总览"；将 8.5 按玩法拆分分析移至 `retention-by-mode.md`；将 4.4 中的玩法偏好指标移至分玩法文档；清理 SQL 中的 `app_code IS NOT NULL` 过滤（全局分析使用全量数据）；`retention-by-client-lang.md` 改为纯增量文档不再重复基础章节；三个文档重命名为 `retention-global.md` / `retention-by-mode.md` / `retention-by-client-lang.md`
>
> **适用范围**：同城游·斗地主 APP 端新增用户留存专项分析
> **关联文档**：
> - [`retention-by-mode.md`](retention-by-mode.md)（分玩法留存分析）
> - [`retention-by-client-lang.md`](retention-by-client-lang.md)（分客户端语言留存分析）
> **使用建议**：
> 1. 确认 `dws_dq_app_daily_reg`、`dws_dq_daily_login`、`dws_ddz_daily_game` 三个 DWS 表已构建
> 2. 运行 8.2 节留存分析 SQL，获取各维度留存率
> 3. 运行 8.3 节游戏行为分析 SQL，分析首日行为与留存关系
> 4. 将分析结果按「发现卡片」模板输出，推动产品优化
> 5. 如需按玩法/客户端语言拆分，参见对应关联文档
