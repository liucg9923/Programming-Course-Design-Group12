# V3 修改方案

| 项 | 内容 |
| --- | --- |
| 修复对象 | `Hospital_Information_System(1).rar` 内 `HIS_4` 目录 |
| 修复版本目录 | `V3_fixed/` |
| 修复范围 | 6 项致命 Bug、3 项严重 Bug、4 项中等问题、1 项题签数据缺失 |
| 修复总行数 | 约 200 行（散布于 7 个源文件） |
| 编译验证 | macOS clang `g++ -Wall -Wextra` 零警告零错误 |
| 测试验证 | 7 个关键场景全部通过 |

> 所有修复在源代码中以 `/* 修复 Bug XX：... */` 注释标注，便于答辩时定位。

---

## 一、修改文件清单

| 文件 | 修改内容 |
| --- | --- |
| `pharmacy.cpp` | 添加 `#include <limits.h>` |
| `global.h` | 增加 `g_nextRegSeq` / `g_lastRegDate` 声明；新增 `REC_TYPE_RECHARGE` 类型 |
| `common.cpp` | 定义 `g_nextRegSeq` / `g_lastRegDate`；重写 `generateRegId`；`releaseAllData` 重置营业额；`generateRecordId/RegId` 改 `snprintf` |
| `register.cpp` | 挂号扣费同步累加营业额；建档增加重名提示；新增 `writeRechargeAudit` 充值审计函数 |
| `module_d_persist.cpp` | `split_ws` 去除空格分隔符；`saveSystemState` / `loadSystemState` 兼容新旧格式；`LogBadLine` 与 `SaveAllData` 字符串改回 UTF-8 中文 |
| `module_d_search.cpp` | `getRecordTypeName` 增加 `REC_TYPE_RECHARGE` 分支 |
| `main.cpp` | 启动时仅在加载日期无效时才同步 OS 时间；`printHospitalRevenueByRule` 增加挂号收入 |
| `ward.cpp` | `ChangeBedStatus` 限制状态转换 |
| `billing.cpp` | 添加 `module_d.h` 头文件；现金支付押金时增加二次确认 + 操作日志 |
| `inpatient.txt` | 新生成 30 条住院数据 |
| `ward_bed.txt` | 同步将 30 张床位标记为占用 |

---

## 二、详细修改

### 修复 Bug 01：pharmacy.cpp 编译错误

**修改内容**：在文件开头添加 `<limits.h>` 头文件。

**修改前**
```cpp
#include "pharmacy.h"

MedicineNode* FindMedicineByIdNode(int medicineId) {
```

**修改后**
```cpp
#include "pharmacy.h"
#include <limits.h>   /* 修复 Bug 01：补 INT_MAX 的头文件，跨平台编译 */

MedicineNode* FindMedicineByIdNode(int medicineId) {
```

**说明**：`pharmacy.cpp` 第 86 行使用 `INT_MAX`，但原版仅靠 MSVC 的间接包含才能编译。补上头文件后 macOS / Linux / 严格 GCC 均可编译。

---

### 修复 Bug 02：挂号费未计入医院营业额

**修改文件**：`register.cpp`

**修改前**
```cpp
patient->balanceCents -= regFee;

generateRegId(regId);
...
rec = (RecordNode*)malloc(sizeof(RecordNode));
if (!rec) {
    printf("内存分配失败。\n");
    patient->balanceCents += regFee;
    return;
}
```

**修改后**
```cpp
patient->balanceCents -= regFee;
g_hospitalRevenue += regFee;            /* 修复 Bug 02：挂号费计入营业额 */

generateRegId(regId);
...
rec = (RecordNode*)malloc(sizeof(RecordNode));
if (!rec) {
    printf("内存分配失败。\n");
    patient->balanceCents += regFee;
    g_hospitalRevenue -= regFee;        /* 同步回滚营业额 */
    return;
}
```

**实测验证**：挂号 1 次后 `system_state.txt` 营业额由 0 变为 2000（普通号 20 元）。

