# ops/py/ 目录使用说明

> 本目录是 StarRocks 数仓回填脚本集，按"调度器 + 单表脚本 + 工具"三层组织。日常运维只需要记住两个调度器命令。

## 目录速览

```text
py/
├── daily_backfill.py            # 调度器一：每日初始化（15 张表）
├── daily_retention.py           # 调度器二：每日刷留存（3 张表，35 天回扫）
├── backfill_runner.py           # 共享回填工具（按天 DELETE+INSERT，被各 batch 脚本复用）
├── sr_exec.py                   # StarRocks 客户端（CloudBeaver GraphQL，连库执行 SQL）
├── batch_insert_*.py            # 20 个单表回填脚本（18 个被调度器调用，也可单独跑）
├── check_data.sql               # 校验脚本：一次查 15 张表的当天行数
└── logs/                        # 调度器运行日志（.gitignore 排除，自动生成）
```

## 跨平台：macOS 用户

下方命令均为 Windows / PowerShell 写法。macOS 做两处适配即可直接用：

**1. 配置 `py` 命令（一次性）**：macOS 没有 Windows 的 Python Launcher，把下面这段加进 `~/.zshrc`——它会自动丢掉 `-3` 版本参数，并把（被引号包裹的）反斜杠路径转为正斜杠：

```zsh
py() {
    local args=()
    for arg in "$@"; do
        [[ "$arg" == -[0-9]* ]] && continue   # 丢弃 -3 / -2 等 Windows 版本选择参数
        args+=("${arg//\\//}")                 # Windows 路径分隔符 \ -> Unix /
    done
    uv run python "${args[@]}"
}
```

`source ~/.zshrc` 生效。项目根有 uv 管理的 `.venv/`，`uv run python` 会自动复用该环境。

**2. 路径用 `./` 不是 `.\`**：zsh 会把不加引号的 `.\` 里的反斜杠当转义符吃掉（`.\daily_backfill.py` → `.daily_backfill.py`，文件找不到），必须用正斜杠。`-3` 保留即可，函数会自动丢弃：

```bash
# 两条日常命令的 macOS 写法（.\  →  ./）
py -3 -u ./daily_backfill.py --start 20260715 --end 20260715
py -3 -u ./daily_retention.py
```

> 其余命令同理：把 `.\` 换成 `./` 即可直接复制粘贴。务必在 `ops/py/` 目录下执行（脚本靠同目录 `from sr_exec import ...` 导入）。

## 日常运维：两条命令

### 每天初始化数据：daily_backfill.py

```powershell
# 跑昨天数据（最常用）
py -3 -u .\daily_backfill.py --start 20260617 --end 20260617

# 跑区间（补数据）
py -3 -u .\daily_backfill.py --start 2026-06-01 --end 2026-06-08

# 只跑某层（补数据用，--layer 1/2/3）
py -3 -u .\daily_backfill.py --start 20260617 --end 20260617 --layer 2

# 先看执行计划不连库
py -3 -u .\daily_backfill.py --start 20260617 --end 20260617 --dry-run
```

按依赖三层串行跑 15 张表：

| 层 | 表（脚本前缀 batch_insert_） | 说明 |
| ---- | ---- | ---- |
| L1 无依赖 | daily_reg / daily_login / ddz_daily_game_raw / crazyddz_daily_game_raw / prop_log | 从 Hive/上游 SR 表搬 |
| L2 依赖 L1 | app_daily_reg / ddz_daily_game / crazyddz_daily_game / dq_silver_logs | 中间层 |
| L3 依赖 L2 | app_game_active / app_gamemode_active / app_silvergame_stat / app_scoregame_stat / allgame_stat / ddz_firstday_game | 聚合层 |

特性：
- **失败即停**：某张表失败立刻停下来报错，提示从哪一层用 `--layer N` 续跑（避免下游空跑）
- **0 行默认失败**：COUNT 校验为 0 时子进程以 code 1 退出，调度器随即停止，避免下游空跑
- **日志**：每天写 `logs/backfill_YYYY-MM-DD.log`，含每张表完整 stdout 便于回溯
- **幂等**：每张表是 `DELETE + INSERT`，重跑同一天不会重复

### 每天刷留存：daily_retention.py

```powershell
# 默认回扫昨天往前 35 天的 reg_date
py -3 -u .\daily_retention.py

