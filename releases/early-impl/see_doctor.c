/*************************************************************
 * see_doctor.c  —  看诊模块（刘承庚）
 *
 * 对外函数：SeeDoctor()
 * 其余均为 static 私有辅助函数。
 *************************************************************/
#include "outpatient_inpatient.h"

/* billing.c 的内部共享入口（不写入 outpatient_inpatient.h） */
extern void __admitPatientFromVisit(PatientNode *patient, DoctorNode *doctor);

/* -------------------- 内部辅助函数 -------------------- */

static RecordNode *selectRegisteredPatient(void) {
    printf("\n  今日待诊患者列表：\n");
    printf("  %-14s %-6s %-10s %-6s %-10s %-12s\n",
           "记录ID", "患者ID", "患者姓名", "医生ID", "医生姓名", "科室");
    printf("  ----------------------------------------------------------------\n");

    int count = 0;
    RecordNode *candidates[200];
    RecordNode *r = g_recordHead;
    while (r) {
        if (r->type == REC_TYPE_REGISTER
            && strcmp(r->date, g_currentDate) == 0
            && r->status == 0
            && !r->isRedInk && !r->isDeleted) {
            DoctorNode *doc = findDoctorById(r->docId);
            printf("  %-14s %-6d %-10s %-6d %-10s %-12s\n",
                   r->recordId, r->patientId, r->patientName,
                   r->docId, r->docName, doc ? doc->department : "未知");
            if (count < 200) candidates[count] = r;
            count++;
        }
        r = r->next;
    }

    if (count == 0) { printf("  [提示] 今天暂无待诊患者。\n"); return NULL; }

    char recId[MAX_REC_ID_LEN];
    safeInputString("\n请输入要接诊的记录ID: ", recId, MAX_REC_ID_LEN);

    for (int i = 0; i < count && i < 200; i++) {
        if (strcmp(candidates[i]->recordId, recId) == 0) return candidates[i];
    }
    printf("[错误] 未找到记录ID \"%s\"！\n", recId);
    return NULL;
}

