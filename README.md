# 医疗管理系统（HIS）· 第 12 组

> **《程序设计基础课程设计》（2025 级）· Hospital Information System**
>
> 基于 C 语言、全程链表实现的轻量级医院信息管理系统课程设计项目。

---

## 👥 小组成员

| 成员 | 占比 | 负责模块 | 核心交付物 |
|------|------|---------|-----------|
| **钟佳凌**（组长） | 20% | 公共底层、项目规范、初始化、统计报表、联调验收 | `global.h`、`main.c`、`initSystemData`、`StatisticModule` |
| **刘承庚** | 30% | 门诊主流程 + 住院收费主流程 | `RegisterPatient`、`SeeDoctor`、`ProcessBilling` |
| **谢欣材** | 25% | 药房管理 + 病房/床位管理 | `PharmacyManagement`、`ShowWardStatus` |
| **周溢程** | 25% | 数据持久化、综合查询、软删除、日志 | `LoadAllData`、`SaveAllData`、`SearchModule` |

---

## 📁 仓库目录结构

```
.
├── README.md                     # 本文件
├── .gitignore                    # 忽略编译产物 / IDE 临时文件
├── docs/                         # 项目文档
│   ├── Programming-Design-Requirements-2025.pdf   # 课程设计要求（学校发的）
│   ├── Group12-HIS-Assignment.docx                # 第12组自拟题签
│   ├── Naming-Convention.rtf                      # 命名规范参考
│   └── Integration-Checklist.md                   # 联调对齐清单（⭐必读）
├── data/                         # 原始测试数据（联调/答辩用）
│   ├── doctor.txt                100 条医生数据
│   ├── patient.txt               100 条患者数据
│   ├── medicine.txt              100 条药品数据
│   ├── record.txt                100 条诊疗记录
│   └── README.txt                数据字段说明
├── src/                          # 源代码（各成员按模块提交）
│   ├── liu-chenggeng/            刘承庚的本地实现（联调前独立版本）
│   ├── (zhong-jialing/)          ← 队长待建
│   ├── (xie-xincai/)             ← 谢欣材待建
│   └── (zhou-yicheng/)           ← 周溢程待建
└── reference/                    # 参考资料 / 早期版本留档
    └── team-leader-init/         队长的早期初始化代码（global.h 草稿 + main.cpp）
```

---

## 🛠 编译运行

### 刘承庚模块（独立测试版）

```bash
cd src/liu-chenggeng
# Windows / Mac / Linux 通用
gcc -Wall -o his main.c global.c register.c see_doctor.c billing.c

# 运行
./his              # Mac / Linux
his.exe            # Windows
```

### 联调版（待队长 `global.h` 定稿后）

届时由队长提供统一 `Makefile` 或 VS 项目文件。

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
- 记录修改不得直接覆盖 → **原记录红冲 + 新增正确记录**
- 删除采用软删除（`is_deleted=1`），不物理移除

### 文件容错
- 读取文件时发现坏行 → 跳过 + 记录错误日志 + 其余正确数据继续读取

### 金额存储
- **全部使用"分"作为最小单位**（`long long` 类型）
- 避免浮点精度丢失

---

## 🗂 数据结构约定

> 以队长 `reference/team-leader-init/global.h` 为最终权威版本。
> 联调前请阅读 [`docs/Integration-Checklist.md`](docs/Integration-Checklist.md) 对齐字段。

核心结构体：
- `Doctor` — 医生
- `Patient` — 患者
- `Medicine` — 药品
- `MedicalRecord` — 诊疗记录（type: 1挂号/2看诊/3检查/4开药/5住院）
- `WardBed` — 病房床位
- `Department` — 科室

---

## 🌿 协作分支策略

### 主要分支

| 分支 | 用途 |
|------|------|
| `main` | 稳定主分支，**禁止直接 push**，通过 Pull Request 合入 |
| `dev/zhong-jialing` | 队长开发分支 |
| `dev/liu-chenggeng` | 刘承庚开发分支 |
| `dev/xie-xincai` | 谢欣材开发分支 |
| `dev/zhou-yicheng` | 周溢程开发分支 |
| `integration` | 联调分支，联调时所有人合入这里 |

### 标准工作流

```bash
# 1. 克隆仓库（首次）
git clone https://github.com/<队长用户名>/Programming-Course-Design-Group12.git
cd Programming-Course-Design-Group12

# 2. 切到自己的分支
git checkout dev/liu-chenggeng   # 换成自己的名字

# 3. 开始写代码，随时提交
git add .
git commit -m "实现挂号限制校验"
git push origin dev/liu-chenggeng

# 4. 从 main 拉取最新（定期同步队长最新的 global.h）
git fetch origin
git merge origin/main

# 5. 功能完成后，在 GitHub 网页上发 Pull Request 合入 integration
```

---

## 🚨 重要约定（作弊警示）

题签明确写了：
> **题签雷同可直接认定为作弊。**

因此：
- ✅ 讨论思路、共享结构体定义、互相 review 代码 — **允许**
- ❌ 不允许把本仓库代码分享给其他组 / 同年级其他同学
- ❌ 不允许直接使用网上或其他组的代码片段（抄袭检测）

---

## 📅 进度节点

| 节点 | 任务 | 状态 |
|------|------|------|
| 第 1 次实验课 | 教师发布题目 | ✅ |
| 第 2~3 次实验课 | 组内分工、讨论题签、提交纸质题签 | ✅ |
| 中期 | 进度检查 | ⏳ |
| 最后 2 次实验课 | 代码检查 + 答辩 + 提交总结报告 | ⏳ |

---

## 🔗 联系方式

群聊：第 12 组 QQ / 微信群

**有问题优先在 GitHub Issue 里讨论，保留记录方便答辩时展示。**

---

*仓库由 Claude Code 辅助搭建（2026-04-23）*
