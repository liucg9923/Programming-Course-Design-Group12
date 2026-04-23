# 协作指南（Contributing Guide）

本仓库为第 12 组《程序设计基础课程设计》项目。**请全组成员认真阅读本指南**。

---

## 🚀 首次使用（新成员必读）

### 1. 配置 Git 身份

```bash
git config --global user.name "你的真名或昵称"
git config --global user.email "你的邮箱@xxx.com"
```

### 2. 克隆仓库

```bash
git clone https://github.com/<队长用户名>/Programming-Course-Design-Group12.git
cd Programming-Course-Design-Group12
```

### 3. 切换到自己的开发分支

```bash
# 每人在自己的分支上开发，不要直接改 main
git checkout -b dev/<自己的拼音名>
# 例：git checkout -b dev/liu-chenggeng
```

---

## 📝 日常开发流程

### 每天开始工作前

```bash
git fetch origin
git checkout main
git pull origin main          # 拉取最新的 main
git checkout dev/<自己>
git merge main                # 把 main 的更新合并到自己分支
```

### 写完代码要提交

```bash
git add src/<自己的目录>/     # 只 add 自己的文件，不要 git add .
git commit -m "简短描述改了什么"
git push origin dev/<自己>
```

### 功能完成想合入主分支

1. 在 GitHub 网页上点 **Pull Requests** → **New Pull Request**
2. base: `main`  ← compare: `dev/<自己>`
3. 描述本次改了什么，@ 队长 review
4. 队长批准后合入

---

## 🎯 提交信息规范（Commit Message）

**好的 commit 信息示例**：
```
feat(register): 实现挂号限制 4 项校验

- 全院500/天、医生20/天、患者5/天、同科室1/天
- 挂号号格式：REGyyyymmddNN
```

**不好的示例**（不要这样）：
```
x       ← 看不出改了什么
修改    ← 太笼统
1       ← 完全没意义
```

### 前缀约定

- `feat:` 新增功能
- `fix:` 修复 bug
- `refactor:` 代码重构，不改变功能
- `docs:` 只改文档
- `style:` 格式化代码
- `test:` 加测试
- `chore:` 杂项（改 .gitignore 等）

---

## 🌿 分支管理

| 分支 | 用途 | 谁可以 push |
|------|------|-----------|
| `main` | 稳定主分支 | **只能通过 Pull Request 合入** |
| `integration` | 联调分支 | 所有人 |
| `dev/zhong-jialing` | 队长开发 | 钟佳凌 |
| `dev/liu-chenggeng` | 刘承庚开发 | 刘承庚 |
| `dev/xie-xincai` | 谢欣材开发 | 谢欣材 |
| `dev/zhou-yicheng` | 周溢程开发 | 周溢程 |

### 禁止事项

- ❌ 不要直接 push 到 main（除非紧急，比如改 README）
- ❌ 不要 commit 编译产物（`.exe`、`.o`、`his` 等）
- ❌ 不要 commit `.vs/`、`x64/`、`Debug/` 这些 VS 临时文件
- ❌ 不要 `git push --force` 到公共分支（会覆盖别人的工作）

---

## 🧪 提交前自检清单

- [ ] 代码能在你本地正常编译（`gcc -Wall`）
- [ ] 核心功能经过手动测试
- [ ] 没有提交编译产物（检查 `git status`）
- [ ] 没有提交敏感信息（密码、token 等）
- [ ] commit 信息清晰说明了改了什么

---

## 📁 目录约定

```
src/
├── zhong-jialing/    队长专属，其他人不要改
├── liu-chenggeng/    刘承庚专属
├── xie-xincai/       谢欣材专属
├── zhou-yicheng/     周溢程专属
└── integrated/       联调合并后的最终版（队长维护）
```

**联调之前**：各人只在自己的目录里改。
**联调阶段**：由队长把各人代码合入 `src/integrated/` 并调整冲突。

---

## 🐛 发现队友代码有问题怎么办？

**不要**直接在别人分支上改代码。正确做法：

1. 去 GitHub 仓库主页 → **Issues** → **New Issue**
2. 标题写清楚问题，例如："register.c 行 123 挂号费没扣"
3. @ 对应的人，描述问题和复现步骤
4. 等对方修改后回复 issue

---

## 💬 团队沟通

- **代码层面的问题** → GitHub Issue（有记录）
- **紧急问题** → QQ/微信群
- **设计决策** → 线下开会，会后在 `docs/` 里写会议纪要

---

## 🆘 常见问题

### Q: git push 时提示 "rejected"？

别人先提交了，你需要先拉最新：
```bash
git pull origin dev/<自己>
# 如果有冲突，解决冲突后再提交
git push origin dev/<自己>
```

### Q: 不小心把错误的文件 commit 了，怎么撤销？

```bash
# 还没 push 的情况：
git reset --soft HEAD~1     # 撤销最近一次 commit，保留改动
# 想连改动都撤销：
git reset --hard HEAD~1     # ⚠️ 改动会丢失，慎用
```

### Q: 如何查看 main 上的最新 `global.h`？

```bash
git fetch origin
git show origin/main:src/zhong-jialing/global.h
```

---

## 📞 遇到困难找谁

- Git / GitHub 问题 → 搜索引擎 or 群里问
- 代码问题 → 对应模块负责人
- 业务规则疑问 → 查 `docs/Integration-Checklist.md` 或题签原文

---

*Happy Coding! 🎉*
