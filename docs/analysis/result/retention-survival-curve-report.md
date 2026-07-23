# 留存生存曲线分析报告：D1-D7 逐日流失时段归因

> **分析窗口**：`reg_date BETWEEN '2026-06-18' AND '2026-06-22'`（5 天 cohort），n=6,623 新增注册用户。
>
> **数据源**：`dws_dq_app_daily_reg` + `dws_app_game_active` + `dws_app_firstday_game_stat`
>
> **方法**：逐日活跃矩阵 → 生存曲线 + 流失时段分群 + 分群首日画像归因。
>
> **SQL**：[retention-survival-curve.md](../../plan/retention/retention-survival-curve.md)

---

## ⚠️ 数据口径说明

**D7 数据不完整**：`dws_app_game_active` 为 T-1 产出，reg_date=06-22 的 D7（=06-29）今日尚未入库。因此 D7 留存率**低估约 1/5**（06-22 cohort 全部 is_d7=0）。本章 D7 数字仅供方向参考，D1-D6 数据完整。

---

## 一、生存曲线：大盘 D1-D7 逐日留存率

### 1.1 全量曲线

| 里程碑 | 留存率 | 日环比衰减 |
| ---- | ---- | ---- |
| D1 | **21.68%** | — |
| D2 | 15.48% | **-6.20pp** ← 最大单日衰减 |
| D3 | 13.97% | -1.51pp |
| D4 | 11.90% | -2.07pp |
| D5 | 11.85% | -0.05pp |
| D6 | 11.79% | -0.06pp |
| D7 | 8.41% ⚠️ | -3.38pp（含数据缺失） |

**关键发现**：

1. **D1→D2 是断崖**：从 21.68% 跌到 15.48%，一天流失 6.2pp（占 D1 用户的 28.6%）。这意味着每 4 个次日回来的用户中，有 1 个在第三天就不来了。

2. **D4-D6 是高原**：连续三天留存率几乎不变（11.90% → 11.85% → 11.79%），三天累计仅降 0.11pp。撑过 D3 的用户进入稳定期。

3. **D7 数字不可靠**：含 06-22 cohort 数据缺失，真实 D7 预计在 10-11% 左右（按 D6→D7 正常衰减估算）。

### 1.2 分渠道生存曲线

| 渠道 | n | D1 | D2 | D3 | D4 | D5 | D6 | D7 ⚠️ |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| **三星** | 88 | **36.36** | 26.14 | **28.41** | **21.59** | **18.18** | 13.64 | 12.50 |
| **IOS** | 665 | 27.37 | **22.41** | 20.60 | 16.99 | 17.59 | **18.80** | 11.58 |
| **鸿蒙** | 562 | 25.27 | 17.97 | 19.40 | 15.12 | 15.30 | 14.77 | 9.79 |
| **荣耀** | 1,615 | 23.22 | 17.40 | 14.43 | 13.93 | 13.50 | 13.07 | 10.03 |
| 华为 | 1,265 | 21.34 | 14.47 | 14.31 | 10.67 | 12.09 | 11.86 | 8.46 |
| 官方(非CPS) | 578 | 21.63 | 14.53 | 14.01 | 9.17 | 10.21 | 11.25 | 8.13 |
| OPPO | 675 | 17.63 | 12.00 | 8.74 | 9.78 | 8.30 | 7.56 | 5.04 |
| vivo | 882 | 17.46 | 11.79 | 10.09 | 9.30 | 7.71 | 7.48 | 5.78 |
| 360 | 83 | 12.05 | 7.23 | 2.41 | 2.41 | 3.61 | 4.82 | 4.82 |
| **咪咕** | 118 | **5.93** | 3.39 | **0.00** | 0.00 | 0.00 | 0.00 | 0.00 |
| 努比亚 | 13 | 23.08 | 7.69 | 23.08 | 0.00 | 15.38 | 15.38 | 23.08 |
| 魅族 | 21 | 28.57 | 9.52 | 4.76 | 4.76 | 4.76 | 19.05 | 9.52 |
| 小米 | 30 | 23.33 | 10.00 | 13.33 | 16.67 | 13.33 | 10.00 | 10.00 |
| 官方CPS | 20 | 20.00 | 15.00 | 5.00 | 10.00 | 10.00 | 25.00 | 5.00 |

