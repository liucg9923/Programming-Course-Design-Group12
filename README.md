# 医疗管理系统（HIS）· 第 12 组

> **《程序设计基础课程设计》（2025 级）· Hospital Information System**
>
> 基于 C 语言、全程链表实现的轻量级医院信息管理系统课程设计项目。

---

## 👥 小组成员

| 成员 | 占比 | 负责模块 | 核心交付物 |
|------|------|---------|-----------|
| **钟佳凌**（组长） | 20% | 公共底层、项目规范、初始化、统计报表、联调验收 | `global.h`、`common.cpp`、`main.cpp`、`StatisticModule()` |
| **刘承庚** | 30% | 门诊主流程 + 住院收费主流程 | `RegisterPatient()`、`SeeDoctor()`、`ProcessBilling()` |
| **谢欣材** | 25% | 药房管理 + 病房/床位管理 | `PharmacyManagement()`、`ShowWardStatus()` |
| **周溢程** | 25% | 数据持久化、综合查询、软删除、日志 | `LoadAllData()`、`SaveAllData()`、`SearchModule()`、`DeleteRecord()` |

---

## 📁 仓库结构

```
.
├── README.md                  本文件
├── CONTRIBUTING.md            协作指南
├── docs/                      项目文档
│   ├── 总结报告.doc                   ⭐ 课程设计总结报告（最终提交版）
│   ├── 总结报告_评审意见.pdf          报告审稿意见
│   ├── Group12-HIS-Assignment.docx    第 12 组题签
│   ├── Programming-Design-Requirements-2025.pdf  课程要求
│   ├── Naming-Convention.rtf          命名规范
│   └── Integration-Checklist.md       联调对齐清单
├── reports/                   Bug 报告与迭代记录
│   ├── V6-Bug报告.pdf                 ⭐ 最新版 Bug 评审（13 项问题 + 修复方案）
│   ├── 总结报告_评审意见.pdf          报告审稿意见
│   ├── V5-CHANGELOG.md
│   ├── V3-修复CHANGELOG.md
│   └── V2-CHANGELOG.md
├── releases/                  版本归档
│   ├── v6-fixed/             ⭐ V6 修复版（推荐使用 / 答辩用）
│   ├── v6-original/          V6 队长原始版（含全部 Bug，仅作对照）
│   ├── v7-original/          V7 队长最新版（待审查）
│   ├── v0-array-version/     V0 队长最初的数组实现（已废弃，留作设计演进对照）
│   └── early-impl/           4-22 早期独立实现（链表 + 完整三模块）
└── data/                      原始测试数据
    ├── doctor.txt             100 名医生
    ├── patient.txt            100 名患者
    ├── medicine.txt           100 种药品
    ├── record.txt             100 条诊疗记录
    └── README.txt             字段说明
```

---

## 🚀 快速开始（推荐使用 `releases/v6-fixed`）

### Windows + Visual Studio

1. 双击 `releases/v6-fixed/HIS_4.vcxproj`
2. 项目属性 → C/C++ → 命令行 → 附加选项加 `/utf-8`
3. `Ctrl + F5` 运行

### Windows MinGW / Dev-C++ / macOS / Linux

```bash
cd releases/v6-fixed
g++ -Wall -Wno-deprecated-declarations -o his *.cpp
./his
```

---

## 📜 核心业务规则

### 挂号限制
- 全院每日最多 **500** 个号
- 每位医生每日最多 **20** 个号
- 每位患者每日最多 **5** 个号
- 同一患者同一天同一科室最多 **1** 个号

### 住院押金
- 押金必须是 **100 元的整数倍**
- 不得低于 **200 元 × 拟住院天数**
- 押金持续 ≥ 1000 元为安全状态，低于触发预警

### 出院计费
- **00:00 ~ 08:00** 办理出院：不收当天住院费
- **08:00 之后** 办理出院：收取当天住院费

### 药房与床位
- 单次开药最多 **100 盒**
- 床位状态：空闲 / 占用 / 维护中
- 病房类型：普通 / 重症 / VIP

### 修改与删除
- 记录修改：**原记录红冲 + 新增正确记录**（红冲后回滚财务和库存）
- 删除采用**软删除**（`is_deleted=1`）

### 金额存储
- 全部使用 **`long long` 分** 存储，避免浮点精度损失

---

## 🗂 V6 修复版 vs 原始版差异

V6 修复版相对队长 V6 原始版修复了 **13 项问题**（详见 `reports/V6-Bug报告.pdf`）：

| 严重度 | 数量 | 主要问题 |
|--------|------|---------|
| 严重 | 1 | 更正 VISIT 类型记录会污染输入流 |
| 中等 | 4 | 每日扣费/出院补扣冲突；跨日不补扣；充值后跳主菜单；押金不足直接拒绝 |
| 业务一致性 | 3 | 现金补缴无审计；30 名住院数据缺失；库存预警阈值过低 |
| 设计层面 | 5 | 按钮文案/macOS 路径/type 校验/菜单返回数字/软删除恢复 |

修复版同时：
- 预生成 30 条住院数据（满足题签要求）
- 所有子菜单"返回上级"统一为 `0`
- 内置 `screenshot.sh` / `screenshot.exp` 自动化截图脚本

---

## 📅 版本演进概览

| 版本 | 时间 | 关键变化 |
|------|------|---------|
| V0（数组版） | 4 月初 | 队长最早的数组实现（违反题签"全程链表"要求，已废弃） |
| 早期独立实现 | 4-22 | 刘承庚的链表版三模块原型 |
| V1 ~ V5 | 4-23 ~ 5-3 | 多轮迭代，逐步合并各成员模块、修复 Bug |
| V6 (原始) | 5-2 | 队长打包提交，含 13 项遗留 Bug |
| **V6 (修复)** | **5-8** | **修复全部 13 项 Bug，预生成住院数据，菜单统一** |
| V7 (原始) | 5-9 | 队长最新一版，待审查 |

---

## 🚨 学术诚信

题签明确写了：

> **题签雷同可直接认定为作弊。**

因此：
- ✅ 允许：组内讨论思路、共享结构体定义、互相 review 代码
- ❌ 不允许：把本仓库代码分享给其他组 / 同年级其他同学
- ❌ 不允许：直接使用网上或其他组的代码片段（抄袭检测）

**答辩前后建议把仓库改为 Private**：

```bash
gh repo edit liucg9923/Programming-Course-Design-Group12 --visibility private --accept-visibility-change-consequences
```

---

## 📞 联系

- 组长：钟佳凌
- 答辩日期：2026-05-17（拟）

---

*最后更新：2026-05-09*