---

### 修复 Bug 03：统计报表漏算挂号费

**修改文件**：`main.cpp`

**修改前**
```cpp
static void printHospitalRevenueByRule(void) {
    Money exam = 0, med = 0, settle = 0;
    while (r) {
        if (r->type == REC_TYPE_EXAM) exam += r->totalCostCents;
        else if (r->type == REC_TYPE_PRESCRIBE) med += r->totalCostCents;
        else if (r->type == REC_TYPE_SETTLEMENT) settle += r->totalCostCents;
        r = r->next;
    }
    printf("检查收入："); printMoney(exam); printf("\n");
    printf("药费收入："); printMoney(med); printf("\n");
    printf("已结算住院费："); printMoney(settle); printf("\n");
    printf("医院总营业额："); printMoney(exam + med + settle); printf("\n");
}
```

**修改后**
```cpp
static void printHospitalRevenueByRule(void) {
    Money reg = 0, exam = 0, med = 0, settle = 0;
    while (r) {
        if (r->type == REC_TYPE_REGISTER) reg += r->totalCostCents;        /* 修复 Bug 03 */
        else if (r->type == REC_TYPE_EXAM) exam += r->totalCostCents;
        else if (r->type == REC_TYPE_PRESCRIBE) med += r->totalCostCents;
        else if (r->type == REC_TYPE_SETTLEMENT) settle += r->totalCostCents;
        r = r->next;
    }
    printf("挂号收入："); printMoney(reg); printf("\n");                    /* 修复 Bug 03 */
    printf("检查收入："); printMoney(exam); printf("\n");
    printf("药费收入："); printMoney(med); printf("\n");
    printf("已结算住院费："); printMoney(settle); printf("\n");
    printf("医院总营业额："); printMoney(reg + exam + med + settle); printf("\n");
}
```

**实测验证**：管理视角报表新增"挂号收入：170.00元"一行，总营业额含此项。

---

### 修复 Bug 04：挂号号跨进程重启后重复

**改动一**：`global.h` 增加全局变量声明。

```c
extern int   g_nextRegSeq;        /* 修复 Bug 04：跨进程持久化的当日挂号序号 */
extern char  g_lastRegDate[MAX_DATE_LEN]; /* g_nextRegSeq 对应的日期 */
```

**改动二**：`common.cpp` 定义并重写 `generateRegId`。

```c
int   g_nextRegSeq = 0;                                /* 修复 Bug 04 */
char  g_lastRegDate[MAX_DATE_LEN] = "";                /* 修复 Bug 04 */
...
void generateRegId(char* buf) {
    /* 修复 Bug 04：dailySeq 改为持久化的 g_nextRegSeq，并按日期重置 */
    if (strcmp(g_lastRegDate, g_currentDate) != 0) {
        g_nextRegSeq = 0;
        strncpy(g_lastRegDate, g_currentDate, MAX_DATE_LEN - 1);
        g_lastRegDate[MAX_DATE_LEN - 1] = '\0';
    }
    g_nextRegSeq++;
    snprintf(buf, MAX_REG_ID_LEN, "REG%s%02d", g_currentDate, g_nextRegSeq);
}
```

**改动三**：`module_d_persist.cpp` 增加新格式持久化。

```c
static void saveSystemState(void) {
    /* 修复 Bug 04：增加 g_nextRegSeq 和 g_lastRegDate 字段
     * 文件格式（共 11 字段，空格分隔）：
     *   nextPatientId nextRecordSeq nextWardId nextDeptId
     *   nextRegSeq lastRegDate
     *   currentDate currentHour currentMinute currentWeekday hospitalRevenue
     * lastRegDate 为空时输出 "-"。 */
    FILE* fp = openDataFile(FILE_SYSTEM, "w");
    const char* lastRegDateOut = (g_lastRegDate[0] != '\0') ? g_lastRegDate : "-";
    if (!fp) return;
    fprintf(fp, "%d %d %d %d %d %s %s %d %d %d %lld\n",
        g_nextPatientId, g_nextRecordSeq, g_nextWardId, g_nextDeptId,
        g_nextRegSeq, lastRegDateOut,
        g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday,
        (long long)g_hospitalRevenue);
    fclose(fp);
}
```