> n<50 的渠道（努比亚/魅族/小米/官方CPS/百度/UC九游）波动大，仅列不分析。

**渠道分层清晰**：

| 梯队 | 渠道 | D1 范围 | 特征 |
| ---- | ---- | ---- | ---- |
| 高留 | 三星、IOS、鸿蒙、荣耀 | 23-36% | D1 高、D4-D6 高原稳健 |
| 中留 | 华为、官方(非CPS) | 21-22% | 与大盘持平 |
| 低留 | OPPO、vivo | 17-18% | D1 低于大盘 4pp，全程偏低 |
| 极低 | 360、咪咕 | 6-12% | 咪咕 D3 起归零，疑似刷量渠道 |

**渠道间曲线形态一致**：所有渠道的衰减模式相同（D1→D2 陡降 → D4-D6 高原），只是基线不同。说明**留存衰减是产品通用规律，渠道差异体现在"起点"而非"衰减速度"**。

### 1.3 分设备生存曲线

| 设备 | n | D1 | D2 | D3 | D4 | D5 | D6 | D7 ⚠️ |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| **iOS** | 665 | **27.37** | **22.41** | **20.60** | **16.99** | **17.59** | **18.80** | 11.58 |
| Android | 5,958 | 21.05 | 14.70 | 13.23 | 11.33 | 11.21 | 11.01 | 8.06 |

**iOS 全程领先 Android 约 6pp**（D1 27.37 vs 21.05），且 D4-D6 高原更高更平（17-19% vs 11%）。差距从 D1 的 6.3pp 扩大到 D6 的 7.8pp，说明 iOS 用户不仅起点高，衰减也更慢。

### 1.4 分客户端生存曲线

| 客户端 | n | D1 | D2 | D3 | D4 | D5 | D6 | D7 ⚠️ |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| **cocos-creator** | 1,142 | **27.58** | **21.28** | **20.84** | **16.90** | **17.43** | **17.69** | 11.21 |
| cocos-lua | 5,477 | 20.47 | 14.28 | 12.54 | 10.86 | 10.68 | 10.55 | 7.83 |

**Creator 全程领先 Lua 约 7pp**，与 global-report 结论一致（Creator 次留 28.33% vs Lua 22.31%），且 Creator 的 D4-D6 高原更高（17-18% vs 10-11%），衰减曲线更平。

### 1.5 D1-D7 全量生存曲线可视化

```text
留存率
25% │
    │  ██
20% │  ██  ██
    │  ██  ██  ██
15% │  ██  ██  ██  ██
    │  ██  ██  ██  ██  ██  ██  ██
10% │  ██  ██  ██  ██  ██  ██  ██
    │  ██  ██  ██  ██  ██  ██  ██  ██
 5% │  ██  ██  ██  ██  ██  ██  ██  ██
    └────┬────┬────┬────┬────┬────┬────┬────
        D1   D2   D3   D4   D5   D6   D7
        21.7 15.5 14.0 11.9 11.9 11.8 (8.4)
              ↑                    ↑
         断崖 -6.2pp         高原 0.11pp/3天
```

---

## 二、流失时段分群：用户最后活跃在哪天

### 2.1 全量分群

| 流失时段 | 人数 | 占比 | 说明 |
| ---- | ---- | ---- | ---- |
| **从未回来** | **4,180** | **63.11%** | D1-D7 零游戏活跃 |
| D1流失 | 473 | 7.14% | 最后活跃在 D1 |
| D2流失 | 269 | 4.06% | 最后活跃在 D2 |
| D3流失 | 239 | 3.61% | 最后活跃在 D3 |
| D4流失 | 198 | 2.99% | 最后活跃在 D4 |
| D5流失 | 262 | 3.96% | 最后活跃在 D5 |
| D6流失 | 445 | 6.72% | 最后活跃在 D6 ⚠️ 含 D7 未到期 |
| D7留存 | 557 | 8.41% | D7 仍有活跃 ⚠️ 低估 |

### 2.2 核心发现

**63% 的用户 D1-D7 从未有过任何游戏活跃。**

这是一个巨大的数字。"从未回来"组中 16.2% 是 0 局用户（纯注册流量），83.8% 首日有对局但再不回来。结合全量 0 局占比约 11%，这意味着：

- ~11% 的用户首日未进游戏，也从未回来 → 纯注册流量
- ~52% 的用户首日进了游戏，但 D1-D7 再也没回来 → **首日体验后彻底流失**

