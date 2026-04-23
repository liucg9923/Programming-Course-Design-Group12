# 刘承庚模块（门诊 + 住院主流程）

**负责人**：刘承庚（占项目 30%）

## 对外函数

本模块严格按题签规定，**仅暴露 3 个对外函数**：

| 函数 | 功能 |
|------|------|
| `RegisterPatient()` | 挂号：新患者建档、老患者挂号、挂号限制校验、生成挂号号 |
| `SeeDoctor()` | 看诊：录入诊断、开检查单、开处方、转住院、记录撤销（红冲） |
| `ProcessBilling()` | 住院：办理入院、每日自动扣费、押金预警、出院结算 |

其余所有辅助函数均为 `static` 私有函数，不对外暴露。

## 文件构成

```
├── outpatient_inpatient.h   本地占位头文件（联调时替换为队长的 global.h）
├── global.c                 全局变量定义 + 通用工具函数
├── register.c               RegisterPatient 实现
├── see_doctor.c             SeeDoctor 实现
├── billing.c                ProcessBilling 实现
└── main.c                   临时测试主程序（含演示数据）
```

## 编译运行

```bash
# Mac / Linux
gcc -Wall -o his main.c global.c register.c see_doctor.c billing.c
./his

# Windows (MinGW / Dev-C++)
gcc -Wall -o his.exe main.c global.c register.c see_doctor.c billing.c
his.exe
```

## 业务规则实现状态

| 规则 | 状态 |
|------|------|
| 全院日挂号 ≤ 500 | ✅ |
| 每医生日挂号 ≤ 20 | ✅ |
| 每患者日挂号 ≤ 5 | ✅ |
| 同患者同科室同天 ≤ 1 | ✅ |
| 押金 100 元整数倍 | ✅ |
| 押金 ≥ 200×天数 | ✅ |
| 押金 <1000 元预警 | ✅ |
| 出院 08:00 前后差异计费 | ✅ |
| 开药 ≤ 100 盒 | ✅ |
| 库存校验 | ✅ |
| 记录红冲机制 | ✅ |
| 撤销后退款退库存 | ✅ |

## 联调说明

当前 `outpatient_inpatient.h` 是**本地占位版**，基于尚未定稿的 struct 定义。
**联调时需要做的事**：

1. 把本目录所有 `.c` 文件里的 `#include "outpatient_inpatient.h"` 改成 `#include "global.h"`
2. 结构体名替换：
   - `DoctorNode` → `Doctor`
   - `PatientNode` → `Patient`
   - `MedicineNode` → `Medicine`
   - `RecordNode` → `MedicalRecord`
   - `BedNode` → `WardBed`
3. 字段名替换：
   - `docId` → `staff_id`
   - `patientId` → `patient_id`
   - `balanceCents` → `account_balance`
   - `schedule[]` → `work_days[]`
   - `currentLoad` → `current_load`
   - `isRedInk` → `is_revoked`
   - `lastRegId` → `reg_id`
   - （其余详见 `docs/Integration-Checklist.md`）
4. 金额类型 `int` → `Money`（`long long`），格式串 `%d` → `%lld`
5. 链表遍历 `while(p){...p=p->next;}` → 数组遍历 `for(i=0;i<count;i++){...}`
   **（除非队长改用链表实现）**
6. 删除 `main.c` 中的演示数据，由队长的 `initSystemData` 提供

详见上级 `docs/Integration-Checklist.md`。