**改动四**：`loadSystemState` 兼容新旧两种格式。
- 优先尝试 11 字段格式，若失败回退 9 字段。
- 旧格式加载时 `g_nextRegSeq = 0`、`g_lastRegDate = ""`。

**实测验证**：
- 第 1 次启动挂号 → REG2026042301，保存退出。`system_state.txt` 第 5 字段 = 1。
- 第 2 次启动挂号 → REG2026042302（不重复），第 5 字段 = 2。

---

### 修复 Bug 05：含空格字段持久化丢失

**修改文件**：`module_d_persist.cpp`

**修改前**
```c
static int split_ws(char* line, char* fields[], int max_fields) {
    int count = 0;
    char* tok = strtok(line, " \t\r\n");
    while (tok && count < max_fields) {
        fields[count++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }
    return count;
}
```

**修改后**
```c
static int split_ws(char* line, char* fields[], int max_fields) {
    /* 修复 Bug 05：分隔符仅用 \t\r\n（不含空格），
     * 否则字段内的中文空格会被误拆，引发坏数据日志和数据丢失。 */
    int count = 0;
    char* tok = strtok(line, "\t\r\n");
    while (tok && count < max_fields) {
        fields[count++] = tok;
        tok = strtok(NULL, "\t\r\n");
    }
    return count;
}
```

**实测验证**：在 `record.txt` 末尾追加诊断为 `急性 感冒 三天` 的记录，启动后正常加载（修复前会被 `LogBadLine` 报为坏数据并丢弃）。

---

### 修复 Bug 06：日志含 GBK 乱码

**修改文件**：`module_d_persist.cpp`

涉及三处字符串：

1. `LogBadLine` 的格式串：

```c
/* 修复前 */
snprintf(msg, sizeof(msg), "[鍧忔暟鎹甝 鏂囦欢=%s 琛?=%d 鍐呭=%s",
    filename, line_no, raw_line);

/* 修复后 */
snprintf(msg, sizeof(msg), "[坏数据] 文件=%s 行=%d 内容=%s",
    filename, line_no, raw_line);
```

2. `SaveAllData` 的日志串：

```c
/* 修复前 */
"[淇濆瓨] doctors=%d patients=%d medicines=%d records=%d beds=%d"
/* 修复后 */
"[保存] doctors=%d patients=%d medicines=%d records=%d beds=%d"
```

3. `loadSystemState` 末尾的注释（一并改回中文）。

**实测验证**：
- `log_operation.txt` 输出 `[2026-04-27 21:22:18] [保存] doctors=100 patients=100 ...`
- `log_bad_data.txt` 输出 `[坏数据] 文件=doctor.txt 行=101 内容=BAD_LINE_NOT_TAB`

---

### 修复 Bug 07：床位状态修改限制

**修改文件**：`ward.cpp`

**核心改动**：`ChangeBedStatus` 增加状态转换约束。

```c
void ChangeBedStatus(void) {
    /* 修复 Bug 07：限制床位状态转换，避免绕过住院流程造成数据不一致。
     * 规则：
     *   - 占用 → 仅允许切换为 维护，且要求该床当前没有"在院"记录；
     *     否则提示先走出院结算。
     *   - 空闲 ↔ 维护：允许互转。
     *   - 任何状态 → 占用：不允许手动设置（必须通过住院办理流程）。
     */
    ...
    if (newStatus == BED_OCCUPIED) {
        printf("不允许手动将床位设为占用，请通过住院办理入口分配。\n");
        return;
    }
    if (b->bedStatus == BED_OCCUPIED) {
        InpatientNode* ip = g_inpatientHead;
        int hasActive = 0;
        while (ip) {
            if (!ip->isDeleted && ip->isAdmitted &&
                ip->wardId == b->wardId && ip->bedId == b->bedId) {
                hasActive = 1;
                break;
            }
            ip = ip->next;
        }
        if (hasActive) {
            printf("该床位仍有在院患者，请先在住院管理中完成出院结算。\n");
            return;
        }
        if (newStatus != BED_MAINTENANCE) {
            printf("占用床位仅允许切换为维护状态。\n");
            return;
        }
    }
    ...
}
```

