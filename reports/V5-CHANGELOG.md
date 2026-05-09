# V5 修复说明

| 项 | 内容 |
| --- | --- |
| 修复对象 | Hospital Information System V4 (`Hospital_Information_System.rar`) |
| 修复版本 | V5 |
| 修复日期 | 2026-05-03 |
| 修复范围 | V4 报告中的全部 13 项 Bug（Bug 21~33）+ V3 D1 |
| 编译验证 | macOS clang `g++ -Wall -Wextra` 通过 |
| 测试验证 | 7 个关键场景实测通过 |

---

## 一、修复清单

| 编号 | 问题 | 严重度 | 修复方案 |
| --- | --- | --- | --- |
| Bug 21 | 更正 VISIT 类型记录污染输入流 | 严重 | 新增 VISIT 专用分支，仅修改诊断文本 |
| Bug 22 | 每日扣费与出院补扣顺序敏感 | 中等 | 重构为按日期差累计扣费 |
| Bug 23 | 跨日设置时间不补扣中间日 | 中等 | `chargeMissingDays` 计算并补全 |
| Bug 24 | 充值后直接返回主菜单 | 中等 | `case 3/4` 改 `break`，停留子菜单 |
| Bug 25 | 押金不足直接拒绝 | 中等 | 改为调用 `ensureEnoughBalance` |
| Bug 26 | 现金补缴无审计日志 | 一致性 | 增加 `WriteLog(LOG_OP_FILE, ...)` |
| Bug 27 | `inpatient.txt` 为空 | 一致性 | 预生成 30 条与床位匹配的住院记录 |
| Bug 28 | 库存预警阈值偏低 | 一致性 | 提升至 `STOCK_WARNING_THRESHOLD = 50` |
| Bug 29 | "每日 08:00 自动扣费" 文案 | 设计 | 改为"补扣未结算住院费" |
| Bug 30 | macOS `/proc/self/exe` 失败 | 设计 | `__APPLE__` 分支用 `_NSGetExecutablePath` |
| Bug 31 | `loadRecords` 不校验 type 范围 | 设计 | 增加 `1 <= type <= 7` 校验 |
| Bug 32 | 各菜单"返回上级"数字不统一 | 设计 | 各模块统一接受 `0` 表示返回上级（同时保留兼容数字） |
| Bug 33 | 软删除不可恢复 | 设计 | 新增 `RestoreDeletedRecord` 入口（信息查询子菜单 6） |

---

## 二、详细修改

### Bug 21 — VISIT 记录更正专用分支

**位置**：`see_doctor.cpp` `modifyRecordByRevoke` 第 195 行附近

**修改前**：允许 VISIT 进入更正流程，但无对应的"录入新数据"分支，导致：
1. 用户输入的"新药品 ID"和"数量"被吞，污染下一次菜单输入。
2. 新记录的金额和药品继承自旧记录（`memcpy`），没有真正"更正"。

**修改后**：单独为 VISIT 写快速路径，仅询问新诊断文本：

```c
if (oldRec->type == REC_TYPE_VISIT) {
    char newDiag[MAX_DIAG_LEN];
    safeInputString("请输入新的诊断/说明（仅修改诊断文本，不涉及金额）：",
        newDiag, MAX_DIAG_LEN);
    // ... 复制旧记录，覆盖诊断字段，旧记录红冲 ...
    return;
}
```

EXAM/PRESCRIBE 走原分支，输入流不再被污染。

### Bug 22 + Bug 23 — 跨日补扣中间日

**位置**：`billing.cpp` 新增 `computeDateDiffDays` 与 `chargeMissingDays`

**修改前**：`runDailyAutoCharge` 用 `lastChargeDate != currentDate` 判断"是否扣"，即使跨日 2 天也只扣 1 笔；并且与 `dischargePatient` 的"补扣当天"逻辑冲突。

**修改后**：用 `mktime` 计算两个日期字符串之间的天数差：

