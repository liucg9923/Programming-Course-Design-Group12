/*************************************************************
 * register.c  —  挂号模块（刘承庚）
 *
 * 对外函数：RegisterPatient()
 * 其余辅助均为 static 私有函数。
 *************************************************************/
#include "outpatient_inpatient.h"

/* -------------------- 内部辅助函数 -------------------- */

static PatientNode *createNewPatient(void) {
    char name[MAX_NAME_LEN];
    int  age;

    safeInputString("请输入患者姓名: ", name, MAX_NAME_LEN);
    if (strlen(name) == 0) { printf("[错误] 姓名不能为空！\n"); return NULL; }

    age = safeInputInt("请输入患者年龄: ");
    if (age <= 0 || age > 150) { printf("[错误] 年龄不合法（1~150）！\n"); return NULL; }

    PatientNode *newP = (PatientNode *)malloc(sizeof(PatientNode));
    if (!newP) { printf("[错误] 内存分配失败！\n"); return NULL; }
    memset(newP, 0, sizeof(PatientNode));
    newP->patientId = g_nextPatientId++;
    strncpy(newP->name, name, MAX_NAME_LEN - 1);
    newP->age = age;
    newP->balanceCents = 0;
    newP->isDeleted = 0;
    newP->next = NULL;

    if (!g_patientHead) g_patientHead = newP;
    else {
        PatientNode *tail = g_patientHead;
        while (tail->next) tail = tail->next;
        tail->next = newP;
    }

    printf("========================================\n");
    printf("  新患者建档成功！\n");
    printf("  患者ID: %d\n", newP->patientId);
    printf("  姓名:   %s\n", newP->name);
    printf("  年龄:   %d\n", newP->age);
    printf("========================================\n");
    return newP;
}

static PatientNode *findExistingPatient(void) {
    printf("\n查找患者方式：\n  1. 按ID  2. 按姓名\n");
    int choice = safeInputInt("请选择 (1/2): ");

    if (choice == 1) {
        int id = safeInputInt("请输入患者ID: ");
        if (id < 0) return NULL;
        PatientNode *p = findPatientById(id);
        if (!p) { printf("[提示] 未找到ID %d 的患者。\n", id); return NULL; }
        printf("  找到: [%d] %s, %d岁, 余额", p->patientId, p->name, p->age);
        printMoney(p->balanceCents); printf("\n");
        return p;
    } else if (choice == 2) {
        char name[MAX_NAME_LEN];
        safeInputString("请输入患者姓名: ", name, MAX_NAME_LEN);
        PatientNode *results[20];
        int count = findPatientsByName(name, results, 20);

        if (count == 0) { printf("[提示] 未找到 \"%s\"。\n", name); return NULL; }
        if (count == 1) {
            printf("  找到: [%d] %s, %d岁\n", results[0]->patientId, results[0]->name, results[0]->age);
            return results[0];
        }
        printf("\n  发现 %d 位同名患者，请选择：\n", count);
        for (int i = 0; i < count; i++) {
            printf("  %d. [%d] %s, %d岁, 余额", i + 1,
                   results[i]->patientId, results[i]->name, results[i]->age);
            printMoney(results[i]->balanceCents); printf("\n");
        }
        int sel = safeInputInt("请输入序号: ");
        if (sel < 1 || sel > count) { printf("[错误] 无效选择！\n"); return NULL; }
        return results[sel - 1];
    }

    printf("[错误] 无效选择！\n");
    return NULL;
}

static DoctorNode *selectDoctor(void) {
    printf("\n选择科室（输入科室名，或 all 查看全部）: ");
    char dept[MAX_DEPT_LEN];
    safeInputString("", dept, MAX_DEPT_LEN);
    int showAll = (strcmp(dept, "all") == 0 || strcmp(dept, "ALL") == 0);

    printf("\n  %-6s %-10s %-14s %-12s 今日号源\n", "工号", "姓名", "职称", "科室");
    printf("  ----------------------------------------------------------\n");

    int found = 0;
    DoctorNode *d = g_doctorHead;
    while (d) {
        if (!d->isDeleted && d->schedule[g_currentWeekday]) {
            if (showAll || strcmp(d->department, dept) == 0) {
                int used = countDailyDoctorRegs(d->docId);
                printf("  %-6d %-10s %-14s %-12s %d/%d\n",
                       d->docId, d->name, d->level, d->department,
                       used, DAILY_MAX_PER_DOCTOR);
                found++;
            }
        }
        d = d->next;
    }

    if (found == 0) {
        printf("  [提示] 今天%s没有出诊医生。\n", showAll ? "" : dept);
        return NULL;
    }

    int docId = safeInputInt("\n请输入要挂号的医生工号: ");
    if (docId < 0) return NULL;
    DoctorNode *sel = findDoctorById(docId);
    if (!sel) { printf("[错误] 未找到医生 %d！\n", docId); return NULL; }
    if (!sel->schedule[g_currentWeekday]) {
        printf("[错误] 医生 %s 今天不出诊！\n", sel->name); return NULL;
    }
    return sel;
}

