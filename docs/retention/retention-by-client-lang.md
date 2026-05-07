# 分客户端层留存分析：稳定性问题专项

> 本文档聚焦**分客户端层**留存分析，对比 Cocos-Lua 与 Cocos-Creator 两个客户端版本的留存差异，重点识别稳定性问题信号。全局层分析见 [retention-global.md](retention-global.md)。
>
> **分析时间段**：2026-02-10 至 2026-04-22
> **留存口径**：登录留存（分母为当日注册APP端用户，分子为第N日登录用户）

---

## 目录

1. [客户端版本映射](#一客户端版本映射)
2. [版本留存对比](#二版本留存对比)
3. [稳定性信号识别](#三稳定性信号识别)
4. [版本特有分析](#四版本特有分析)

---

## 一、客户端版本映射

### 1.1 app_code 映射

```sql
CASE r.reg_app_code
    WHEN 'zgda' THEN 'Cocos-Lua'
    WHEN 'zgdx' THEN 'Cocos-Creator'
    ELSE '其他'
END AS client_lang
```

### 1.2 字段来源

| 表 | 字段 | 说明 |
| -- | ------ | ------ |
| `dws_dq_app_daily_reg` | `reg_app_code` | 用户**注册时**使用的客户端版本 |
| `dws_dq_daily_login` | `first_app_code` | 用户**当日首次登录**使用的客户端版本 |
| `dws_ddz_daily_game` | `app_code` | **对局时**使用的客户端版本 |

---

## 二、版本留存对比

### 2.1 各客户端版本留存率

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY client_lang;
```

**典型发现**：
- Cocos-Lua iOS 留存仅11.7%（客户端稳定性问题）
- Cocos-Creator iOS 留存27.3%（差异15.6pp）

### 2.2 版本 × 设备类型留存

```sql
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
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1, 2;
```

---

## 三、稳定性信号识别

> **核心问题**：客户端是否存在闪退/掉线/卡顿问题？

### 3.1 首日登录次数分组（检测闪退/掉线）

> 多次登录（≥3次）可能反映：闪退、掉线后重连、进程被杀等稳定性问题

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次（正常）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt <= 5 THEN 'C: 3-5次（可疑）'
        ELSE 'D: 5次以上（异常）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

**高危信号**：
- **0局/1局 + 多次登录（≥3）** → 强烈崩溃/掉线信号
- 若某版本「3-5次登录」占比显著偏高 → 稳定性问题

### 3.2 分版本 × 逃跑率（检测操作体验问题）

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.escape_count = 0 THEN 'A: 无逃跑'
        WHEN g.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN g.escape_count = 2 THEN 'C: 逃跑2次'
        ELSE 'D: 逃跑3+次'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date AND g.game_count > 0
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY client_lang, escape_group;
```

### 3.3 分版本 × 对局时长（检测渲染/网络卡顿）

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN g.timecost < 30   THEN 'A: <30s（异常短）'
        WHEN g.timecost < 120  THEN 'B: 1-2分钟'
        WHEN g.timecost < 240  THEN 'C: 2-4分钟'
        ELSE                       'D: 4分钟+'
    END AS timecost_group,
    COUNT(*) AS game_count,
    ROUND(AVG(g.timecost), 0) AS avg_seconds
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
    AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY client_lang, timecost_group;
```

---

## 四、版本特有分析

### 4.1 版本切换行为

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END AS reg_client_lang,
    CASE
        WHEN login1.first_app_code IS NULL THEN 'X: 首日无登录'
        WHEN login1.first_app_code = r.reg_app_code THEN 'A: 版本未切换'
        ELSE 'B: 版本已切换'
    END AS switch_status,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login login1
    ON login1.app_id = r.app_id AND login1.uid = r.uid AND login1.login_date = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY reg_client_lang, switch_status;
```

---

## 五、高危信号组合

| 组合特征 | 留存预期 | 优先级 |
| -------- | -------- | ------ |
| **0局/1局 + 多次登录（≥3）** | 低（崩溃/掉线） | P0 |
| **高逃跑率 + 异常时长** | 低（卡顿/操作问题） | P0 |
| **版本切换（注册后立即切版本）** | 中（对原版本不满意） | P1 |

---

## 六、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数/胜率/经济差异 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |
