# 联调对齐清单（Integration Checklist）

> **本清单用于全组成员联调前对齐数据结构定义、命名规范、全局变量和常量。**
> **所有模块必须以队长 `reference/team-leader-init/global.h` 为基础扩展。**

---

## 🔴 紧急待确认项（致命级）

### ❓ 问题 1：数据结构是链表还是数组？

- **题签要求**（第 2 页第 2(4) 条）：
  > "用于代码检查的 C 语言源程序需要含有必要的代码注释，**全程链表实现**。"
- **队长目前 `global.h`** 用的是数组：
  ```c
  extern Doctor doctor_table[MAX_DOCTOR];
  extern Patient patient_table[MAX_PATIENT];
  ```
- **结论**：需要队长改成链表，否则答辩扣分。

### ❓ 问题 2：住院押金存哪里？

- 题签要求：每日 08:00 自动扣押金、<1000 元预警、出院结算。
- 但队长的 `Patient` 只有 `account_balance`，`MedicalRecord` 无押金字段。
- **待队长确认**：
  - [ ] 方案 A：在 `Patient` 新增 `Money deposit`
  - [ ] 方案 B：新增独立的 `Admission` 结构体
  - [ ] 方案 C：在 `MedicalRecord` 里加 `Money deposit_left` 字段

### ❓ 问题 3：需要单独的住院信息结构体吗？

- 题签要求维护：已住天数、预计出院日期、日住院费、押金余额、主治医生。
- 队长 `global.h` 无 `Admission` / `InpatientNode` 相关结构体。
- **建议**：新增 `Admission` 结构体关联患者和床位。

---

## 📋 数据结构最终定义（以队长版本为准）

### 1. Doctor（医生）

```c
typedef struct {
    int staff_id;                 // 医生工号，唯一
    char name[50];                // 姓名（允许重名）
    char level[30];               // 职称：主任医师/副主任医师/主治医师/住院医师
    char department[50];          // 所属科室
    int work_days[7];             // 下标 0~6 对应周一~周日，1=出诊 0=休息
    int current_load;             // 当前接诊量
} Doctor;
```

### 2. Patient（患者）

```c
typedef struct {
    int patient_id;               // 患者ID，唯一
    char name[50];                // 姓名（允许重名）
    int age;                      // 年龄
    char reg_id[30];              // 最近一次挂号号
    Money account_balance;        // 账户余额，单位：分
    // ⚠️ 可能需要新增：Money deposit;  (待队长确认)
} Patient;
```

### 3. Medicine（药品）

```c
typedef struct {
    int med_id;                   // 药品ID，唯一
    char official_name[100];      // 通用名
    char trade_name[100];         // 商品名
    char alias_name[100];         // 别名
    Money unit_price;             // 单价，单位：分
    int stock;                    // 库存
    char dept_related[50];        // 关联科室
} Medicine;
```

### 4. MedicalRecord（诊疗记录）

```c
typedef struct {
    char record_id[30];           // 记录唯一编号
    int p_id;                     // 患者ID
    int d_id;                     // 医生ID
    int type;                     // 1挂号 2看诊 3检查 4开药 5住院
    char diagnosis[100];          // 诊断结果
    Money check_fee;              // 检查费，单位：分
    int med_id;
    int med_count;
    int ward_id;
    int bed_id;
    int start_month, start_day, start_hour, start_min;
    int end_month, end_day, end_hour, end_min;
    int is_revoked;               // 撤销标记（红冲）
    int is_deleted;               // 软删除标记
    Money total_cost;             // 总费用，单位：分
} MedicalRecord;
```

### 5. WardBed（病房床位）

```c
typedef struct {
    int ward_id;
    int bed_id;
    int ward_type;                // 1普通 2重症 3VIP
    int status;                   // 0空闲 1占用 2维护
    int p_id;                     // 占用该床的患者ID
    char dept_name[50];           // 关联科室
} WardBed;
```

### 6. Department（科室）

```c
typedef struct {
    int dept_id;
    char dept_name[50];
} Department;
```

---

## 🔢 全局常量（队长版）

```c
#define MAX_PATIENT         300
#define MAX_DOCTOR           50
#define MAX_RECORDS        5000
#define MAX_MED_TYPES       100
#define MAX_WARD             20
#define MAX_BED_PER_WARD      5
#define MAX_DEPARTMENT       10
#define DEPOSIT_LIMIT    100000  // 1000元，单位分
```

### ⚠️ 建议补充的业务常量（刘承庚模块需要）