static int doRegister(PatientNode *patient, DoctorNode *doctor) {
    /* === 挂号限制校验（4 项） === */
    if (g_dailyRegCount >= DAILY_MAX_TICKETS) {
        printf("[拒绝] 今日全院挂号已满 %d！\n", DAILY_MAX_TICKETS); return 0;
    }
    int docRegs = countDailyDoctorRegs(doctor->docId);
    if (docRegs >= DAILY_MAX_PER_DOCTOR) {
        printf("[拒绝] 医生 %s 今日号源已满 (%d/%d)！\n",
               doctor->name, docRegs, DAILY_MAX_PER_DOCTOR); return 0;
    }
    int patRegs = countDailyPatientRegs(patient->patientId);
    if (patRegs >= DAILY_MAX_PER_PATIENT) {
        printf("[拒绝] 患者 %s 今日已挂 %d 个号，达到上限！\n",
               patient->name, patRegs); return 0;
    }
    int deptRegs = countDailyPatientDeptRegs(patient->patientId, doctor->department);
    if (deptRegs >= DAILY_MAX_PER_DEPT) {
        printf("[拒绝] 患者 %s 今日已在 %s 科挂过号！\n",
               patient->name, doctor->department); return 0;
    }

    /* === 挂号费 === */
    int regFee = isExpertDoctor(doctor) ? REG_FEE_EXPERT : REG_FEE_NORMAL;

    /* === 余额检查 === */
    if (patient->balanceCents < regFee) {
        printf("[提示] 患者余额不足！当前余额: ");
        printMoney(patient->balanceCents);
        printf("，挂号费: "); printMoney(regFee); printf("\n");
        int charge = safeInputInt("请输入充值金额（元，0取消）: ");
        if (charge <= 0) { printf("[取消]\n"); return 0; }
        patient->balanceCents += charge * 100;
        if (patient->balanceCents < regFee) {
            printf("[拒绝] 余额仍不足！\n"); return 0;
        }
    }

    /* === 扣费 & 记录 === */
    patient->balanceCents -= regFee;
    g_hospitalRevenue += regFee;
    g_dailyRegCount++;

    char regId[MAX_REG_ID_LEN];
    generateRegId(regId);
    strncpy(patient->lastRegId, regId, MAX_REG_ID_LEN - 1);

    RecordNode *rec = (RecordNode *)malloc(sizeof(RecordNode));
    if (!rec) { printf("[错误] 内存分配失败！\n"); return 0; }
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = doctor->docId;
    strncpy(rec->docName, doctor->name, MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_REGISTER;
    snprintf(rec->diagnosis, MAX_DIAG_LEN, "%s挂号-%s",
             isExpertDoctor(doctor) ? "专家" : "普通", doctor->department);
    rec->checkFeeCents = regFee;
    rec->totalCostCents = regFee;
    strncpy(rec->date, g_currentDate, 11);
    rec->next = NULL;
    appendRecord(rec);

    doctor->currentLoad++;

    printf("\n╔══════════════════════════════════════╗\n");
    printf("║         挂 号 成 功                  ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║  挂号号:   %-24s  ║\n", regId);
    printf("║  记录ID:   %-24s  ║\n", rec->recordId);
    printf("║  患者:     [%d] %-18s  ║\n", patient->patientId, patient->name);
    printf("║  科室:     %-24s  ║\n", doctor->department);
    printf("║  医生:     [%d] %-18s  ║\n", doctor->docId, doctor->name);
    printf("║  类型:     %-24s  ║\n", isExpertDoctor(doctor) ? "专家号" : "普通号");
    printf("║  挂号费:   "); printMoney(regFee); printf("%*s║\n", 22, "");
    printf("║  剩余余额: "); printMoney(patient->balanceCents); printf("%*s║\n", 21, "");
    printf("╚══════════════════════════════════════╝\n");
    return 1;
}

/* ==================== 对外接口 ==================== */

/**
 * RegisterPatient() —— 挂号模块主入口（题签规定的对外函数）
 *
 * 功能：
 *   - 新患者建档 + 挂号
 *   - 老患者查询（ID / 姓名重名二次确认）并挂号
 *   - 挂号号唯一性自动生成
 *   - 挂号限制校验（全院/医生/患者/科室）
 *   - 生成挂号记录、扣费
 */
void RegisterPatient(void) {
    printf("\n========================================\n");
    printf("           挂 号 窗 口\n");
    printf("========================================\n");
    printf("  1. 新患者建档并挂号\n");
    printf("  2. 老患者挂号\n");
    printf("  0. 返回\n");
    printf("========================================\n");

    int choice = safeInputInt("请选择: ");
    PatientNode *patient = NULL;

    switch (choice) {
        case 1: patient = createNewPatient();   break;
        case 2: patient = findExistingPatient(); break;
        case 0: return;
        default: printf("[错误] 无效选择！\n"); return;
    }

    if (!patient) { printf("[取消] 挂号流程终止。\n"); return; }

    /* 住院患者挂号确认 */
    InpatientNode *ip = findActiveInpatient(patient->patientId);
    if (ip) {
        printf("[提示] 患者正在住院（床位%d），是否仍要挂号？(y/n): ", ip->bedId);
        char yn[4]; safeInputString("", yn, 4);
        if (yn[0] != 'y' && yn[0] != 'Y') { printf("[取消]\n"); return; }
    }

    DoctorNode *doctor = selectDoctor();
    if (!doctor) { printf("[取消] 挂号流程终止。\n"); return; }

    doRegister(patient, doctor);
}