static void doExamination(PatientNode *patient, DoctorNode *doctor) {
    printf("\n--- 检查项目 ---\n");
    char diagnosis[MAX_DIAG_LEN];
    safeInputString("请输入检查项目名称: ", diagnosis, MAX_DIAG_LEN);
    if (strlen(diagnosis) == 0) { printf("[取消]\n"); return; }

    int fee = safeInputInt("请输入检查费用（元）: ");
    if (fee <= 0) { printf("[错误] 检查费用必须大于0！\n"); return; }
    int feeCents = fee * 100;

    if (patient->balanceCents < feeCents) {
        printf("[提示] 余额不足！当前: "); printMoney(patient->balanceCents);
        printf("，费用: "); printMoney(feeCents); printf("\n");
        int charge = safeInputInt("请输入充值金额（元，0取消）: ");
        if (charge <= 0) { printf("[取消]\n"); return; }
        patient->balanceCents += charge * 100;
        if (patient->balanceCents < feeCents) { printf("[拒绝] 余额仍不足！\n"); return; }
    }

    patient->balanceCents -= feeCents;
    g_hospitalRevenue += feeCents;

    RecordNode *rec = (RecordNode *)malloc(sizeof(RecordNode));
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = doctor->docId;
    strncpy(rec->docName, doctor->name, MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_EXAM;
    strncpy(rec->diagnosis, diagnosis, MAX_DIAG_LEN - 1);
    rec->checkFeeCents = feeCents;
    rec->totalCostCents = feeCents;
    strncpy(rec->date, g_currentDate, 11);
    appendRecord(rec);

    printf("\n  [检查记录已生成] %s | %s | 费用: ", rec->recordId, diagnosis);
    printMoney(feeCents); printf("\n");
}

static void doPrescription(PatientNode *patient, DoctorNode *doctor) {
    printf("\n--- 开药处方 ---\n  输入药品ID，0 结束开药。\n");
    while (1) {
        int medId = safeInputInt("\n请输入药品ID (0=结束): ");
        if (medId == 0) break;
        if (medId < 0) continue;

        MedicineNode *med = findMedicineById(medId);
        if (!med) { printf("[错误] 未找到药品 %d！\n", medId); continue; }

        printf("  药品: %s (%s / %s) 单价: ",
               med->officialName, med->tradeName, med->aliasName);
        printMoney(med->priceCents);
        printf("  库存: %d\n", med->stock);

        int qty = safeInputInt("请输入数量 (1~100): ");
        if (qty <= 0 || qty > MAX_PRESCRIPTION_QTY) {
            printf("[错误] 数量不合法（1~%d）！\n", MAX_PRESCRIPTION_QTY); continue;
        }
        if (qty > med->stock) { printf("[错误] 库存不足！\n"); continue; }

        int totalCents = med->priceCents * qty;
        printf("  合计: "); printMoney(totalCents); printf("\n");

        if (patient->balanceCents < totalCents) {
            printf("[提示] 余额不足！\n");
            int charge = safeInputInt("请输入充值金额（元，0跳过）: ");
            if (charge <= 0) { printf("[跳过]\n"); continue; }
            patient->balanceCents += charge * 100;
            if (patient->balanceCents < totalCents) {
                printf("[跳过] 余额仍不足！\n"); continue;
            }
        }

        patient->balanceCents -= totalCents;
        med->stock -= qty;
        g_hospitalRevenue += totalCents;

        RecordNode *rec = (RecordNode *)malloc(sizeof(RecordNode));
        memset(rec, 0, sizeof(RecordNode));
        generateRecordId(rec->recordId);
        rec->patientId = patient->patientId;
        strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
        rec->docId = doctor->docId;
        strncpy(rec->docName, doctor->name, MAX_NAME_LEN - 1);
        rec->type = REC_TYPE_PRESCRIBE;
        snprintf(rec->diagnosis, MAX_DIAG_LEN, "处方: %s x%d", med->officialName, qty);
        rec->medId = med->medId;
        rec->medCount = qty;
        rec->totalCostCents = totalCents;
        strncpy(rec->date, g_currentDate, 11);
        appendRecord(rec);

        printf("  [处方] %s | %s x%d = ", rec->recordId, med->officialName, qty);
        printMoney(totalCents); printf("\n");
    }
}

static void doAdmitForward(PatientNode *patient, DoctorNode *doctor) {
    InpatientNode *ip = findActiveInpatient(patient->patientId);
    if (ip) {
        printf("[提示] 患者已在住院（床位%d），不能重复入院！\n", ip->bedId); return;
    }
    printf("\n  将患者 [%d] %s 转入住院流程...\n", patient->patientId, patient->name);
    __admitPatientFromVisit(patient, doctor);
}

/* 记录撤销（红冲机制：先撤销旧记录，再新增正确记录） */
static void doRedInk(void) {
    printf("\n--- 记录撤销（红冲）---\n");
    printf("说明：原记录仅标记为红冲（作废），如需修改请再另行新增正确记录。\n\n");

    char recId[MAX_REC_ID_LEN];
    safeInputString("请输入要撤销的记录ID: ", recId, MAX_REC_ID_LEN);

    RecordNode *r = g_recordHead;
    while (r) {
        if (strcmp(r->recordId, recId) == 0 && !r->isDeleted) break;
        r = r->next;
    }
    if (!r)          { printf("[错误] 未找到记录 \"%s\"！\n", recId); return; }
    if (r->isRedInk) { printf("[提示] 该记录已红冲。\n"); return; }

    printf("\n  即将撤销：%s | 患者[%d]%s | 医生[%d]%s | 类型%d | 费用",
           r->recordId, r->patientId, r->patientName,
           r->docId, r->docName, r->type);
    printMoney(r->totalCostCents); printf("\n");

    printf("确认撤销？(y/n): ");
    char yn[4]; safeInputString("", yn, 4);
    if (yn[0] != 'y' && yn[0] != 'Y') { printf("[取消]\n"); return; }

    r->isRedInk = 1;

    /* 退款 */
    PatientNode *patient = findPatientById(r->patientId);
    if (patient && r->totalCostCents > 0) {
        patient->balanceCents += r->totalCostCents;
        g_hospitalRevenue -= r->totalCostCents;
        printf("  [退款] 退还 "); printMoney(r->totalCostCents); printf(" 至患者账户。\n");
    }

    /* 开药记录：恢复库存 */
    if ((r->type == REC_TYPE_PRESCRIBE || r->type == REC_TYPE_VISIT)
        && r->medId > 0 && r->medCount > 0) {
        MedicineNode *med = findMedicineById(r->medId);
        if (med) {
            med->stock += r->medCount;
            printf("  [退药] %s 恢复 %d，当前库存 %d\n",
                   med->officialName, r->medCount, med->stock);
        }
    }

    /* 挂号记录：恢复计数 */
    if (r->type == REC_TYPE_REGISTER) {
        if (g_dailyRegCount > 0) g_dailyRegCount--;
        DoctorNode *doc = findDoctorById(r->docId);
        if (doc && doc->currentLoad > 0) doc->currentLoad--;
    }

    printf("  [完成] 记录 %s 已红冲。\n", recId);
}

/* ==================== 对外接口 ==================== */

/**
 * SeeDoctor() —— 看诊模块主入口（题签规定的对外函数）
 *
 * 功能：
 *   - 从当日挂号记录中选择就诊患者
 *   - 医生录入诊断信息
 *   - 子操作：开检查单 / 开处方 / 转住院
 *   - 记录撤销（红冲）
 */
void SeeDoctor(void) {
    printf("\n========================================\n");
    printf("           看 诊 系 统\n");
    printf("========================================\n");
    printf("  1. 接诊（选择已挂号患者）\n");
    printf("  2. 撤销/红冲历史记录\n");
    printf("  0. 返回\n");
    printf("========================================\n");

    int choice = safeInputInt("请选择: ");
    if (choice == 2) { doRedInk(); return; }
    if (choice == 0) return;
    if (choice != 1) { printf("[错误] 无效选择！\n"); return; }

    RecordNode *regRec = selectRegisteredPatient();
    if (!regRec) return;
    regRec->status = 1;  /* 1 = 已就诊 */

    PatientNode *patient = findPatientById(regRec->patientId);
    DoctorNode  *doctor  = findDoctorById(regRec->docId);
    if (!patient || !doctor) { printf("[错误] 数据异常！\n"); return; }

    printf("\n────────────────────────────────────────\n");
    printf("  就诊中：患者[%d]%s %d岁 | 医生[%d]%s (%s-%s)\n",
           patient->patientId, patient->name, patient->age,
           doctor->docId, doctor->name, doctor->level, doctor->department);
    printf("────────────────────────────────────────\n");

    char diagnosis[MAX_DIAG_LEN];
    safeInputString("请输入诊断结果: ", diagnosis, MAX_DIAG_LEN);

    RecordNode *visitRec = (RecordNode *)malloc(sizeof(RecordNode));
    memset(visitRec, 0, sizeof(RecordNode));
    generateRecordId(visitRec->recordId);
    visitRec->patientId = patient->patientId;
    strncpy(visitRec->patientName, patient->name, MAX_NAME_LEN - 1);
    visitRec->docId = doctor->docId;
    strncpy(visitRec->docName, doctor->name, MAX_NAME_LEN - 1);
    visitRec->type = REC_TYPE_VISIT;
    strncpy(visitRec->diagnosis, diagnosis, MAX_DIAG_LEN - 1);
    strncpy(visitRec->date, g_currentDate, 11);
    appendRecord(visitRec);

    printf("  [看诊记录] %s 已生成。\n", visitRec->recordId);

    int running = 1;
    while (running) {
        printf("\n  ---- 就诊操作 ----\n");
        printf("  1. 开检查单\n  2. 开处方（开药）\n  3. 转住院\n  0. 结束本次就诊\n");

        int sub = safeInputInt("  请选择: ");
        switch (sub) {
            case 1: doExamination(patient, doctor); break;
            case 2: doPrescription(patient, doctor); break;
            case 3: doAdmitForward(patient, doctor); break;
            case 0: running = 0; break;
            default: printf("  [错误] 无效选择！\n"); break;
        }
    }

    printf("\n  [完成] 患者 %s 本次就诊结束。余额: ", patient->name);
    printMoney(patient->balanceCents); printf("\n");
}