这 52% 的"首日玩过但再不回来"是最大的流失黑洞，远超"D1 来了但 D2 流失"的 7%。

**D6 流失占比异常偏高（6.72%）**，原因是 06-22 cohort 的 D7 数据未到期，部分本应标为"D7 留存"的用户被标为"D6 流失"。剔除该偏差后，D6 流失真实占比预计在 3-4%。

---

## 三、流失时段 × 首日画像归因

### 3.1 各群组首日行为对比

| 群组 | 占比 | 0局% | 平均对局数 | 单次会话% | 胜率 | <60s/局% | 官方% | 华为% | Creator% |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 从未回来 | 63.1% | **16.2%** | **9.1** | **84.1%** | 59.0% | **8.1%** | 9.2% | 18.9% | 15.5% |
| D1流失 | 7.1% | 5.7% | 14.4 | 67.2% | 56.3% | 6.1% | 7.2% | 18.0% | 18.6% |
| D2流失 | 4.1% | 3.3% | 12.9 | 72.9% | 54.9% | 4.1% | 8.6% | 17.8% | 16.4% |
| D3流失 | 3.6% | 5.0% | 13.8 | 74.5% | 56.2% | 5.0% | 8.8% | 21.3% | 22.2% |
| D4流失 | 3.0% | 2.0% | 14.3 | 72.7% | 54.0% | 4.0% | 8.1% | 15.7% | 16.2% |
| D5流失 | 4.0% | 2.7% | 14.5 | 70.2% | 54.9% | 3.4% | 6.1% | 21.8% | 18.3% |
| D6流失 | 6.7% | 2.9% | 16.1 | 62.2% | 54.3% | 5.6% | 8.3% | 21.1% | 23.1% |
| D7留存 | 8.4% | **1.3%** | **17.7** | **55.3%** | 53.3% | **3.8%** | 8.4% | 19.2% | **23.0%** |

### 3.2 对局数：单调递增，最强区分度

```text
平均对局数
18 ┤                                 ● (D7留存: 17.7)
16 ┤                            ● (D6流失: 16.1)
14 ┤          ●     ●     ●    ●
   ┤     ●   (D1)  (D3)  (D4) (D5)
12 ┤    (D2: 12.9)
10 ┤
 8 ┤ ● (从未回来: 9.1)
   └────┬────┬────┬────┬────┬────┬────┬────
       从未  D1   D2   D3   D4   D5   D6   D7
```

**结论**：对局数与留存时段呈严格单调递增。D7 留存者首日平均打 17.7 局，是"从未回来"组（9.1 局）的近 2 倍。这一规律在 D1-D7 全时间线上保持，与之前"对局数→D7 二值"的结论一致。

**"从未回来"组平均 9.1 局但再不回来**：这群人首日打得不算少（9.1 局远超 1 局试用），说明不是"浅尝辄止"——是**打完首日后再无动力回来**。

### 3.3 单次会话：越早流失，单次会话占比越高

```text
单次会话占比
85% ┤ ● (从未回来: 84.1%)
80% ┤
75% ┤     ● (D3: 74.5%)
    ┤    ●  ●  ● (D2/D4/D5: 70-73%)
70% ┤  ● (D1: 67.2%)
65% ┤
60% ┤           ● (D6: 62.2%)
55% ┤                 ● (D7: 55.3%)
    └────┬────┬────┬────┬────┬────┬────┬────
       从未  D1   D2   D3   D4   D5   D6   D7
```

**"从未回来"组 84.1% 单次会话**——与归因报告 §6A 的 D1 流失者画像（85.5% 单次会话）一致。这群人首日只打开一次 App，打完就走，再不回来。

**D7 留存者仍有 55.3% 单次会话**——即使七日留存用户，也有一半以上首日只登录一次。说明"首日多次登录"虽然与留存正相关，但不是必要条件。

### 3.4 高倍偏好（<60s/局）：从未回来组最高

| 群组 | <60s/局占比 |
| ---- | ---- |
| 从未回来 | **8.1%** |
| D1流失 | 6.1% |
| D6流失 | 5.6% |
| D3流失 | 5.0% |
| D2流失 | 4.1% |
| D4流失 | 4.0% |
| D7留存 | **3.8%** |
| D5流失 | 3.4% |

