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
├── 第三层：客户端版本特有分析（性能/稳定性/行为）
│   ├── L-09: 版本切换行为与留存（注册版本 vs 对局版本不一致）
│   ├── L-10: 分版本 × 对局时长分析（检测渲染/网络卡顿）
│   ├── L-11: 分版本 × 首日登录次数分组（检测闪退/掉线）
│   └── L-12: 分版本 × 逃跑率分组（检测操作体验问题）
```

### 3.2 留存口径说明

**本文档采用登录留存口径**（与 `retention-by-mode.md` 的游戏留存不同）：

| 概念 | 定义 |
|------|------|
| 分母 | 当日注册的 APP 端用户数（**不要求有对局**，0 局用户也计入） |
| 分子 | 第 N 日在 `dws_dq_daily_login` 中存在登录记录的用户数 |

**为什么与 by-mode 不同**：客户端版本差异可能导致用户"能登录进来但因性能/稳定性问题玩不了对局"，这类用户是 by-mode（游戏留存口径）会过滤掉的分母，但恰恰是本文档的核心关注对象。因此 0 局用户必须保留在分母里。

**留存天命名约定**（与 by-mode 对齐，注册日计为 Day 1）：

| 字段名 | 偏移量 | 口径 |
|------|------|------|
| `day1_rate` | reg_date + 1 | 次留 |
| `day7_rate` | reg_date + 6 | 7 日留存 |

---

## 四、基础数据准备

> 分析时间段：**20260210 至 20260416**。
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

> **日期偏移约定**（下方所有 SQL 通用）：
> - 次留（`day1_rate`）= `l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY)`
> - 7留（`day7_rate`）= `l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY)`
> - LEFT JOIN 补充 `l.app_id = r.app_id` 防止多应用串数据

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
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.app_id = r.app_id
    AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
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
-- 按客户端版本 × 渠道分类统计留存（消除渠道分布偏差后看版本差异）
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') THEN r.channel_category_name ELSE '其他' END AS channel_category_name,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.app_id = r.app_id
    AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀') THEN r.channel_category_name ELSE '其他' END
ORDER BY client_lang, channel_category_name;
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
    COUNT(DISTINCT r.uid) AS reg_user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND l.app_id = r.app_id
    AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS' WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android' ELSE '其他' END
ORDER BY client_lang, platform;
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
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE 
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'A: 0局'
        WHEN g.game_count = 1 THEN 'B: 1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
        ELSE 'E: 10局以上'
    END
ORDER BY client_lang, game_count_group;
```

### L-05: 分版本 × 胜率分组留存

```sql
-- 按客户端版本 × 首日胜率分组分析留存（仅分析有对局的用户）
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE 
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
  AND g.game_count > 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE 
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END
ORDER BY client_lang, win_rate_group;
```

### L-06: 分版本 × 倍数分组留存

```sql
-- 按客户端版本 × 首日平均倍数分组分析留存
-- 0 局用户单独分组，避免污染最低倍数组
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.avg_magnification <= 6  THEN 'A: <=6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END AS multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.avg_magnification <= 6  THEN 'A: <=6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END
ORDER BY client_lang, multi_group;
```

### L-07: 分版本 × 经济变化分组留存

```sql
-- 按客户端版本 × 首日经济变化分组分析留存
-- 0 局用户单独分组，避免 NULL 误入 ELSE 分支
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN g.total_diff_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN g.total_diff_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN g.total_diff_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                  'F: 巨赚 (>5万)'
    END AS money_change_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN g.total_diff_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN g.total_diff_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN g.total_diff_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                  'F: 巨赚 (>5万)'
    END
ORDER BY client_lang, money_change_group;
```

### L-08: 分版本 × 高倍局经历留存