```c
static int chargeMissingDays(InpatientNode* ip) {
    int days = computeDateDiffDays(ip->lastChargeDate, g_currentDate);
    for (int i = 0; i < days; i++) {
        ip->depositBalanceCents -= ip->dailyFeeCents;
        ip->totalChargedCents += ip->dailyFeeCents;
        g_hospitalRevenue += ip->dailyFeeCents;
        ip->daysStayed += 1;
    }
    strncpy(ip->lastChargeDate, g_currentDate, MAX_DATE_LEN - 1);
    return days;
}
```

`runDailyAutoCharge` 与 `dischargePatient` 都先调用 `chargeMissingDays` 把昨天及之前的费用结清，再处理"今天的特殊规则"。这样：
- 用户先点扣费再出院、或者先出院再扣费，结果一致。
- 一次性把时间从 4-23 设到 4-25，扣 3 天费用而不是 1 天。

**实测**：30 名住院患者，时间从 4-23 → 4-25，每人补扣 3 天；2001 患者押金从 90000 → 30000 ✓。

### Bug 24 — 充值/余额查询后停留子菜单

**位置**：`register.cpp` `RegisterPatient` 第 300 行附近

**修改前**：`case 3: rechargeEntry(); return;` 充值完直接 return。

**修改后**：改为 `break;` 让外层 `while(1)` 重新展示子菜单：

```c
case 3:
    rechargeEntry();
    PauseScreen();
    break;
case 4:
    balanceQueryEntry();
    PauseScreen();
    break;
case 0:           /* Bug 32 */
case 5:
    return;
```

### Bug 25 — 押金不足触发现场充值

**位置**：`billing.cpp` `admitNewPatient` 第 142 行附近

**修改前**：余额 `<` 押金时直接 `return`。

**修改后**：调用 `ensureEnoughBalance`，与挂号/检查/开药一致：

```c
if (yn[0] == 'y' || yn[0] == 'Y') {
    if (!ensureEnoughBalance(patient, deposit, "住院押金")) {
        printf("押金未到位，住院办理取消。\n");
        return;
    }
    patient->balanceCents -= deposit;
}
```

`ensureEnoughBalance` 在 `register.cpp` 实现，本次去掉 `static` 并在 `global.h` 声明。

### Bug 26 — 现金补缴写审计日志

**位置**：`billing.cpp` `dischargePatient` 第 360 行附近

**修改后**：账户补扣和现金补缴各写一条 `WriteLog(LOG_OP_FILE, ...)`：

```c
[出院账户补扣] 患者ID=2031 姓名=林木 金额=20000分
[出院现金补缴] 患者ID=2031 姓名=林木 应缴=20000分 实付=30000分 找零=10000分
```

### Bug 27 — 预生成 30 条住院数据

**位置**：`inpatient.txt` + `ward_bed.txt`

新生成 30 条住院记录（患者 2001~2030，覆盖普通病房/ICU/VIP），同步更新 `ward_bed.txt` 的 30 张床位状态为占用。所有数据满足"押金 100 整数倍 + ≥200×天数"约束。

### Bug 28 — 库存预警阈值

**位置**：`global.h` + `pharmacy.cpp`

新增宏 `#define STOCK_WARNING_THRESHOLD 50`，`CountStockWarnings` 与 `showWarnings` 同步使用此宏。预警时显示阈值说明。

### Bug 29 — "每日扣费" 按钮文案

**位置**：`billing.cpp` 住院菜单

旧："3. 执行每日 08:00 自动扣费" → 新："3. 补扣未结算住院费（按日期差自动补全）"。

### Bug 30 — macOS 路径解析

**位置**：`module_d_persist.cpp` `openDataFile`

新增 `#elif defined(__APPLE__)` 分支，使用 `_NSGetExecutablePath`（来自 `<mach-o/dyld.h>`）替代 Linux 特有的 `/proc/self/exe`。