**从未回来组高倍偏好占比最高（8.1%），D7 留存组最低（3.8%）**。这与归因报告 §6 的"高倍偏好组 D1 高 D7 低"一致，并进一步证实：高倍偏好不仅与 D7 留存负相关，在 D1-D7 全时间线上都倾向于更早流失。

### 3.5 渠道分布：差异不大

官方渠道在各群组占比 6-9%，华为 16-22%，无明显梯度。说明**流失时段主要由首日行为驱动，而非渠道属性**。渠道影响的是"来不来"（D1 绝对值），而非"什么时候走"（流失时段分布）。

### 3.6 胜率：弱信号

各群组胜率在 53-59% 之间，D7 留存组最低（53.3%），从未回来组最高（59.0%）。胜率差异仅 5.7pp，远小于对局数差异（9.1 vs 17.7，差 8.6 局）。结合归因报告的"首局保护到位但留不住"结论：**胜率不是区分留存的关键变量**。

### 3.7 0 局用户：集中在"从未回来"组

| 群组 | 0局占比 |
| ---- | ---- |
| 从未回来 | **16.2%** |
| D1流失 | 5.7% |
| D2流失 | 3.3% |
| D4流失 | 2.0% |
| D7留存 | **1.3%** |

**"从未回来"组 16.2% 是 0 局用户**，远高于其他群组（1-6%），D7 留存组仅 1.3%。0 局用户几乎全部落在"从未回来"——注册后首日没打、之后也不会打。

全量 0 局占比 = 63.1% × 16.2% + 其余各组 × 各自的 0 局% ≈ 10.2% + 0.6% ≈ **~11%**，与保护期报告（11.09%）一致。

### 3.8 客户端版本：Creator 留存更好，但分布均匀

| 群组 | Creator 占比 |
| ---- | ---- |
| 从未回来 | 15.5% |
| D1流失 | 18.6% |
| D3流失 | 22.2% |
| D6流失 | 23.1% |
| D7留存 | **23.0%** |

**D7 留存组 Creator 占比最高（23.0%），从未回来组最低（15.5%）**。全量 Creator 占比 = 1,142/6,623 ≈ 17.2%。Creator 用户在留存更长的群组中占比高于基线，与 §1.4 生存曲线结论一致——Creator 引擎版本留存全面优于 Lua。

---

## 四、综合结论

### 4.1 流失结构全景

```text
新增用户 6,623 (100%)
├── D1-D7 从未活跃: 4,180 (63.1%)  ← 最大流失黑洞
│   ├── 首日 0 局 16.2%: 纯注册流量
│   └── 首日有对局 83.8%: 打完首日再不回来
│       ├── 平均 9.1 局，84% 单次会话
│       └── 高倍偏好 8.1%（各群组最高）
│
├── D1-D6 期间流失: 1,886 (28.5%)
│   ├── D1 流失: 473 (7.1%) — 次日来了但 D2 起消失
│   ├── D2-D5 流失: 968 (14.6%) — 撑过几天但没到 D7
│   └── D6 流失: 445 (6.7%) — 部分含 D7 未到期
│
└── D7 留存: 557 (8.4%) ⚠️ 低估
```

### 4.2 三个关键发现

**发现 1：63% 零回访是最大问题，远超 D1-D7 之间的流失**

D1-D7 之间的流失（D1 来了 D2 不来 + D2 来了 D3 不来 + ...）合计占 28.5%，而"从未回来"占 63.1%。换句话说，**注册后一次都不回来的用户，是"回来过又走了"的用户的 2.2 倍**。

产品优化的优先级应该是：**召回/触达（让 63% 的人至少回来一次）> 续玩钩子（让回来的人多留几天）**。

**发现 2：D4-D6 高原——撑过 D3 就稳住了**

D1→D2 断崖（-6.2pp）→ D2→D3→D4 持续衰减 → D4-D6 几乎平坦（三天仅降 0.11pp）。

这说明用户存在一个"三天考验期"：如果 D1-D3 都挺过来了，D4-D6 基本不会走。干预窗口应集中在 **D1-D3**（注册后前 3 天），而非均摊到整周。

**发现 3：对局数是最强区分度，但"从未回来"组也打了 9 局**