```c
#define DAILY_MAX_TICKETS         500   // 全院每日挂号上限
#define DAILY_MAX_PER_DOCTOR       20   // 每医生每日号上限
#define DAILY_MAX_PER_PATIENT       5   // 每患者每日号上限
#define DAILY_MAX_PER_DEPT          1   // 同患者同科室每日上限
#define DEPOSIT_UNIT            10000   // 押金单位：100元 = 10000分
#define MAX_PRESCRIPTION_QTY      100   // 单次开药最大盒数
#define REG_FEE_NORMAL           2000   // 普通挂号费 20元
#define REG_FEE_EXPERT           5000   // 专家挂号费 50元
#define DAILY_FEE_NORMAL        20000   // 普通病房日费 200元
#define DAILY_FEE_ICU           50000   // 重症病房日费 500元
#define DAILY_FEE_VIP           80000   // VIP病房日费  800元

// 记录类型
#define REC_TYPE_REGISTER      1
#define REC_TYPE_VISIT         2
#define REC_TYPE_EXAM          3
#define REC_TYPE_PRESCRIBE     4
#define REC_TYPE_INPATIENT     5

// 病房类型
#define WARD_NORMAL   1
#define WARD_ICU      2
#define WARD_VIP      3

// 床位状态
#define BED_FREE         0
#define BED_OCCUPIED     1
#define BED_MAINTENANCE  2
```

---

## 🌐 全局数据表（队长版 · 数组实现）

```c
extern Doctor        doctor_table[MAX_DOCTOR];
extern Patient       patient_table[MAX_PATIENT];
extern MedicalRecord record_table[MAX_RECORDS];
extern Medicine      medicine_table[MAX_MED_TYPES];
extern WardBed       bed_table[MAX_WARD * MAX_BED_PER_WARD];
extern Department    department_table[MAX_DEPARTMENT];

extern int doctor_count;
extern int patient_count;
extern int record_count;
extern int medicine_count;
extern int bed_count;
extern int department_count;
extern Money hospital_total_revenue;
```

> ⚠️ 如果改为链表，以上 table 变成头指针（如 `extern Doctor *doctor_head;`），各个 `*_count` 改为链表节点计数函数。

---

## 🧰 公共工具函数（队长 global.c 实现）

```c
int  GetSafeInt();
void GetSafeString(char* buffer, int max_len);
void SmartSuggestDemo();
```

**各模块需要队长补充的工具函数（建议）**：

```c
// 查找
Doctor*        FindDoctorById(int staff_id);
Patient*       FindPatientById(int patient_id);
Medicine*      FindMedicineById(int med_id);
WardBed*       FindBedById(int ward_id, int bed_id);
MedicalRecord* FindRecordById(const char* record_id);

// 按姓名查找（处理重名）
int FindPatientsByName(const char* name, Patient** results, int max_results);
int FindDoctorsByName(const char* name, Doctor** results, int max_results);

// 生成ID
void GenerateRecordId(char* buf);
void GenerateRegId(char* buf);

// 金额输出
void PrintMoney(Money cents);

// 当前时间查询（供每日扣费、挂号等使用）
int GetCurrentMonth();
int GetCurrentDay();
int GetCurrentHour();
int GetCurrentWeekday();
```

---

## ✍️ 命名规范（按队长版本）

- ✅ **下划线命名**：`patient_id`、`staff_id`、`current_load`
- ✅ 字段名小写 + 下划线
- ✅ 函数名 `PascalCase`：`GetSafeInt`、`RegisterPatient`
- ✅ 结构体名 `PascalCase`：`Doctor`、`MedicalRecord`
- ✅ 常量宏 `UPPER_CASE`：`MAX_DOCTOR`
- ✅ 金额统一用 `typedef long long Money`，单位"分"

---

## 🔗 三个对外函数（刘承庚模块 → 队长 main 调用）

请队长把以下三行加入 `global.h`：

```c
// ============ 刘承庚模块 ============
void RegisterPatient(void);   // 挂号：建档/老患者挂号/限制校验/生成挂号号
void SeeDoctor(void);         // 看诊：诊断/检查/开药/转住院/记录撤销
void ProcessBilling(void);    // 住院：办理/扣费/预警/出院结算
```

---

## ✅ 对齐确认表

联调当天，每人对照此表打勾：

- [ ] `global.h` 已改为链表实现（或保留数组但论证合理性）
- [ ] 押金存储方案已确定（A / B / C 之一）
- [ ] 住院信息结构体已决定（合入 MedicalRecord / 新增 Admission）
- [ ] 所有字段名与本清单一致
- [ ] 所有模块能独立编译通过
- [ ] 所有模块联调后能一键编译通过
- [ ] 测试数据能正常加载（100 条/类）

---

*最后更新：2026-04-23*
*若有修改请同步更新此文件并通知全组。*