**实测验证**：尝试将占用床位 101 改为空闲 → 系统提示 `该床位仍有在院患者，请先在住院管理中完成出院结算`。

---

### 修复 Bug 08：充值审计

**改动一**：`global.h` 新增 `REC_TYPE_RECHARGE = 7`。

**改动二**：`register.cpp` 新增 `writeRechargeAudit` 函数，并在 `rechargeForPatient` 末尾调用。

```c
/* 修复 Bug 08：充值时生成审计记录 + 操作日志 */
static void writeRechargeAudit(const PatientNode* patient, Money amount) {
    RecordNode* rec;
    char logMsg[256];
    if (!patient || amount <= 0) return;

    rec = (RecordNode*)malloc(sizeof(RecordNode));
    if (rec) {
        memset(rec, 0, sizeof(RecordNode));
        generateRecordId(rec->recordId);
        rec->patientId = patient->patientId;
        strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
        rec->docId = 0;
        strncpy(rec->docName, "前台", MAX_NAME_LEN - 1);
        rec->type = REC_TYPE_RECHARGE;
        strncpy(rec->diagnosis, "账户充值", MAX_DIAG_LEN - 1);
        rec->totalCostCents = amount;
        strncpy(rec->date, g_currentDate, MAX_DATE_LEN - 1);
        rec->hour = g_currentHour;
        rec->minute = g_currentMinute;
        appendRecord(rec);
    }

    snprintf(logMsg, sizeof(logMsg),
        "[充值] 患者ID=%d 姓名=%s 金额=%lld分 余额=%lld分",
        patient->patientId, patient->name,
        (long long)amount, (long long)patient->balanceCents);
    WriteLog(LOG_OP_FILE, logMsg);
}
```

**改动三**：`module_d_search.cpp` 让 `getRecordTypeName` 显示"充值"。

**实测验证**：
- `record.txt` 增加一条 `type=7 患者=2031 金额=20000`
- `log_operation.txt` 增加 `[充值] 患者ID=2031 姓名=林木 金额=20000分 余额=64000分`

---

### 修复 Bug 09：现金支付押金需二次确认

**修改文件**：`billing.cpp`

**改动一**：增加 `module_d.h` 头文件以便使用 `WriteLog` / `LOG_OP_FILE`。

**改动二**：在 `admitNewPatient` 押金支付环节增加二次确认 + 日志。

```c
{
    /* 修复 Bug 09：选 n（现金支付）时要求二次确认并写操作日志 */
    char yn[8];
    safeInputString("是否从患者账户余额中扣除押金？(y/n)：", yn, 8);
    if (yn[0] == 'y' || yn[0] == 'Y') {
        if (patient->balanceCents < deposit) {
            printf("账户余额不足。\n");
            return;
        }
        patient->balanceCents -= deposit;
    } else {
        char confirm[8];
        char logBuf[256];
        printf("现金支付押金 ");
        printMoney(deposit);
        printf("，请确认现金已收讫(y 确认 / 其他取消)：");
        safeInputString("", confirm, sizeof(confirm));
        if (confirm[0] != 'y' && confirm[0] != 'Y') {
            printf("现金未确认到位，住院办理取消。\n");
            return;
        }
        snprintf(logBuf, sizeof(logBuf),
            "[现金押金] 患者ID=%d 姓名=%s 金额=%lld分",
            patient->patientId, patient->name, (long long)deposit);
        WriteLog(LOG_OP_FILE, logBuf);
    }
}
```