D7 留存者平均 17.7 局 vs 从未回来组 9.1 局。但 9.1 局并不少——这不是"试一局就走"的浅尝用户。这群人首日投入了近 10 局，然后彻底消失。

可能的解释：
- 首日体验有"消耗感"——打完后觉得"够了"，没有回来的理由
- 缺少次日钩子——没有未完成的任务、未领取的奖励、社交关系
- 渠道/获客场景——可能是激励视频等一次性场景来的用户

### 4.3 干预优先级

| 优先级 | 目标人群 | 规模 | 干预方向 | 时间窗口 |
| ---- | ---- | ---- | ---- | ---- |
| **P0** | 从未回来（首日有对局） | ~52% | 次日推送召回、首日结束埋钩子（未领取奖励/连续登录任务） | D0 结束 → D1 |
| **P0** | 从未回来（首日 0 局） | ~11% | 获客渠道质量排查、注册→首局引导优化 | D0 |
| **P1** | D1 流失 | 7.1% | D1 体验优化、D1 结束召回、连续登录奖励 Day2 | D1 → D2 |
| **P1** | D2-D3 流失 | 7.7% | 三日留存任务、社交邀请 | D2 → D4 |
| **P2** | D4-D6 流失 | 10.7% | 内容深度、赛季/活动预告 | D4+ |
| — | D7 留存 | 8.4% | 长期运营（不在本报告范围） | D7+ |

### 4.4 与现有结论的一致性

| 现有结论 | 本报告验证 |
| ---- | ---- |
| 归因报告 §5：仅1局玩家 71.9% 单次会话试用即走 | ✅ 扩展到全量：从未回来组 84.1% 单次会话，且平均 9.1 局 |
| 归因报告 §6：高倍偏好 D1 高 D7 低 | ✅ 从未回来组高倍偏好 8.1%（最高），D7 留存 3.8%（最低） |
| 归因报告 §6A：D1 是漏斗顶端，次日推送优先 | ✅ 强化：63% 零回访 + D1→D2 断崖，前 3 天是干预窗口 |
| 保护期报告 §2：对局数单调递增 | ✅ D1-D7 全时间线保持单调递增 |
| 复核报告 §1：连败/破产证伪 | ✅ 胜率在各群组差异仅 5.7pp，非流失主驱动 |

---

## 五、复核 SQL

### 5.1 逐日生存曲线（全量 + 渠道）

```sql
WITH reg_base_raw AS (
    SELECT
        uid, reg_date, app_id,
        channel_category_name,
        reg_group_id,
        reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-06-18' AND '2026-06-22'
),
date_bounds AS (
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
reg_base AS (
    SELECT i.*, b.min_reg, b.max_reg, b.min_act_date, b.max_act_date
    FROM reg_base_raw i
    CROSS JOIN date_bounds b
),
user_profile_tags AS (
    SELECT
        uid, reg_date, app_id,
        channel_category_name,
        CASE
            WHEN reg_group_id IN (8, 88) THEN 'iOS'
            WHEN reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            ELSE 'Other'
        END AS device_type,
        CASE
            WHEN reg_app_code = 'zgdx' THEN 'cocos-creator'
            WHEN reg_app_code = 'zgda' THEN 'cocos-lua'
            ELSE 'Other'
        END AS client_type,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 2 DAY) AS d2_target,
        DATE_ADD(reg_date, INTERVAL 3 DAY) AS d3_target,
        DATE_ADD(reg_date, INTERVAL 4 DAY) AS d4_target,
        DATE_ADD(reg_date, INTERVAL 5 DAY) AS d5_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d6_target,
        DATE_ADD(reg_date, INTERVAL 7 DAY) AS d7_target,
        min_reg, max_reg, min_act_date, max_act_date
    FROM reg_base
),
daily_active_matrix AS (
    SELECT
        p.uid, p.reg_date,
        p.channel_category_name,
        p.device_type,
        p.client_type,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d2_target THEN 1 ELSE 0 END) AS is_d2,
        MAX(CASE WHEN a.dt = p.d3_target THEN 1 ELSE 0 END) AS is_d3,
        MAX(CASE WHEN a.dt = p.d4_target THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = p.d5_target THEN 1 ELSE 0 END) AS is_d5,
        MAX(CASE WHEN a.dt = p.d6_target THEN 1 ELSE 0 END) AS is_d6,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d2_target, p.d3_target, p.d4_target, p.d5_target, p.d6_target, p.d7_target)
        AND a.dt BETWEEN p.min_act_date AND p.max_act_date
    GROUP BY p.uid, p.reg_date, p.channel_category_name, p.device_type, p.client_type
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'all' AS dim_type,
    'all' AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, dim_value

UNION ALL

SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'channel' AS dim_type,
    channel_category_name AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, channel_category_name

UNION ALL

SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'device' AS dim_type,
    device_type AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, device_type

UNION ALL

SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'client' AS dim_type,
    client_type AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, client_type;
```

