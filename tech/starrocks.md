StarRocks 读/写放大排查 SQL 手册
本手册用于监控和分析 StarRocks 表的健康状况，重点排查存储和查询效率问题。

一、 读放大 (Read Amplification) 排查
现象：SQL 运行缓慢、磁盘 IO 负载高。本质是系统扫描了远超实际需要的数据量。

1. 实时扫描行数对比 (最推荐)
在 SQL 前加上 EXPLAIN ANALYZE 运行，查看底层的 OlapScanNode 详情。

SQL
EXPLAIN ANALYZE 
SELECT * FROM your_table WHERE login_date = '2026-02-11';
核心指标：

RowsRead: 磁盘实际读取的行数。

RowsReturned: 过滤后返回给计算层的行数。

判定标准：如果 RowsRead 是 RowsReturned 的 10 倍以上，说明存在严重读放大。

2. 检查分区裁剪 (Partition Pruning)
检查查询是否成功跳过了无关的日期分区。

SQL
EXPLAIN SELECT * FROM your_table WHERE login_date = '2026-02-11';
判定标准：查看输出中的 partitions 字段。

优秀：partitions=1/400（只读了 1 天）。

读放大：partitions=400/400（触发了全表扫描）。

3. 检查本地关联 (Colocate Join)
确认 JOIN 查询是否产生了跨节点数据 Shuffle。

SQL
EXPLAIN SELECT * FROM table_a a JOIN table_b b ON a.uid = b.uid;
判定标准：搜索结果中是否存在 Colocate: true。

如果为 false 且伴随大量 EXCHANGE 节点，说明存在网络读放大。

二、 写放大 (Write Amplification) 排查
现象：磁盘占用增长过快、后台合并（Compaction）任务堆积。本质是写入过于频繁导致的小文件版本堆积。

1. 检查版本号 (Visible Version)
Version 是衡量写放大最核心的指标。

SQL
-- 查看各分区的版本汇总
SHOW PARTITIONS FROM your_table;
核心指标：VisibleVersion

判定标准：

1 ~ 50：极度健康（批量写入）。

50 ~ 500：正常范围，但需关注。

1000+：严重写放大。说明写入太碎（如每秒 INSERT 一次），会导致查询显著变慢并消耗大量 IO 进行后台合并。

2. 检查 Tablet 物理细节
查看底层存储单元的版本和合并开销。

SQL
-- 查看 Tablet 列表（可以按 DataVersion 排序）
SHOW TABLETS FROM your_table;
核心指标：DataVersion、DataSize、RowCount。

判定标准：如果 DataVersion 很高而 RowCount 很小，说明该 Tablet 经历了频繁的微小改动。

3. 监控后端合并压力
查看各节点（BE）当前的合并任务状态。

SQL
SHOW PROC '/backends';
核心指标：CompactionStatus

判定标准：如果该字段显示的 Score 持续处于高位（例如 > 100），说明写放大导致的合并任务已经堆积，会拖慢整机性能。

三、 💡 性能调优锦囊
1. 消除读放大的设计准则
分区 (Partition)：必须对时间字段（如 login_date）做 Range 分区。

分桶 (Bucket)：关联频繁的表必须使用相同分桶键 (uid) 和 相同分桶数 (Buckets)。

排序键 (Key)：将 WHERE 条件中最常用的字段放在 DUPLICATE KEY 的前三位。

2. 消除写放大的写入准则
改“微批”为“大批”：StarRocks 极其讨厌高频单条 INSERT。建议每批次写入不低于 10,000 行，或者每 1-5 分钟 导一次数据。

使用 Stream Load：对于实时流，优先使用 Stream Load 接口替代 INSERT INTO ... VALUES。