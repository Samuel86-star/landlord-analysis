# 斗地主 App 新增用户留存分析框架（按客户端开发语言）

> 本文档是 [`retention-global.md`](retention-global.md) 的补充延伸，专注于按**客户端开发语言**（Cocos Creator vs Cocos Lua）维度拆分后的新增用户留存分析。所有 SQL 基于 StarRocks 语法，表结构与主文档一致。
>
> **共享基础**：本文档共享全局文档的 **一~七章**（业务背景、数据基础、指标体系、分析方法论），此处不再重复。如需查阅指标定义或方法论，请参见全局文档。

---

## 目录

1. [分析背景](#一分析背景)
2. [客户端语言映射关系](#二客户端语言映射关系)
3. [分析框架与维度设计](#三分析框架与维度设计)
4. [基础数据准备](#四基础数据准备)
5. [分析SQL](#五分析sql)
6. [分析思路与预期产出](#六分析思路与预期产出)

---

## 一、分析背景

### 1.1 为什么需要按客户端语言分析

斗地主 App 目前存在两个并行的客户端版本，使用不同的技术栈开发：

| 客户端版本 | 技术栈 | app_code | 特点 |
|-----------|--------|----------|------|
| 老版本 | Cocos Lua | zgda | 成熟稳定，存量用户为主 |
| 新版本 | Cocos Creator | zgdx | 新技术栈，性能/交互可能有差异 |

两个版本在以下方面可能存在差异，进而影响留存：

- **性能体验**：渲染帧率、加载速度、内存占用、发热控制等
- **交互差异**：UI 布局、动画效果、操作响应速度等
- **功能覆盖**：新版可能有功能缺失或新增功能
- **用户分布**：不同渠道/机型可能默认分配不同版本

如果不区分客户端版本，整体留存数据可能被**版本混合效应**掩盖——例如某个渠道留存偏低，可能是该渠道分配了较多新版本用户，而新版本存在性能问题。

### 1.2 核心问题

1. 两个客户端版本的留存率是否存在显著差异？
2. 同一留存因子（如对局数、胜率、倍数）在不同客户端版本下的影响是否一致？
3. 客户端版本差异是否在特定渠道/设备类型上更显著？
4. 是否存在某个客户端版本特有的异常行为模式（如高逃跑率、异常对局时长）？

---

## 二、客户端语言映射关系

### 2.1 app_code 映射

```sql
CASE r.reg_app_code
    WHEN 'zgda' THEN 'Cocos-Lua'
    WHEN 'zgdx' THEN 'Cocos-Creator'
    ELSE '其他'
END AS client_lang
```

### 2.2 字段来源

| 表 | 字段 | 说明 |
|---|------|------|
| `dws_dq_app_daily_reg` | `reg_app_code` | 用户**注册时**使用的客户端版本 |
| `dws_dq_daily_login` | `first_app_code` | 用户**当日首次登录**使用的客户端版本 |
| `dws_ddz_daily_game` | `app_code` | **对局时**使用的客户端版本 |
| `dws_ddz_app_game_stat` | `app_code` | 聚合表中的客户端版本 |

> **注意**：同一用户可能在注册后切换客户端版本。分析以 `reg_app_code`（注册时版本）为主维度，辅以 `app_code`（对局时版本）交叉分析，可发现版本切换行为。

---

## 三、分析框架与维度设计

### 3.1 分析结构总览

```
├── 第一层：客户端版本级概览
│   ├── L-01: 各客户端版本新增用户留存率（整体对比）
│   ├── L-02: 客户端版本 × 渠道 留存对比
│   └── L-03: 客户端版本 × 设备类型 留存对比
│
├── 第二层：客户端版本内因子分析（分别在 Cocos-Lua / Cocos-Creator 内做）
│   ├── L-04: 分版本 × 对局数分组留存
│   ├── L-05: 分版本 × 胜率分组留存
│   ├── L-06: 分版本 × 倍数分组留存
│   ├── L-07: 分版本 × 经济变化分组留存
│   └── L-08: 分版本 × 高倍局经历留存
│
└── 第三层：客户端版本特有分析
    ├── L-09: 版本切换行为与留存（注册版本 vs 对局版本不一致）
    └── L-10: 分版本 × 对局时长分析（检测性能差异）
```

### 3.2 留存口径说明

与全局文档一致采用**新增用户留存**口径：

| 概念 | 定义 |
|------|------|
| 分母 | 当日注册的 APP 端用户数（不要求有对局） |
| 分子 | 第 N 日在 `dws_dq_daily_login` 中存在登录记录的用户数 |

留存率计算统一使用登录留存，不区分对局与否，与全局文档保持一致。

---

## 四、基础数据准备

> 分析时间段：**20260210 至 20260414**。
> 本文档不需要额外建宽表，直接基于全局文档的 DWS 表加 `reg_app_code`/`app_code` 维度即可。

**依赖的 DWS 表**（详见全局文档 8.1 节）：

| 表名 | 说明 | 关键字段 |
| --- | --- | --- |
| `dws_dq_app_daily_reg` | APP 端注册用户宽表 | `reg_app_code` |
| `dws_dq_daily_login` | 每日登录聚合表 | `first_app_code` |
| `dws_ddz_app_game_stat` | 用户每日游戏行为聚合表 | `app_code` |

---

## 五、分析SQL

> 以下 SQL 均在全局文档对应 SQL 的基础上增加 `reg_app_code` 分组维度。
> 仅列出有差异的核心 SQL，其余维度可按相同方式扩展。

### L-01: 各客户端版本新增用户留存率

```sql
-- 按客户端版本统计新增用户留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 2 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day2_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END,
    r.reg_date
ORDER BY client_lang, r.reg_date;
```

### L-02: 客户端版本 × 渠道 留存对比

```sql
-- 按客户端版本 × 渠道分类统计留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') THEN r.channel_category_name ELSE '其他' END AS channel_category_name,
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
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') THEN r.channel_category_name ELSE '其他' END,
    r.reg_date
ORDER BY client_lang, channel_category_name, r.reg_date;
```

### L-03: 客户端版本 × 设备类型 留存对比

```sql
-- 按客户端版本 × Android/iOS 统计留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
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
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS' WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android' ELSE '其他' END,
    r.reg_date
ORDER BY client_lang, platform, r.reg_date;
```

### L-04: 分版本 × 对局数分组留存

```sql
-- 按客户端版本 × 首日对局数分组分析留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
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
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE 
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'A: 0局'
        WHEN g.game_count = 1 THEN 'B: 1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
        ELSE 'E: 10局以上'
    END,
    r.reg_date
ORDER BY client_lang, game_count_group, r.reg_date;
```

### L-05: 分版本 × 胜率分组留存

```sql
-- 按客户端版本 × 首日胜率分组分析留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE 
        WHEN g.win_rate < 30 OR g.win_rate IS NULL THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
  AND g.game_count > 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE 
        WHEN g.win_rate < 30 OR g.win_rate IS NULL THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END,
    r.reg_date
ORDER BY client_lang, win_rate_group, r.reg_date;
```

### L-06: 分版本 × 倍数分组留存

```sql
-- 按客户端版本 × 首日平均倍数分组分析留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.avg_magnification <= 6 OR g.avg_magnification IS NULL THEN 'A: <=6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END AS multi_group,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN g.avg_magnification <= 6 OR g.avg_magnification IS NULL THEN 'A: <=6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END,
    r.reg_date
ORDER BY client_lang, multi_group, r.reg_date;
```

### L-07: 分版本 × 经济变化分组留存

```sql
-- 按客户端版本 × 首日经济变化分组分析留存
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN g.total_diff_money < 0 THEN 'C: 小亏 (-1万~0)'
        WHEN g.total_diff_money < 10000 THEN 'D: 小赚 (0~1万)'
        WHEN g.total_diff_money < 50000 THEN 'E: 大赚 (1万~5万)'
        ELSE 'F: 巨赚 (>5万)'
    END AS money_change_group,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN g.total_diff_money < 0 THEN 'C: 小亏 (-1万~0)'
        WHEN g.total_diff_money < 10000 THEN 'D: 小赚 (0~1万)'
        WHEN g.total_diff_money < 50000 THEN 'E: 大赚 (1万~5万)'
        ELSE 'F: 巨赚 (>5万)'
    END,
    r.reg_date
ORDER BY client_lang, money_change_group, r.reg_date;
```

### L-08: 分版本 × 对局时长分析

```sql
-- 按客户端版本 × 首日总对局时长分组分析留存
-- 对局时长是检测性能差异的关键指标：如果某版本的平均单局时长显著偏长，可能存在卡顿
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE 
        WHEN g.total_play_seconds IS NULL OR g.total_play_seconds = 0 THEN 'A: 无对局'
        WHEN g.total_play_seconds < 300 THEN 'B: <5分钟'
        WHEN g.total_play_seconds < 900 THEN 'C: 5-15分钟'
        WHEN g.total_play_seconds < 1800 THEN 'D: 15-30分钟'
        ELSE 'E: 30分钟+'
    END AS duration_group,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(COALESCE(g.avg_game_seconds, 0)), 0) AS avg_game_seconds,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day6_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260414
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE 
        WHEN g.total_play_seconds IS NULL OR g.total_play_seconds = 0 THEN 'A: 无对局'
        WHEN g.total_play_seconds < 300 THEN 'B: <5分钟'
        WHEN g.total_play_seconds < 900 THEN 'C: 5-15分钟'
        WHEN g.total_play_seconds < 1800 THEN 'D: 15-30分钟'
        ELSE 'E: 30分钟+'
    END,
    r.reg_date
ORDER BY client_lang, duration_group, r.reg_date;
```

**分析要点**：
- 对比两个版本的 `avg_game_seconds`（平均单局时长），如果 Cocos-Creator 版本显著偏长，可能存在性能问题
- 极短时长（<5分钟）在两个版本中的占比差异，可能反映客户端稳定性差异

---

## 六、分析思路与预期产出

### 6.1 分析路径

```
Step 1: 跑 L-01 ~ L-03 → 客户端版本级留存概览
  → 明确两个版本的基线留存、用户规模、渠道/设备分布
  → 核心问题：两个版本留存是否有显著差异？
  ↓
Step 2: 跑 L-04 ~ L-08 → 版本内因子拆解
  → 每个版本内部的对局数/胜率/倍数/经济/时长的留存规律
  → 与全局分析对比：全局发现的规律在分版本后是否仍成立？
  → 是否存在某个因子仅在某一版本下显著？
  ↓
Step 3: 综合结论 → 差异化策略
  → 如果版本差异显著：定位技术层面原因（性能/功能/交互）
  → 如果版本差异不显著：说明客户端不是留存瓶颈，聚焦游戏体验优化
```

### 6.2 预期核心产出

| 产出项 | 内容 |
|--------|------|
| 各版本留存基线 | Cocos-Lua vs Cocos-Creator 的次留、7留对比，含渠道和设备类型拆分 |
| 版本差异归因 | 留存差异是版本自身导致还是用户分布差异导致（通过控制渠道/设备后对比） |
| 版本特有问题 | 是否存在某版本的异常行为模式（如高逃跑率、异常对局时长、高 0 局用户占比） |
| 版本因子交互 | 同一因子（如对局数、胜率）在不同版本下对留存的影响是否一致 |
| 优化建议 | 如果某版本留存显著偏低，给出技术优化方向（性能、交互、功能对齐） |

### 6.3 对比分析要点

执行分析时重点关注以下**跨版本对比**：

| 对比维度 | 核心问题 |
|---------|---------|
| 0 局用户占比 | 某版本的 0 局用户占比是否更高？（可能是加载/启动问题导致用户未进入对局） |
| 平均单局时长 | 某版本的平均单局时长是否更长？（可能是渲染卡顿或网络问题） |
| 逃跑率 | 某版本的逃跑率是否更高？（可能是操作体验问题导致用户主动退出） |
| 首日对局数分布 | 两个版本的对局数分布是否一致？分布差异可能反映游戏流畅度差异 |
| 渠道差异放大 | 某个渠道在两个版本间的留存差异是否特别大？（可能是该渠道的设备适配问题） |

---

> **文档版本**：v2.0
> **创建日期**：2026-04-01
> **更新说明**：
> - v1.0：初始版本，完整复制全局文档框架加 app_code 维度
> - **v2.0**：**三层解耦重构** — 删除与全局文档重复的一~七章基础内容，改为纯增量文档；重新设计分析框架（L-01 ~ L-08）；SQL 从全局文档模式出发增加 `reg_app_code` 维度；新增分析思路与对比要点
>
> **关联文档**：
> - [`retention-global.md`](retention-global.md)（全局分析框架，含共享基础设定）
> - [`retention-by-mode.md`](retention-by-mode.md)（分玩法留存分析）
> - `dws/dws_dq_app_daily_reg.md`（APP 端注册用户宽表）
> - `dws/dws_ddz_daily_game.md`（对局战绩统一字段表）
>
> **使用说明**：
> 1. 先阅读全局文档了解基础设定和指标定义
> 2. 确认 DWS 表已构建
> 3. 执行 L-01 ~ L-03 获取版本级留存概览
> 4. 执行 L-04 ~ L-08 进行版本内因子拆解
> 5. 将查询结果填入对应区域，用于后续分析结论生成