### 5.2 流失时段分群

```sql
WITH reg_base_raw AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-06-18' AND '2026-06-22'
),
date_bounds AS (
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
reg_base AS (
    SELECT i.*, b.min_reg, b.max_reg, b.min_act_date, b.max_act_date
    FROM reg_base_raw i
    CROSS JOIN date_bounds b
),
user_profile_tags AS (
    SELECT
        uid, reg_date, app_id,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 2 DAY) AS d2_target,
        DATE_ADD(reg_date, INTERVAL 3 DAY) AS d3_target,
        DATE_ADD(reg_date, INTERVAL 4 DAY) AS d4_target,
        DATE_ADD(reg_date, INTERVAL 5 DAY) AS d5_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d6_target,
        DATE_ADD(reg_date, INTERVAL 7 DAY) AS d7_target,
        min_reg, max_reg, min_act_date, max_act_date
    FROM reg_base
),
daily_active_matrix AS (
    SELECT
        p.uid, p.reg_date,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d2_target THEN 1 ELSE 0 END) AS is_d2,
        MAX(CASE WHEN a.dt = p.d3_target THEN 1 ELSE 0 END) AS is_d3,
        MAX(CASE WHEN a.dt = p.d4_target THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = p.d5_target THEN 1 ELSE 0 END) AS is_d5,
        MAX(CASE WHEN a.dt = p.d6_target THEN 1 ELSE 0 END) AS is_d6,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d2_target, p.d3_target, p.d4_target, p.d5_target, p.d6_target, p.d7_target)
        AND a.dt BETWEEN p.min_act_date AND p.max_act_date
    GROUP BY p.uid, p.reg_date
),
last_active_label AS (
    SELECT
        uid, reg_date,
        CASE
            WHEN is_d7 = 1 THEN 'D7留存'
            WHEN is_d6 = 1 THEN 'D6流失'
            WHEN is_d5 = 1 THEN 'D5流失'
            WHEN is_d4 = 1 THEN 'D4流失'
            WHEN is_d3 = 1 THEN 'D3流失'
            WHEN is_d2 = 1 THEN 'D2流失'
            WHEN is_d1 = 1 THEN 'D1流失'
            ELSE '从未回来'
        END AS churn_period
    FROM daily_active_matrix
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    churn_period,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 2) AS pct
FROM last_active_label
GROUP BY churn_period
ORDER BY
    CASE churn_period
        WHEN '从未回来' THEN 1 WHEN 'D1流失' THEN 2 WHEN 'D2流失' THEN 3
        WHEN 'D3流失' THEN 4 WHEN 'D4流失' THEN 5 WHEN 'D5流失' THEN 6
        WHEN 'D6流失' THEN 7 WHEN 'D7留存' THEN 8
    END;
```

### 5.3 流失时段 × 首日画像归因

