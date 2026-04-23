/*************************************************************
 * billing.c  —  住院收费模块（刘承庚）
 *
 * 对外函数：
 *   ProcessBilling()  —— 题签规定的唯一对外接口
 *
 * 内部逻辑（均为 static，不对外暴露）：
 *   - 住院办理（入院、选床位、收押金）
 *   - 每日08:00自动扣费
 *   - 押金预警/欠费预警
 *   - 出院结算（00:00-08:00 / 08:00之后的计费规则）
 *   - 押金追缴
 *
 * 说明：
 *   题签中"欠费风险预警接口"、"每日自动扣费"、"出院结算"
 *   均为 ProcessBilling 的内部子功能，不单独做成对外函数。
 *************************************************************/
#include "outpatient_inpatient.h"

/* 供 see_doctor.c 在"转住院"时内部调用的共享入院入口 */
/* 由于题签只允许暴露 ProcessBilling，这里采用文件间共享但不写入 outpatient_inpatient.h */
void __admitPatientFromVisit(PatientNode *patient, DoctorNode *doctor);

/* -------------------- 内部辅助函数（静态） -------------------- */

/* 列出指定科室的空闲床位 */
static void listFreeBeds(const char *dept) {
    printf("\n  %-6s %-10s %-12s %-8s\n", "床位号", "病房类型", "关联科室", "状态");
    printf("  -------------------------------------------------\n");

    int count = 0;
    BedNode *b = g_bedHead;
    while (b) {
        if (!b->isDeleted && b->bedStatus == BED_FREE) {
            if (dept && strlen(dept) > 0 && strcmp(dept, "all") != 0) {
                if (strcmp(b->relatedDept, dept) != 0) {
                    b = b->next;
                    continue;
                }
            }
            printf("  %-6d %-10s %-12s 空闲\n",
                   b->bedId, getWardTypeName(b->wardType), b->relatedDept);
            count++;
        }
        b = b->next;
    }

    if (count == 0) {
        printf("  [提示] 暂无空闲床位。\n");
    } else {
        printf("  共 %d 张空闲床位。\n", count);
    }
}