# 指定 reg_date 区间（补历史数据）
py -3 -u .\daily_retention.py --start 20260301 --end 20260513

# 自定义回扫窗口
py -3 -u .\daily_retention.py --window 60

# 先看执行计划不连库
py -3 -u .\daily_retention.py --dry-run
```

按 reg_date 三层串行跑 retention 域 3 张表：

| 层 | 表（脚本前缀 batch_insert_） | 说明 |
| ---- | ---- | ---- |
| A1 | daily_allgame_stat | 把 allgame_stat 从 uid×dt×play_mode 降维到 uid×dt |
| A2 | retention_flag | 计算 d1/d3/d7/d14/d30 留存 flag（NULL=未到期/0=未留存/1=已留存）|
| B | firstday_game_stat | 注册首日游戏指标宽表（首日银子/积分/全玩法体验）|

为什么要回扫 35 天：留存 flag 按到期日逐步刷新（5/14 注册的 d30 要到 6/13 才能算准），中途漏跑后，只要回扫窗口覆盖到，下次跑就能补上。`game_active` / `daily_login` 是按天分区表不会过期，重算天然正确。

为什么默认 end=today-1：今天的注册用户 d1 还没到期、上游 daily_backfill 也可能没跑完，强行算只会得到错误结果。

## 完整链路：每天怎么用

凌晨数据就绪后（一般 02:00 左右），按顺序跑两条：

```powershell
# 1. 把昨天 15 张表数据回填到 SR
py -3 -u .\daily_backfill.py --start <昨天> --end <昨天>

# 2. 刷新最近 35 天 reg_date 的留存（包括昨天，因为昨天的 d1 今天才到期）
py -3 -u .\daily_retention.py
```

跑完用 check_data.sql 校验当天行数（改 sql 里日期为昨天）：

```powershell
py -3 -u .\sr_exec.py -f check_data.sql
```

## 单表脚本：何时单独跑

调度器底层调用的就是 18 个 `batch_insert_*.py`。日常用调度器即可，**单独跑场景**：

- **debug**：某张表数据有问题，单独跑这一张快速验证
- **补单表**：上游某天 raw 漏了，调度器整层重跑太重，单独跑那张表更快

所有 batch 脚本签名一致：

```powershell
py -3 -u .\batch_insert_<table>.py --start <date> --end <date> [--dry-run] [--app-id 1880053]
```

例：

```powershell
# 单独跑 dws_ddz_daily_game 6/17 一天
py -3 -u .\batch_insert_ddz_daily_game.py --start 20260617 --end 20260617

# dry-run 只看 SQL 不连库
py -3 -u .\batch_insert_ddz_daily_game.py --start 20260617 --end 20260617 --dry-run
```

## 工具脚本

### sr_exec.py：连库执行 SQL

```powershell
# 命令行直接传 SQL
py -3 -u .\sr_exec.py "SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_reg WHERE reg_date = '2026-06-17'"