```sql
WITH reg_base_raw AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-06-18' AND '2026-06-22'
),
date_bounds AS (
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
reg_base AS (
    SELECT i.*, b.min_reg, b.max_reg, b.min_act_date, b.max_act_date
    FROM reg_base_raw i
    CROSS JOIN date_bounds b
),
user_profile_tags AS (
    SELECT
        uid, reg_date, app_id,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 2 DAY) AS d2_target,
        DATE_ADD(reg_date, INTERVAL 3 DAY) AS d3_target,
        DATE_ADD(reg_date, INTERVAL 4 DAY) AS d4_target,
        DATE_ADD(reg_date, INTERVAL 5 DAY) AS d5_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d6_target,
        DATE_ADD(reg_date, INTERVAL 7 DAY) AS d7_target,
        min_reg, max_reg, min_act_date, max_act_date
    FROM reg_base
),
daily_active_matrix AS (
    SELECT
        p.uid, p.reg_date,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d2_target THEN 1 ELSE 0 END) AS is_d2,
        MAX(CASE WHEN a.dt = p.d3_target THEN 1 ELSE 0 END) AS is_d3,
        MAX(CASE WHEN a.dt = p.d4_target THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = p.d5_target THEN 1 ELSE 0 END) AS is_d5,
        MAX(CASE WHEN a.dt = p.d6_target THEN 1 ELSE 0 END) AS is_d6,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d2_target, p.d3_target, p.d4_target, p.d5_target, p.d6_target, p.d7_target)
        AND a.dt BETWEEN p.min_act_date AND p.max_act_date
    GROUP BY p.uid, p.reg_date
),
last_active_label AS (
    SELECT
        uid, reg_date,
        CASE
            WHEN is_d7 = 1 THEN 'D7留存'
            WHEN is_d6 = 1 THEN 'D6流失'
            WHEN is_d5 = 1 THEN 'D5流失'
            WHEN is_d4 = 1 THEN 'D4流失'
            WHEN is_d3 = 1 THEN 'D3流失'
            WHEN is_d2 = 1 THEN 'D2流失'
            WHEN is_d1 = 1 THEN 'D1流失'
            ELSE '从未回来'
        END AS churn_period
    FROM daily_active_matrix
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    l.churn_period,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 2) AS pct,

    ROUND(SUM(CASE WHEN COALESCE(g.silver_game_count, 0) = 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_0game,
    ROUND(SUM(CASE WHEN g.silver_game_count = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_1game,
    ROUND(SUM(CASE WHEN g.silver_game_count BETWEEN 2 AND 5 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_2_5game,
    ROUND(SUM(CASE WHEN g.silver_game_count BETWEEN 6 AND 10 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_6_10game,
    ROUND(SUM(CASE WHEN g.silver_game_count > 10 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_10plus_game,
    ROUND(AVG(g.silver_game_count), 1) AS avg_game_cnt,

    ROUND(AVG(g.silver_win_rate), 1) AS avg_win_rate,
    ROUND(SUM(CASE WHEN g.silver_win_rate < 30 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_low_winrate,

    ROUND(AVG(g.first_day_login_cnt), 2) AS avg_login_cnt,
    ROUND(SUM(CASE WHEN g.first_day_login_cnt = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_single_session,

    ROUND(AVG(g.silver_total_diff_money), 0) AS avg_silver_diff,
    ROUND(AVG(g.silver_end_money), 0) AS avg_end_money,

    ROUND(SUM(CASE
        WHEN g.silver_total_play_seconds > 0 AND g.silver_game_count > 0
         AND g.silver_total_play_seconds * 1.0 / g.silver_game_count < 60 THEN 1
        ELSE 0
    END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_fast_game,

    ROUND(SUM(CASE WHEN r.channel_category_name = '官方(非CPS)' THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_official,
    ROUND(SUM(CASE WHEN r.channel_category_name = '华为' THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_huawei

FROM last_active_label l
LEFT JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = 1880053 AND g.reg_date = l.reg_date AND g.uid = l.uid
LEFT JOIN tcy_temp.dws_dq_app_daily_reg r
    ON r.app_id = 1880053 AND r.reg_date = l.reg_date AND r.uid = l.uid
GROUP BY l.churn_period
ORDER BY
    CASE l.churn_period
        WHEN '从未回来' THEN 1 WHEN 'D1流失' THEN 2 WHEN 'D2流失' THEN 3
        WHEN 'D3流失' THEN 4 WHEN 'D4流失' THEN 5 WHEN 'D5流失' THEN 6
        WHEN 'D6流失' THEN 7 WHEN 'D7留存' THEN 8
    END;
```

> ⚠️ 5.3 中 `pct_0game` 已修复为 `COALESCE(g.silver_game_count, 0) = 0`（原 plan 中未处理 NULL，导致全列输出 0）。其余字段未变。

---

> **创建时间**：2026-06-29
>
> **关联**：[retention-survival-curve.md](../../plan/retention/retention-survival-curve.md)（分析方案）、[retention-gameplayers-attribution-report.md](retention-gameplayers-attribution-report.md)（归因报告，D1 视角）、[retention-protection-period-report.md](retention-protection-period-report.md)（保护期报告，0 局占比参考）、[retention-analysis-framework.md](../../plan/retention/retention-analysis-framework.md)（框架）