---

### 修复 Bug 10：启动时不再覆盖加载日期

**修改文件**：`main.cpp`

```c
LoadAllData();
initDefaultDepartmentsAndBeds();
/* 修复 Bug 10：仅当持久化文件中没有有效日期时才用 OS 时间，
 * 否则保留用户上次"设置系统时间"的设置，便于演示模拟时间相关业务。 */
if (!isValidDateString(g_currentDate)) {
    syncSystemTimeFromOS();
    printf("未检测到有效系统时间，已自动同步操作系统时间：%s %02d:%02d 星期%d\n",
        g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday + 1);
} else {
    printf("已加载系统时间：%s %02d:%02d 星期%d\n",
        g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday + 1);
}
```

**实测验证**：`system_state.txt` 中日期为 20260423，启动后保留为 20260423，不再被 OS 时间 20260427 覆盖。

---

### 修复 Bug 16：releaseAllData 重置全局营业额

**修改文件**：`common.cpp`

```c
void releaseAllData(void) {
    ...
    g_departmentHead = NULL;

    /* 修复 Bug 16：重置营业额，避免重新加载时累积错误值 */
    g_hospitalRevenue = 0;
    g_nextRegSeq = 0;
    g_lastRegDate[0] = 0;
}
```

---

### 修复 Bug 17：sprintf 改 snprintf

**修改文件**：`common.cpp`

```c
void generateRecordId(char* buf) {
    /* 修复 Bug 17：sprintf -> snprintf，避免缓冲区溢出 */
    snprintf(buf, MAX_REC_ID_LEN, "REC%s%04d", g_currentDate, g_nextRecordSeq++);
}

void generateRegId(char* buf) {
    ...
    snprintf(buf, MAX_REG_ID_LEN, "REG%s%02d", g_currentDate, g_nextRegSeq);
}
```

---

### 修复 Bug 20：建档重名提示

**修改文件**：`register.cpp`

```c
/* 修复 Bug 20：建档时提示同名患者数量，并要求确认 */
dupCount = findPatientsByName(name, NULL, 0);
if (dupCount > 0) {
    char yn[8];
    printf("系统中已有 %d 名同名患者，仍要继续建档？(y/n)：", dupCount);
    safeInputString("", yn, sizeof(yn));
    if (yn[0] != 'y' && yn[0] != 'Y') {
        printf("建档取消。\n");
        return NULL;
    }
}
```

---

### 修复 D1：预生成 30 名住院患者

**新生成文件**：`inpatient.txt`（共 30 条）

```
2001	1001	1	101	150000	90000	20000	60000	20260420	20260422	9	5	3	1	0
2002	1001	1	102	160000	100000	20000	60000	20260420	20260422	10	6	3	1	0
...（共 30 行）
```

**字段说明**（按 `saveInpatients` 输出顺序）：

| 字段 | 含义 |
| --- | --- |
| patientId | 患者 ID（2001~2030） |
| docId | 主治医生 ID（与床位科室匹配） |
| wardId / bedId | 病房 / 床位 |
| depositTotalCents / depositBalanceCents | 累计押金 / 当前押金余额 |
| dailyFeeCents | 日住院费（普通 200 / ICU 500 / VIP 800 元） |
| totalChargedCents | 已扣总额 |
| admitDate | 入院日期 20260420 |
| lastChargeDate | 上次扣费日期 20260422 |
| admitHour | 入院小时 |
| expectedDays | 预计住院天数（5~10） |
| daysStayed | 已住天数（统一 3 天） |
| isAdmitted | 1（在院） |
| isDeleted | 0 |

**同步修改**：`ward_bed.txt` 中对应 30 张床位的状态 `0` → `1`，`patientId` 设为对应人员。

**对照题签**：题签第 3 节明确要求"至少 30 名住院患者"，原版本 `inpatient.txt` 为空，修复后满足该硬性要求。

---

## 三、未修复的问题（说明）