```sql
-- 按客户端版本 × 首日高倍局占比分组分析留存
-- 考察客户端是否存在"进了局之后倍数体验不一致"的情况（如 RNG 差异）
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.high_multi_games = 0 THEN 'A: 无高倍局'
        WHEN g.high_multi_games * 1.0 / g.game_count < 0.1 THEN 'B: 高倍<10%'
        WHEN g.high_multi_games * 1.0 / g.game_count < 0.3 THEN 'C: 高倍10-30%'
        WHEN g.high_multi_games * 1.0 / g.game_count < 0.5 THEN 'D: 高倍30-50%'
        ELSE 'E: 高倍>=50%'
    END AS high_multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.high_multi_games, 0)), 1) AS avg_high_multi_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.high_multi_games = 0 THEN 'A: 无高倍局'
        WHEN g.high_multi_games * 1.0 / g.game_count < 0.1 THEN 'B: 高倍<10%'
        WHEN g.high_multi_games * 1.0 / g.game_count < 0.3 THEN 'C: 高倍10-30%'
        WHEN g.high_multi_games * 1.0 / g.game_count < 0.5 THEN 'D: 高倍30-50%'
        ELSE 'E: 高倍>=50%'
    END
ORDER BY client_lang, high_multi_group;
```

### L-09: 版本切换行为与留存

```sql
-- 对比"注册版本 == 对局版本" vs "注册版本 != 对局版本"的用户留存差异
-- 版本切换可能反映：用户主动切版本（对新版不满意）或被动切版本（渠道强推/多版本并存）
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS reg_client_lang,
    CASE
        WHEN login1.first_app_code IS NULL THEN 'X: 首日无登录'
        WHEN login1.first_app_code = r.reg_app_code THEN 'A: 版本未切换'
        ELSE 'B: 版本已切换（到 ' || login1.first_app_code || '）'
    END AS switch_status,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
-- 取注册当日最后一次登录使用的客户端版本（可能与 reg_app_code 不同）
LEFT JOIN tcy_temp.dws_dq_daily_login login1
    ON r.uid = login1.uid AND r.app_id = login1.app_id
    AND login1.login_date = str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN login1.first_app_code IS NULL THEN 'X: 首日无登录'
        WHEN login1.first_app_code = r.reg_app_code THEN 'A: 版本未切换'
        ELSE 'B: 版本已切换（到 ' || login1.first_app_code || '）'
    END
ORDER BY reg_client_lang, switch_status;
```

### L-10: 分版本 × 对局时长分析（检测渲染/网络卡顿）

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
        WHEN g.total_play_seconds IS NULL OR g.total_play_seconds = 0 THEN '0: 无对局'
        WHEN g.total_play_seconds < 300  THEN 'A: <5分钟'
        WHEN g.total_play_seconds < 900  THEN 'B: 5-15分钟'
        WHEN g.total_play_seconds < 1800 THEN 'C: 15-30分钟'
        ELSE                                  'D: 30分钟+'
    END AS duration_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(g.avg_game_seconds), 0) AS avg_game_seconds,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE 
        WHEN g.total_play_seconds IS NULL OR g.total_play_seconds = 0 THEN '0: 无对局'
        WHEN g.total_play_seconds < 300  THEN 'A: <5分钟'
        WHEN g.total_play_seconds < 900  THEN 'B: 5-15分钟'
        WHEN g.total_play_seconds < 1800 THEN 'C: 15-30分钟'
        ELSE                                  'D: 30分钟+'
    END
ORDER BY client_lang, duration_group;
```

**分析要点**：
- 对比两个版本的 `avg_game_seconds`（平均单局时长），如果 Cocos-Creator 版本显著偏长，可能存在性能问题
- 极短时长（<5分钟）在两个版本中的占比差异，可能反映客户端稳定性差异

### L-11: 分版本 × 首日登录次数分组（检测闪退/掉线）

```sql
-- 按客户端版本 × 首日登录次数分组分析留存
-- 多次登录（≥3 次）可能反映：闪退、掉线后重连、进程被杀等稳定性问题
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 0 THEN 'X: 无登录（日志丢失）'
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次（正常）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt <= 5 THEN 'C: 3-5次（可疑）'
        ELSE 'D: 5次以上（异常）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN r.first_day_login_cnt = 0 THEN 'X: 无登录（日志丢失）'
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次（正常）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt <= 5 THEN 'C: 3-5次（可疑）'
        ELSE 'D: 5次以上（异常）'
    END