# 从文件读 SQL（utf-8）
py -3 -u .\sr_exec.py -f my_query.sql
```

被所有 batch 脚本 import 用。包含三个修复：
- `trust_env=False` 强制忽略环境代理（团队任何机器都能直连内网，无需代理软件）
- INSERT/DELETE 完成后校验 `statusMessage`（拦"任务真没跑"，但拦不住 strict mode 静默回滚——后者必须 SELECT COUNT 复核）
- `-f` 读 SQL 文件强制 utf-8（避免 Windows GBK 解码失败）

#### sr_exec.py 配置

`sr_exec.py` 从环境变量读取连接配置。真实值必须放在 shell 环境或被 Git 忽略的本地文件中，不能提交到仓库。

| 环境变量 | 必填 | 说明 |
| --- | --- | --- |
| `CLOUDBEAVER_BASE_URL` | 是 | CloudBeaver GraphQL HTTPS 地址 |
| `CLOUDBEAVER_USERNAME` | 是 | CloudBeaver 登录账号 |
| `CLOUDBEAVER_PASSWORD_HASH` | 是 | CloudBeaver 密码摘要 |
| `CLOUDBEAVER_PROJECT_ID` | 是 | CloudBeaver 项目 ID |
| `CLOUDBEAVER_ALLOW_HTTP` | 否 | 仅限已记录的受限内网例外；设为 `1` 才允许 HTTP |

此前提交过的凭据必须从公司网络完成轮换。

### check_data.sql：行数校验

一次性查 15 张表指定日期的行数，按表名排序输出。改日期方式：

```powershell
# PowerShell 一行替换
(Get-Content check_data.sql) -replace '2026-06-17', '2026-06-18' | Set-Content check_data.sql
py -3 -u .\sr_exec.py -f check_data.sql
```

### backfill_runner.py：内部模板

被所有 batch 脚本 import。封装"按天循环 + DELETE + INSERT + COUNT 校验 + 失败即停"的标准流程。模板占位符：`{dt}`（YYYY-MM-DD）、`{dt_int}`（YYYYMMDD 整数）、`{dt_next_int}`（次日 YYYYMMDD，跨天扫描用）、`{app_id}`。

COUNT 校验为 0 行默认失败；只有确认允许空表的单表脚本才能显式传入 `allow_zero=True`。失败时子进程返回 code 1，调度器会停止后续任务。

新增表回填脚本时，复制现有 batch 脚本（如 `batch_insert_daily_reg.py`）改 SQL 即可，不用碰底层逻辑。

## 出问题怎么办

| 症状 | 排查 |
| ---- | ---- |
| 脚本报 OK 但目标表 0 行 | strict mode 静默回滚，看 [troubleshooting.md](../docs/lessons/troubleshooting.md) 第 1 节 |
| `python` 命令无输出 / exit 9009 | Windows Store 占位 launcher 抢了，用 `py -3` 不要用 `python` |
| 子进程报 `UnicodeDecodeError 'utf-8'` | sr_exec.py 已修复 utf-8 文件读取；如果还遇到，看是否是新写的脚本没加 `encoding="utf-8"` |
| Hive 上游表当天分区还没就绪 | 等几小时，或换前一天 |
| `Insert has filtered data in strict mode` | SR 真实报错（去 CloudBeaver 网页端跑同样 SQL 看），按 troubleshooting.md 流程查 `load_tracking_logs` 定位脏行字段 |

## 环境要求

- Windows + PowerShell + Python 3（用 `py -3` 调用，不要直接用 `python`）
- macOS：见上方「跨平台：macOS 用户」，配置 `py` 函数 + 路径改用 `./`
- 能从公司网络直连 CloudBeaver（内网，无需代理）
- 已安装 `requests` 和 `pandas`：`py -3 -m pip install requests pandas`（macOS 用 `uv pip install requests pandas`）
- DDL 操作（CREATE/ALTER/DROP）禁止走脚本，必须在 CloudBeaver 网页端手动执行（详见根目录 [CLAUDE.md](../CLAUDE.md)）

## 相关文档

- [ops/daily_data_ops.md](../ops/daily_data_ops.md) — 各表逐一的回填命令、依赖关系、常见问题
- [ops/troubleshooting.md](../docs/lessons/troubleshooting.md) — strict mode 静默回滚等疑难问题排查
- [starrocks/](../starrocks/) — 各表完整字段定义、建表 SQL、字段说明