/* ---------- 住院办理内部实现 ---------- */
static void admitNewPatient(PatientNode *patient, DoctorNode *doctor) {
    if (!patient || !doctor) return;

    /* 检查是否已在住院 */
    InpatientNode *existing = findActiveInpatient(patient->patientId);
    if (existing) {
        printf("[错误] 患者 %s 已在住院（床位 %d），不能重复入院！\n",
               patient->name, existing->bedId);
        return;
    }

    printf("\n========================================\n");
    printf("         住 院 办 理\n");
    printf("========================================\n");
    printf("  患者: [%d] %s, %d岁\n", patient->patientId, patient->name, patient->age);
    printf("  主治医生: [%d] %s (%s)\n", doctor->docId, doctor->name, doctor->department);
    printf("========================================\n");

    printf("\n输入科室名筛选床位（或输入 all 查看全部）: ");
    char dept[MAX_DEPT_LEN];
    safeInputString("", dept, MAX_DEPT_LEN);
    listFreeBeds(dept);

    int bedId = safeInputInt("\n请输入要安排的床位号 (0=取消): ");
    if (bedId <= 0) {
        printf("[取消] 住院办理已取消。\n");
        return;
    }

    BedNode *bed = findBedById(bedId);
    if (!bed) {
        printf("[错误] 未找到床位 %d！\n", bedId);
        return;
    }
    if (bed->bedStatus != BED_FREE) {
        printf("[错误] 床位 %d 当前不可用（状态: %d）！\n", bedId, bed->bedStatus);
        return;
    }

    int expectedDays = safeInputInt("请输入预计住院天数: ");
    if (expectedDays <= 0) {
        printf("[错误] 住院天数必须大于0！\n");
        return;
    }

    int dailyFee = getWardDailyFee(bed->wardType);
    int minDeposit = 20000 * expectedDays;  /* 200元 x 天数 */
    if (minDeposit % DEPOSIT_UNIT != 0) {
        minDeposit = ((minDeposit / DEPOSIT_UNIT) + 1) * DEPOSIT_UNIT;
    }

    printf("\n  病房类型: %s\n", getWardTypeName(bed->wardType));
    printf("  日住院费: ");
    printMoney(dailyFee);
    printf("\n  预计住院: %d 天\n", expectedDays);
    printf("  最低押金: ");
    printMoney(minDeposit);
    printf("（需为100元整数倍，≥200元×天数）\n");
    printf("  患者余额: ");
    printMoney(patient->balanceCents);
    printf("\n");

    int depositYuan = safeInputInt("请输入缴纳押金（元）: ");
    if (depositYuan <= 0) {
        printf("[取消] 住院办理已取消。\n");
        return;
    }
    int depositCents = depositYuan * 100;

    if (depositCents % DEPOSIT_UNIT != 0) {
        printf("[错误] 押金必须是100元的整数倍！\n");
        return;
    }
    if (depositCents < minDeposit) {
        printf("[错误] 押金不足！最低需要 ");
        printMoney(minDeposit);
        printf("\n");
        return;
    }

    printf("\n  从患者账户余额扣除押金？(y=从余额扣 / n=现金缴纳): ");
    char yn[4];
    safeInputString("", yn, 4);
    if (yn[0] == 'y' || yn[0] == 'Y') {
        if (patient->balanceCents < depositCents) {
            printf("[提示] 余额不足，需先充值。\n");
            int charge = safeInputInt("请输入充值金额（元，0取消）: ");
            if (charge <= 0) {
                printf("[取消] 住院办理已取消。\n");
                return;
            }
            patient->balanceCents += charge * 100;
        }
        if (patient->balanceCents < depositCents) {
            printf("[错误] 充值后余额仍不足！\n");
            return;
        }
        patient->balanceCents -= depositCents;
    }

    /* 绑定床位 */
    bed->bedStatus = BED_OCCUPIED;
    bed->patientId = patient->patientId;

    /* 创建 InpatientNode */
    InpatientNode *ip = (InpatientNode *)malloc(sizeof(InpatientNode));
    memset(ip, 0, sizeof(InpatientNode));
    ip->patientId = patient->patientId;
    ip->docId = doctor->docId;
    ip->bedId = bedId;
    ip->depositCents = depositCents;
    ip->dailyFeeCents = dailyFee;
    strncpy(ip->admitDate, g_currentDate, 11);
    ip->expectedDays = expectedDays;
    ip->daysStayed = 0;
    ip->totalCharged = 0;
    ip->isAdmitted = 1;
    ip->isDeleted = 0;
    ip->next = NULL;

    if (!g_inpatientHead) {
        g_inpatientHead = ip;
    } else {
        InpatientNode *tail = g_inpatientHead;
        while (tail->next) tail = tail->next;
        tail->next = ip;
    }

    /* 创建 type=5 住院记录 */
    RecordNode *rec = (RecordNode *)malloc(sizeof(RecordNode));
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = doctor->docId;
    strncpy(rec->docName, doctor->name, MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_INPATIENT;
    snprintf(rec->diagnosis, MAX_DIAG_LEN, "住院-%s-床位%d-预计%d天",
             getWardTypeName(bed->wardType), bedId, expectedDays);
    rec->checkFeeCents = depositCents;
    rec->totalCostCents = depositCents;
    strncpy(rec->date, g_currentDate, 11);
    rec->next = NULL;
    appendRecord(rec);

    g_hospitalRevenue += depositCents;

    printf("\n╔══════════════════════════════════════╗\n");
    printf("║       住 院 办 理 成 功              ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║  记录ID:   %-24s  ║\n", rec->recordId);
    printf("║  患者:     [%d] %-18s  ║\n", patient->patientId, patient->name);
    printf("║  主治医生: [%d] %-18s  ║\n", doctor->docId, doctor->name);
    printf("║  床位号:   %-24d  ║\n", bedId);
    printf("║  病房类型: %-24s  ║\n", getWardTypeName(bed->wardType));
    printf("║  日住院费: ");
    printMoney(dailyFee);
    printf("%*s║\n", 22, "");
    printf("║  已缴押金: ");
    printMoney(depositCents);
    printf("%*s║\n", 22, "");
    printf("║  预计天数: %-24d  ║\n", expectedDays);
    printf("║  入院日期: %-24s  ║\n", g_currentDate);
    printf("╚══════════════════════════════════════╝\n");
}

/* 供 see_doctor.c "转住院"调用的跨文件入口
 * 这是内部共享接口，不写入 outpatient_inpatient.h，仅作为 billing 与 see_doctor 间的桥梁。
 */
void __admitPatientFromVisit(PatientNode *patient, DoctorNode *doctor) {
    admitNewPatient(patient, doctor);
}

/* ---------- 每日自动扣费（内部） ---------- */
static void runDailyAutoCharge(void) {
    printf("\n========================================\n");
    printf("  执行每日自动扣费 (模拟08:00)\n");
    printf("========================================\n");

    int chargedCount = 0;
    int warningCount = 0;

    InpatientNode *ip = g_inpatientHead;
    while (ip) {
        if (ip->isAdmitted && !ip->isDeleted) {
            ip->depositCents -= ip->dailyFeeCents;
            ip->totalCharged += ip->dailyFeeCents;
            ip->daysStayed++;

            PatientNode *p = findPatientById(ip->patientId);
            printf("  [扣费] 患者 %d", ip->patientId);
            if (p) printf(" (%s)", p->name);
            printf(" | 床位 %d | 扣 ", ip->bedId);
            printMoney(ip->dailyFeeCents);
            printf(" | 押金余额 ");
            printMoney(ip->depositCents);
            printf(" | 已住 %d 天\n", ip->daysStayed);

            chargedCount++;

            if (ip->depositCents < DEPOSIT_SAFE_LINE) {
                printf("  ⚠️  [预警] 患者 %d 押金低于安全线（", ip->patientId);
                printMoney(DEPOSIT_SAFE_LINE);
                printf("），当前押金: ");
                printMoney(ip->depositCents);
                printf("\n");
                warningCount++;
            }
            if (ip->depositCents < 0) {
                printf("  🚨 [欠费] 患者 %d 已欠费 ", ip->patientId);
                printMoney(-ip->depositCents);
                printf("，请尽快催缴！\n");
            }
        }
        ip = ip->next;
    }

    printf("\n  本次扣费完成：共 %d 人，%d 人触发预警。\n", chargedCount, warningCount);
}

/* ---------- 出院结算（内部） ---------- */
static void runDischarge(void) {
    printf("\n========================================\n");
    printf("         出 院 结 算\n");
    printf("========================================\n");

    printf("\n  当前在院患者：\n");
    printf("  %-6s %-10s %-6s %-10s %-8s %-10s\n",
           "患者ID", "姓名", "床位", "病房类型", "已住天数", "押金余额");
    printf("  ---------------------------------------------------------------\n");

    int count = 0;
    InpatientNode *ip = g_inpatientHead;
    while (ip) {
        if (ip->isAdmitted && !ip->isDeleted) {
            PatientNode *p = findPatientById(ip->patientId);
            BedNode *bed = findBedById(ip->bedId);
            printf("  %-6d %-10s %-6d %-10s %-8d ",
                   ip->patientId,
                   p ? p->name : "未知",
                   ip->bedId,
                   bed ? getWardTypeName(bed->wardType) : "未知",
                   ip->daysStayed);
            printMoney(ip->depositCents);
            printf("\n");
            count++;
        }
        ip = ip->next;
    }

    if (count == 0) {
        printf("  [提示] 当前没有在院患者。\n");
        return;
    }

    int patId = safeInputInt("\n请输入要出院的患者ID (0=取消): ");
    if (patId <= 0) return;

    InpatientNode *target = findActiveInpatient(patId);
    if (!target) {
        printf("[错误] 未找到该患者的住院记录！\n");
        return;
    }

    PatientNode *patient = findPatientById(patId);
    if (!patient) {
        printf("[错误] 患者数据异常！\n");
        return;
    }

    int hour = safeInputInt("请输入当前出院时间（小时，0~23）: ");
    if (hour < 0 || hour > 23) {
        printf("[错误] 时间不合法！\n");
        return;
    }

    /* 出院计费规则：08:00前免当天，08:00后收当天 */
    int todayCharge = 0;
    if (hour >= 8) {
        todayCharge = target->dailyFeeCents;
        printf("  [计费] 08:00后出院，收取当天住院费: ");
        printMoney(todayCharge);
        printf("\n");
    } else {
        printf("  [计费] 08:00前出院，免收当天住院费。\n");
    }

    target->depositCents -= todayCharge;
    target->totalCharged += todayCharge;

    int refund = target->depositCents;

    printf("\n  ---- 出院结算清单 ----\n");
    printf("  患者:       [%d] %s\n", patient->patientId, patient->name);
    printf("  入院日期:   %s\n", target->admitDate);
    printf("  住院天数:   %d 天\n", target->daysStayed + (todayCharge > 0 ? 1 : 0));
    printf("  日住院费:   ");
    printMoney(target->dailyFeeCents);
    printf("\n  已扣费总额: ");
    printMoney(target->totalCharged);
    printf("\n");

    if (refund >= 0) {
        printf("  退还押金:   ");
        printMoney(refund);
        printf("\n");
        patient->balanceCents += refund;
    } else {
        printf("  欠费金额:   ");
        printMoney(-refund);
        printf("\n");
        if (patient->balanceCents >= (-refund)) {
            patient->balanceCents += refund;  /* refund < 0 */
            printf("  已从患者账户补扣欠费。\n");
        } else {
            printf("  ⚠️  患者账户余额不足以补扣欠费！\n");
            printf("  账户余额: ");
            printMoney(patient->balanceCents);
            printf("\n  请安排人工处理剩余欠款。\n");
            patient->balanceCents = 0;
        }
    }

    /* 释放床位 */
    BedNode *bed = findBedById(target->bedId);
    if (bed) {
        bed->bedStatus = BED_FREE;
        bed->patientId = 0;
    }
    target->isAdmitted = 0;

    RecordNode *rec = (RecordNode *)malloc(sizeof(RecordNode));
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = target->docId;
    DoctorNode *doc = findDoctorById(target->docId);
    if (doc) strncpy(rec->docName, doc->name, MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_INPATIENT;
    snprintf(rec->diagnosis, MAX_DIAG_LEN, "出院结算-住%d天",
             target->daysStayed + (todayCharge > 0 ? 1 : 0));
    rec->totalCostCents = target->totalCharged;
    strncpy(rec->date, g_currentDate, 11);
    rec->next = NULL;
    appendRecord(rec);

    printf("\n  出院记录: %s\n", rec->recordId);
    printf("  患者当前余额: ");
    printMoney(patient->balanceCents);
    printf("\n");
    printf("  [完成] 患者 %s 出院手续已办理。\n", patient->name);
}

/* ---------- 欠费预警（内部） ---------- */
static void showArrearList(void) {
    printf("\n========================================\n");
    printf("       住院欠费/预警一览\n");
    printf("========================================\n");
    printf("  %-6s %-10s %-6s %-10s %-12s %-8s\n",
           "患者ID", "姓名", "床位", "病房类型", "押金余额", "状态");
    printf("  ---------------------------------------------------------------\n");

    int count = 0;
    InpatientNode *ip = g_inpatientHead;
    while (ip) {
        if (ip->isAdmitted && !ip->isDeleted) {
            if (ip->depositCents < DEPOSIT_SAFE_LINE) {
                PatientNode *p = findPatientById(ip->patientId);
                BedNode *bed = findBedById(ip->bedId);
                const char *status = ip->depositCents < 0 ? "🚨欠费" : "⚠️预警";

                printf("  %-6d %-10s %-6d %-10s ",
                       ip->patientId,
                       p ? p->name : "未知",
                       ip->bedId,
                       bed ? getWardTypeName(bed->wardType) : "未知");
                printMoney(ip->depositCents);
                printf("    %s\n", status);
                count++;
            }
        }
        ip = ip->next;
    }

    if (count == 0) {
        printf("  [良好] 当前所有住院患者押金充足，无预警。\n");
    } else {
        printf("\n  共 %d 位患者需要关注。\n", count);
    }
}

/* ---------- 追缴押金（内部） ---------- */
static void addDeposit(void) {
    int patId = safeInputInt("请输入患者ID: ");
    if (patId <= 0) return;
    InpatientNode *ip = findActiveInpatient(patId);
    if (!ip) {
        printf("[错误] 未找到该患者的住院信息！\n");
        return;
    }
    printf("  当前押金余额: ");
    printMoney(ip->depositCents);
    printf("\n");
    int addYuan = safeInputInt("请输入追缴金额（元，需为100的整数倍）: ");
    if (addYuan <= 0) return;
    int addCents = addYuan * 100;
    if (addCents % DEPOSIT_UNIT != 0) {
        printf("[错误] 追缴金额必须是100元的整数倍！\n");
        return;
    }
    ip->depositCents += addCents;
    printf("  [成功] 追缴 ");
    printMoney(addCents);
    printf("，当前押金余额: ");
    printMoney(ip->depositCents);
    printf("\n");
}

/* ---------- 直接办理住院（内部，从主菜单进入） ---------- */
static void admitFromBillingMenu(void) {
    int patId = safeInputInt("请输入患者ID: ");
    if (patId <= 0) return;
    PatientNode *p = findPatientById(patId);
    if (!p) {
        printf("[错误] 未找到患者 %d！\n", patId);
        return;
    }
    int docId = safeInputInt("请输入主治医生工号: ");
    if (docId <= 0) return;
    DoctorNode *d = findDoctorById(docId);
    if (!d) {
        printf("[错误] 未找到医生 %d！\n", docId);
        return;
    }
    admitNewPatient(p, d);
}

/* ==================== 对外接口 ==================== */

/**
 * ProcessBilling() - 住院收费模块主入口（题签规定的对外函数）
 *
 * 整合功能：
 *   - 住院办理（录入住院时间、预计天数、押金，校验押金规则）
 *   - 每日08:00自动扣费
 *   - 押金预警 & 欠费预警接口
 *   - 出院结算（按出院时段判断是否收当天费用）
 *   - 押金追缴
 */
void ProcessBilling(void) {
    printf("\n");
    printf("========================================\n");
    printf("         住 院 管 理\n");
    printf("========================================\n");
    printf("  1. 住院办理（新入院）\n");
    printf("  2. 每日自动扣费（模拟）\n");
    printf("  3. 出院结算\n");
    printf("  4. 欠费预警查看\n");
    printf("  5. 追缴押金（住院期间补缴）\n");
    printf("  0. 返回上级菜单\n");
    printf("========================================\n");

    int choice = safeInputInt("请选择: ");

    switch (choice) {
        case 1: admitFromBillingMenu(); break;
        case 2: runDailyAutoCharge();   break;
        case 3: runDischarge();         break;
        case 4: showArrearList();       break;
        case 5: addDeposit();           break;
        case 0: return;
        default:
            printf("[错误] 无效选择！\n");
            break;
    }
}