ORDER BY client_lang, login_cnt_group;
```

**分析要点**：
- 核心观察量：**"多次登录组"的占比差异**。如果 Cocos-Creator 中 3 次以上登录的用户占比显著高于 Lua，强烈暗示稳定性问题
- "多次登录组"的留存通常偏低——如果某版本的"多次登录组占比高且该组留存特别低"，两个信号叠加定位客户端问题

### L-12: 分版本 × 逃跑率分组（检测操作体验问题）

```sql
-- 按客户端版本 × 首日逃跑率分组分析留存
-- 逃跑率高可能原因：操作卡顿导致主动放弃、UI 响应慢、被误操作等
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.escape_count = 0 THEN 'A: 无逃跑'
        WHEN g.escape_count * 1.0 / g.game_count < 0.05 THEN 'B: 逃跑<5%'
        WHEN g.escape_count * 1.0 / g.game_count < 0.15 THEN 'C: 逃跑5-15%'
        WHEN g.escape_count * 1.0 / g.game_count < 0.30 THEN 'D: 逃跑15-30%'
        ELSE 'E: 逃跑>=30%'
    END AS escape_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(g.escape_count, 0)), 1) AS avg_escape_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = date_add(str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.app_id = r.app_id AND l.login_date > str_to_date(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260416
  AND r.is_login_log_missing = 0
GROUP BY
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.escape_count = 0 THEN 'A: 无逃跑'
        WHEN g.escape_count * 1.0 / g.game_count < 0.05 THEN 'B: 逃跑<5%'
        WHEN g.escape_count * 1.0 / g.game_count < 0.15 THEN 'C: 逃跑5-15%'
        WHEN g.escape_count * 1.0 / g.game_count < 0.30 THEN 'D: 逃跑15-30%'
        ELSE 'E: 逃跑>=30%'
    END
ORDER BY client_lang, escape_rate_group;
```

**分析要点**：
- 对比两版本的**高逃跑率组占比**：如果 Creator 版中"逃跑≥15%"的用户占比显著偏高，可能有操作体验问题
- 结合 L-11 登录次数：**多次登录 + 高逃跑率**的组合强烈指向"运行不稳定导致被动逃跑"

---

## 六、分析思路与预期产出

### 6.1 分析路径

```
Step 1: 跑 L-01 ~ L-03 → 客户端版本级留存概览
  → 明确两个版本的基线留存、用户规模、渠道/设备分布
  → 核心问题：两个版本留存是否有显著差异？
  → L-02/L-03 在控制渠道/设备后看版本差异，消除用户分布偏差
  ↓
Step 2: 跑 L-04 ~ L-08 → 版本内因子拆解（游戏行为层）
  → 每个版本内部的对局数/胜率/倍数/经济/高倍局的留存规律
  → 与全局分析对比：全局发现的规律在分版本后是否仍成立？
  → 是否存在某个因子仅在某一版本下显著？
  ↓
Step 3: 跑 L-09 ~ L-12 → 版本特有分析（性能/稳定性/行为）
  → L-09 版本切换：检测新版本是否被"主动抛弃"
  → L-10 对局时长：检测渲染/网络卡顿
  → L-11 登录次数：检测闪退/掉线稳定性问题（核心信号）
  → L-12 逃跑率：检测操作体验问题
  → L-11 + L-12 交叉：定位"多次登录 + 高逃跑"的客户端根因用户
  ↓
Step 4: 综合结论 → 差异化策略
  → 如果版本差异显著且 L-11/L-12 有异常：定位技术层面原因（闪退/卡顿/操作）
  → 如果版本差异不显著：说明客户端不是留存瓶颈，聚焦游戏体验优化
```

### 6.2 预期核心产出

| 产出项 | 内容 |
|--------|------|
| 各版本留存基线 | Cocos-Lua vs Cocos-Creator 的次留、7留对比，含渠道和设备类型拆分 |
| 版本差异归因 | 留存差异是版本自身导致还是用户分布差异导致（通过 L-02/L-03 控制变量对比） |
| 版本特有问题 | 稳定性（多次登录）、性能（对局时长）、操作体验（逃跑率）三类信号是否在某版本集中出现 |
| 版本因子交互 | 同一因子（如对局数、胜率）在不同版本下对留存的影响是否一致 |
| 版本切换洞察 | 是否存在大量用户"注册后立即切版本"的现象，该类用户留存如何 |
| 优化建议 | 如果某版本留存显著偏低，给出技术优化方向（性能、交互、功能对齐） |

### 6.3 对比分析要点

执行分析时重点关注以下**跨版本对比**：

| 对比维度 | 核心问题 | 对应 SQL |
|---------|---------|---------|
| 0 局用户占比 | 某版本的 0 局用户占比是否更高？（可能是加载/启动问题导致未进入对局） | L-04 A 组 |
| 平均单局时长 | 某版本的 `avg_game_seconds` 是否更长？（渲染卡顿/网络问题） | L-10 |
| 多次登录占比 | 某版本 3 次以上登录的用户占比是否显著偏高？（闪退/掉线信号） | L-11 |
| 逃跑率 | 某版本高逃跑组的占比是否显著偏高？（操作体验问题） | L-12 |
| 版本切换率 | 注册版本 ≠ 登录版本的用户占比及其留存——某版本被主动抛弃？ | L-09 |
| 首日对局数分布 | 两个版本的对局数分布是否一致？分布差异可能反映游戏流畅度差异 | L-04 |
| 渠道差异放大 | 某个渠道在两个版本间的留存差异是否特别大？（渠道设备适配问题） | L-02 |
| 平台差异放大 | iOS/Android 在两个版本间的表现是否不一致？（原生适配差异） | L-03 |

---

> **文档版本**：v3.0
> **创建日期**：2026-04-01
> **更新说明**：
> - v1.0：初始版本，完整复制全局文档框架加 app_code 维度
> - v2.0：**三层解耦重构** — 删除与全局文档重复的一~七章基础内容，改为纯增量文档；重新设计分析框架（L-01 ~ L-08）
> - **v3.0**：**修复 SQL 错误 + 扩展版本特有分析**
>   * 修复日期函数错误（`DATE_FORMAT` 返回 VARCHAR 无法 `DATE_ADD`）：全部改为 `date_add(str_to_date(...))` 模式
>   * 统一命名：`day6_rate` → `day7_rate`（与 by-mode 对齐 reg+6 = Day 7 的 Day1 计数约定）
>   * 修复 NULL 污染（L-06/L-07 将 0 局用户单独分组）
>   * 所有 JOIN 补充 `l.app_id = r.app_id`，多数 SQL 补充 `is_login_log_missing = 0` 过滤
>   * 明确留存口径与 by-mode 差异（本文档用登录留存以保留 0 局用户）
>   * 新增：L-08 高倍局经历、L-09 版本切换行为、L-11 首日登录次数、L-12 逃跑率
>   * 原 L-08 对局时长改编号为 L-10（与框架对齐）
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
> 3. 执行 L-01 ~ L-03 获取版本级留存概览（含控制变量对比）
> 4. 执行 L-04 ~ L-08 进行版本内因子拆解（游戏行为层）
> 5. 执行 L-09 ~ L-12 进行版本特有分析（性能/稳定性/行为层）
> 6. 将查询结果填入对应区域，用于后续分析结论生成