以下中等问题保留，理由如下：

| 编号 | 问题 | 保留原因 |
| --- | --- | --- |
| Bug 11 | 选医生流程的限制反馈滞后 | 仅 UX 体验，不影响正确性 |
| Bug 12 | 加载老格式记录日期被覆盖 | 当前数据均为新格式，无触发条件 |
| Bug 13 | 智能联想区分大小写 | 中文场景影响小 |
| Bug 14 | 医生繁忙度排行最大显示 15 位 | 演示需要可临时调整 |
| Bug 15 | money.txt 死代码 | 不影响运行；删除涉及 `module_d.h` 改动 |
| Bug 18 | macOS 下路径解析回退 | 当前 fallback 方案在演示场景下可用 |
| Bug 19 | 出院结算无现场充值 | 一致性问题，触发概率低 |
| 设计 D2~D11 | 各项设计建议 | 不在题签强制要求范围内 |

---

## 四、验证清单

### 编译

```
cd V3_fixed
g++ -Wall -Wextra -Wno-deprecated-declarations -o his *.cpp
```

macOS clang 编译结果：零警告零错误。

### 功能测试（实测通过）

| 编号 | 场景 | 预期 | 实测 |
| --- | --- | --- | --- |
| T01 | macOS 编译 | 成功 | 通过 |
| T02 | 启动加载 30 条住院数据 | 全部加载 | 通过 |
| T03 | 挂号扣费同步进营业额 | 营业额 = 挂号费总和 | 通过 |
| T04 | 重启后挂号号唯一 | 序号继续递增 | 通过（REG2026042302） |
| T05 | 含空格诊断字段持久化 | 不丢失 | 通过 |
| T06 | 日志中文显示正常 | 无乱码 | 通过 |
| T07 | 占用床位手动改空闲 | 拒绝 | 通过 |
| T08 | 充值生成审计记录 | record.txt 与 log 各一条 | 通过 |
| T09 | 管理视角报表显示挂号收入 | 出现"挂号收入"行 | 通过 |

---

## 五、对照修复前后

| 关键指标 | 修复前 | 修复后 |
| --- | --- | --- |
| macOS 编译 | 失败（INT_MAX） | 通过 |
| 挂号费进营业额 | 否 | 是 |
| 报表显示挂号收入 | 否 | 是 |
| 重启后挂号号唯一 | 否 | 是 |
| 含空格字段持久化 | 丢失 | 完整保留 |
| 日志可读性 | GBK 乱码 | 标准中文 |
| 床位状态可绕过住院流程 | 是 | 否 |
| 充值留痕 | 无 | record + 日志各一条 |
| 现金押金留痕 | 无 | 操作日志 + 二次确认 |
| 启动时模拟日期保留 | 否 | 是 |
| 30 名住院患者初始数据 | 缺失 | 完整 |

---

## 六、答辩演示建议

按以下顺序演示可覆盖大部分修复点：

1. **启动**：观察"已加载系统时间：20260423 12:42 星期4"（Bug 10）。
2. **菜单 5 → 1**：查看床位列表，30 张已占用（D1）。
3. **菜单 7 → 1**：管理视角报表，重点展示"挂号收入"行（Bug 03）。
4. **菜单 1 → 2 → 1 → 2031 → 1003**：挂号一次，确认显示余额扣减、营业额累加（Bug 02）。
5. **菜单 1 → 1 → 输入已存在姓名**：触发"系统中已有 N 名同名患者"提示（Bug 20）。
6. **保存退出 → 重启 → 挂号一次**：观察挂号号继续递增不重复（Bug 04）。
7. **菜单 5 → 3 → 输入占用床位 → 选 0**：拒绝（Bug 07）。
8. **菜单 1 → 3 → 充值**：查看 `record.txt` 与 `log_operation.txt` 留痕（Bug 08）。
9. **打开 `log_operation.txt`**：内容为标准中文（Bug 06）。

---

*修订日期：2026-04-27*