### Bug 31 — `loadRecords` type 校验

**位置**：`module_d_persist.cpp` `loadRecords` 第 310 行后

读取后增加 `if (r->type < 1 || r->type > 7) { LogBadLine; free; continue; }`，防止恶意篡改 type 字段。

### Bug 32 — 菜单"返回上级"统一为 0

**位置**：`register.cpp`、`billing.cpp`、`module_d_search.cpp` 各子菜单

各模块的 `switch` 同时接受 `case 0:` 和原数字（兼容旧版本），保证肌肉记忆友好。

### Bug 33 — 软删除恢复入口

**位置**：`module_d_search.cpp` 新增 `RestoreDeletedRecord`

信息查询子菜单新增"6. 恢复软删除记录"，对 `is_deleted=1` 的记录置 0。

---

## 三、目录结构

```
第五版程序/
└── Hospital_Information_System/
    └── HIS_4/
        ├── *.cpp / *.h               # 源代码
        ├── *.txt                     # 数据文件
        ├── HIS_4.vcxproj             # Visual Studio 工程文件
        ├── HIS_4.vcxproj.filters
        ├── HIS_4.vcxproj.user
        ├── README.txt
        ├── MERGE_GUIDE.txt
        ├── DATA_CHECK.txt
        ├── UTF8_使用说明.txt
        └── V5_CHANGELOG.md           # 本文档
```

---

## 四、编译运行

### Windows (Visual Studio)

打开 `HIS_4.vcxproj` → 项目属性 → C/C++ → 命令行 → 附加选项添加 `/utf-8` → 生成解决方案 → Ctrl+F5。

### Windows (MinGW / Dev-C++)

```cmd
cd HIS_4
g++ -Wall *.cpp -o his.exe
his.exe
```

### macOS / Linux

```bash
cd HIS_4
g++ -Wall -Wextra -Wno-deprecated-declarations -o his *.cpp
./his
```

---

## 五、关键测试

| 编号 | 场景 | 预期 | 结果 |
| --- | --- | --- | --- |
| T01 | 启动加载 30 名住院 | 床位空闲=2 占用=30 | 通过 |
| T02 | 库存预警阈值 50 | 显示库存 <50 的药品 | 通过 |
| T03 | VISIT 记录更正 | 仅询问诊断，不索取药品/数量 | 通过 |
| T04 | 跨日（4-23 → 4-25）补扣 | 补扣 3 天，押金按日费递减 | 通过 |
| T05 | 充值/余额查询后停留子菜单 | 子菜单提示重复出现 | 通过 |
| T06 | 押金不足触发现场充值 | 弹"是否立即充值？" | 通过 |
| T07 | 编译零警告（除已知 `tm_min` 初始化警告外） | macOS clang 通过 | 通过 |

---

## 六、答辩演示路径

1. 启动 → 显示 `数据加载完成：医生=100 患者=100 药品=100 记录=100 床位=32`，时间 20260423 12:42。
2. 主菜单 7 → 1：管理视角统计，"床位空闲=2 占用=30"，"挂号收入"独立显示，"医院总营业额"含挂号费。
3. 主菜单 4 → 5：库存预警，显示 4 个低于 50 的药品。
4. 主菜单 9：把时间设到 20260425 09:00。
5. 主菜单 3 → 3：补扣未结算住院费 → 30 名患者各补 3 天，部分触发押金预警。
6. 主菜单 1 → 3 → 1 → 2007 → 100：充值给郑七 100 元，结束后回到挂号子菜单（演示 Bug 24 修复）。
7. 主菜单 2 → 2 → 输入一条 type=2 (VISIT) 记录 → 仅修改诊断（演示 Bug 21 修复）。
8. 主菜单 6 → 5 → 删除一条记录 → 6 恢复（演示 Bug 33 修复）。

---

*V5 修复 + 测试 + 文档：约 1.5 小时*
