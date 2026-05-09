#include "outpatient_inpatient.h"
#include "ward.h"
#include "module_d.h"   /* 修复 Bug 09：使用 WriteLog 记录现金押金操作 */
#include <time.h>       /* V5 修复 Bug 22/23：用 mktime 计算日期差 */

/* V5 修复 Bug 23：计算两个 yyyymmdd 字符串之间的天数差（end - start），
 * 失败返回 0。 */
static int computeDateDiffDays(const char* startDate, const char* endDate) {
    struct tm s = { 0 }, e = { 0 };
    time_t st, et;
    int sy, sm, sd, ey, em, ed;
    if (!startDate || !endDate) return 0;
    if (strlen(startDate) != 8 || strlen(endDate) != 8) return 0;
    if (sscanf(startDate, "%4d%2d%2d", &sy, &sm, &sd) != 3) return 0;
    if (sscanf(endDate, "%4d%2d%2d", &ey, &em, &ed) != 3) return 0;
    s.tm_year = sy - 1900; s.tm_mon = sm - 1; s.tm_mday = sd;
    e.tm_year = ey - 1900; e.tm_mon = em - 1; e.tm_mday = ed;
    s.tm_hour = 12; e.tm_hour = 12;   /* 避免 DST 影响 */
    st = mktime(&s);
    et = mktime(&e);
    if (st == (time_t)-1 || et == (time_t)-1) return 0;
    return (int)((et - st) / (60 * 60 * 24));
}

static void listFreeBeds(const char* dept) {
    BedNode* b = g_bedHead;
    int count = 0;
    printf("\n%-6s %-6s %-10s %-12s %-8s\n", "病房", "床位", "类型", "科室", "状态");
    printf("------------------------------------------------------\n");
    while (b) {
        if (!b->isDeleted && b->bedStatus == BED_FREE) {
            if (!dept || strcmp(dept, "all") == 0 || strcmp(b->relatedDept, dept) == 0) {
                printf("%-6d %-6d %-10s %-12s 空闲\n",
                    b->wardId, b->bedId, getWardTypeName(b->wardType), b->relatedDept);
                count++;
            }
        }
        b = b->next;
    }
    if (count == 0) printf("暂无空闲床位。\n");
}

static void createInpatientAdmissionRecord(PatientNode* patient, DoctorNode* doctor, BedNode* bed, Money deposit, int expectedDays) {
    RecordNode* rec = (RecordNode*)malloc(sizeof(RecordNode));
    if (!rec) return;
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = doctor->docId;
    strncpy(rec->docName, doctor->name, MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_INPATIENT;
    snprintf(rec->diagnosis, MAX_DIAG_LEN, "住院办理_%s_预计%d天", getWardTypeName(bed->wardType), expectedDays);
    rec->wardId = bed->wardId;
    rec->bedId = bed->bedId;
    rec->totalCostCents = deposit;
    strncpy(rec->date, g_currentDate, MAX_DATE_LEN - 1);
    rec->hour = g_currentHour;
    rec->minute = g_currentMinute;
    appendRecord(rec);
}

static void createSettlementRecord(InpatientNode* ip, PatientNode* patient, DoctorNode* doctor) {
    RecordNode* rec = (RecordNode*)malloc(sizeof(RecordNode));
    if (!rec) return;
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = doctor ? doctor->docId : ip->docId;
    strncpy(rec->docName, doctor ? doctor->name : "未知", MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_SETTLEMENT;
    snprintf(rec->diagnosis, MAX_DIAG_LEN, "出院结算_已住%d天", ip->daysStayed);
    rec->wardId = ip->wardId;
    rec->bedId = ip->bedId;
    rec->totalCostCents = ip->totalChargedCents;
    strncpy(rec->date, g_currentDate, MAX_DATE_LEN - 1);
    rec->hour = g_currentHour;
    rec->minute = g_currentMinute;
    appendRecord(rec);
}

static void admitNewPatient(PatientNode* patient, DoctorNode* doctor) {
    char dept[MAX_DEPT_LEN];
    int bedId;
    int expectedDays;
    Money deposit;
    Money minDeposit;
    BedNode* bed;
    InpatientNode* ip;

    if (!patient || !doctor) return;
    if (findActiveInpatient(patient->patientId)) {
        printf("该患者已经在住院中。\n");
        return;
    }

    safeInputString("输入科室筛选床位（all=全部）：", dept, MAX_DEPT_LEN);
    listFreeBeds(strlen(dept) == 0 ? "all" : dept);

    bedId = safeInputInt("请输入床位ID（0取消）：");
    if (bedId == 0) return;

    bed = findBedById(bedId);
    if (!bed || bed->bedStatus != BED_FREE) {
        printf("床位不存在或不可用。\n");
        return;
    }

    expectedDays = safeInputInt("请输入预计住院天数：");
    if (expectedDays <= 0) {
        printf("住院天数必须大于0。\n");
        return;
    }

    minDeposit = (Money)expectedDays * 20000;
    if (minDeposit % DEPOSIT_UNIT != 0) {
        minDeposit = ((minDeposit / DEPOSIT_UNIT) + 1) * DEPOSIT_UNIT;
    }

    printf("该床位类型=%s 日费=", getWardTypeName(bed->wardType));
    printMoney(getWardDailyFee(bed->wardType));
    printf("\n最低押金=");
    printMoney(minDeposit);
    printf("\n");

    deposit = safeInputMoneyFen("请输入押金金额（元）：");
    if (deposit % DEPOSIT_UNIT != 0) {
        printf("押金必须是100元的整数倍。\n");
        return;
    }
    if (deposit < minDeposit) {
        printf("押金不足。\n");
        return;
    }

    {
        /* 修复 Bug 09：选 n（现金支付）时要求二次确认并写操作日志，
         * 避免凭空创造押金。 */
        char yn[8];
        safeInputString("是否从患者账户余额中扣除押金？(y/n)：", yn, 8);
        if (yn[0] == 'y' || yn[0] == 'Y') {
            /* V5 修复 Bug 25：与挂号/检查/开药一致，余额不足时弹现场充值，
             * 而非直接拒绝。 */
            if (!ensureEnoughBalance(patient, deposit, "住院押金")) {
                printf("押金未到位，住院办理取消。\n");
                return;
            }
            patient->balanceCents -= deposit;
        }
        else {
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

    bed->bedStatus = BED_OCCUPIED;
    bed->patientId = patient->patientId;

    ip = (InpatientNode*)malloc(sizeof(InpatientNode));
    if (!ip) {
        bed->bedStatus = BED_FREE;
        bed->patientId = 0;
        printf("内存分配失败。\n");
        return;
    }
    memset(ip, 0, sizeof(InpatientNode));
    ip->patientId = patient->patientId;
    ip->docId = doctor->docId;
    ip->wardId = bed->wardId;
    ip->bedId = bed->bedId;
    ip->depositTotalCents = deposit;
    ip->depositBalanceCents = deposit;
    ip->dailyFeeCents = getWardDailyFee(bed->wardType);
    ip->totalChargedCents = 0;
    strncpy(ip->admitDate, g_currentDate, MAX_DATE_LEN - 1);
    strncpy(ip->lastChargeDate, g_currentDate, MAX_DATE_LEN - 1);
    ip->admitHour = g_currentHour;
    ip->expectedDays = expectedDays;
    ip->daysStayed = 0;
    ip->isAdmitted = 1;
    appendInpatient(ip);

    createInpatientAdmissionRecord(patient, doctor, bed, deposit, expectedDays);

    printf("住院办理成功：患者[%d] %s 床位=%d 押金余额=",
        patient->patientId, patient->name, bed->bedId);
    printMoney(ip->depositBalanceCents);
    printf("\n");
}

void __admitPatientFromVisit(PatientNode* patient, DoctorNode* doctor) {
    admitNewPatient(patient, doctor);
}

static void listActiveInpatients(void) {
    InpatientNode* ip = g_inpatientHead;
    printf("\n%-6s %-10s %-6s %-6s %-10s %-10s %-8s\n",
        "患者ID", "姓名", "床位", "病房", "押金余额", "已收费", "已住天数");
    printf("---------------------------------------------------------------------\n");
    while (ip) {
        if (!ip->isDeleted && ip->isAdmitted) {
            PatientNode* p = findPatientById(ip->patientId);
            char dep[32], charged[32];
            formatMoney(ip->depositBalanceCents, dep, sizeof(dep));
            formatMoney(ip->totalChargedCents, charged, sizeof(charged));
            printf("%-6d %-10s %-6d %-6d %-10s %-10s %-8d\n",
                ip->patientId, p ? p->name : "未知", ip->bedId, ip->wardId,
                dep, charged, ip->daysStayed);
        }
        ip = ip->next;
    }
}

/* V5 修复 Bug 22 + Bug 23：
 * 1. 用 lastChargeDate 与 currentDate 的天数差计算应补扣天数，
 *    一次跨多日设置时间也能补全所有中间日费用。
 * 2. 同一日二次执行该函数不会重复扣费（天数差为 0）。
 * 这同时为出院结算提供权威的"已扣到哪一天"基准。 */
static int chargeMissingDays(InpatientNode* ip) {
    int days = computeDateDiffDays(ip->lastChargeDate, g_currentDate);
    int i;
    if (days <= 0) return 0;
    for (i = 0; i < days; i++) {
        ip->depositBalanceCents -= ip->dailyFeeCents;
        ip->totalChargedCents += ip->dailyFeeCents;
        g_hospitalRevenue += ip->dailyFeeCents;
        ip->daysStayed += 1;
    }
    strncpy(ip->lastChargeDate, g_currentDate, MAX_DATE_LEN - 1);
    ip->lastChargeDate[MAX_DATE_LEN - 1] = '\0';
    return days;
}

static void runDailyAutoCharge(void) {
    InpatientNode* ip = g_inpatientHead;
    int patientCount = 0;
    int totalDays = 0;
    while (ip) {
        if (!ip->isDeleted && ip->isAdmitted) {
            int days = chargeMissingDays(ip);
            if (days > 0) {
                patientCount++;
                totalDays += days;
                printf("  患者 %d 补扣 %d 天，共 ", ip->patientId, days);
                printMoney((Money)days * ip->dailyFeeCents);
                printf("，押金余额=");
                printMoney(ip->depositBalanceCents);
                printf("\n");
            }
            if (ip->depositBalanceCents < DEPOSIT_SAFE_LINE) {
                printf("  ⚠预警：患者 %d 押金余额低于1000元，当前=", ip->patientId);
                printMoney(ip->depositBalanceCents);
                printf("\n");
            }
        }
        ip = ip->next;
    }
    printf("自动扣费完成，本次处理 %d 名住院患者，共补扣 %d 天。\n",
        patientCount, totalDays);
}

static void topUpDeposit(void) {
    int patientId = safeInputInt("请输入患者ID：");
    Money add = safeInputMoneyFen("请输入补缴押金（元）：");
    InpatientNode* ip = findActiveInpatient(patientId);
    if (!ip || add <= 0 || add % DEPOSIT_UNIT != 0) {
        printf("患者未住院或押金金额非法。\n");
        return;
    }
    ip->depositTotalCents += add;
    ip->depositBalanceCents += add;
    printf("补缴成功，当前押金余额=");
    printMoney(ip->depositBalanceCents);
    printf("\n");
}

static void dischargePatient(void) {
    int patientId = safeInputInt("请输入要出院的患者ID：");
    InpatientNode* ip = findActiveInpatient(patientId);
    PatientNode* patient;
    DoctorNode* doctor;
    Money todayCharge = 0;
    Money refund;
    if (!ip) {
        printf("未找到该住院患者。\n");
        return;
    }

    patient = findPatientById(patientId);
    doctor = findDoctorById(ip->docId);

    /* V5 修复 Bug 22：先把"昨天及以前"应扣的费用全部结清，再按 08:00 规则
     * 决定是否补扣当天。这样无论用户是否事先点过"每日自动扣费"，
     * 出院结算的总费用都一致。 */
    {
        int autoDays = chargeMissingDays(ip);
        if (autoDays > 0) {
            printf("  住院日扣费补全 %d 天，共 ", autoDays);
            printMoney((Money)autoDays * ip->dailyFeeCents);
            printf("\n");
        }
    }
    /* lastChargeDate 现在保证 == g_currentDate（如果之前差 ≥1 天），
     * 但若入院当天就出院、且 g_currentDate == admitDate，差为 0，
     * 仍然由下面的 08:00 规则判断是否收取当天费用。 */
    if (g_currentHour >= 8) {
        /* 检查"今天"是否还没扣过（autoDays 仅补到昨天，所以今天大概率没扣）。
         * 用 daysStayed 与 admitDate→currentDate 天数对比来判断。 */
        int diff = computeDateDiffDays(ip->admitDate, g_currentDate);
        if (ip->daysStayed <= diff) {
            todayCharge = ip->dailyFeeCents;
            ip->depositBalanceCents -= todayCharge;
            ip->totalChargedCents += todayCharge;
            g_hospitalRevenue += todayCharge;
            ip->daysStayed += 1;
            strncpy(ip->lastChargeDate, g_currentDate, MAX_DATE_LEN - 1);
            ip->lastChargeDate[MAX_DATE_LEN - 1] = '\0';
            printf("  08:00后出院，补收当天住院费=");
            printMoney(todayCharge);
            printf("\n");
        }
        else {
            printf("  当天住院费已扣，不重复收取。\n");
        }
    }
    else {
        printf("  08:00前出院，不收取当天住院费。\n");
    }

    refund = ip->depositBalanceCents;

    if (refund < 0) {
        Money needPay = -refund;
        printf("患者仍需补缴：");
        printMoney(needPay);
        printf("\n");
        {
            char yn[8];
            safeInputString("是否从患者账户余额补扣？(y/n)：", yn, 8);
            if ((yn[0] == 'y' || yn[0] == 'Y') && patient && patient->balanceCents >= needPay) {
                patient->balanceCents -= needPay;
                refund = 0;
                /* V5 修复 Bug 26：账户补扣也写日志 */
                {
                    char logBuf[256];
                    snprintf(logBuf, sizeof(logBuf),
                        "[出院账户补扣] 患者ID=%d 姓名=%s 金额=%lld分",
                        patient->patientId, patient->name, (long long)needPay);
                    WriteLog(LOG_OP_FILE, logBuf);
                }
            }
            else {
                Money cashPay = safeInputMoneyFen("请输入现场补缴金额（元）：");
                if (cashPay < needPay) {
                    printf("补缴不足，出院取消。\n");
                    return;
                }
                refund = cashPay - needPay;
                /* V5 修复 Bug 26：现金补缴写审计日志（含找零金额） */
                {
                    char logBuf[256];
                    snprintf(logBuf, sizeof(logBuf),
                        "[出院现金补缴] 患者ID=%d 姓名=%s 应缴=%lld分 实付=%lld分 找零=%lld分",
                        patient ? patient->patientId : 0,
                        patient ? patient->name : "未知",
                        (long long)needPay, (long long)cashPay, (long long)refund);
                    WriteLog(LOG_OP_FILE, logBuf);
                }
            }
        }
    }

    if (patient && refund > 0) {
        patient->balanceCents += refund;
    }

    createSettlementRecord(ip, patient, doctor);
    /* 建议 B 修复：营收已在每日扣费时实时累加，此处不再重复累加 */

    ReleaseBed(ip->bedId);
    ip->isAdmitted = 0;

    printf("出院完成。总住院费用=");
    printMoney(ip->totalChargedCents);
    printf("，退回金额=");
    printMoney(refund > 0 ? refund : 0);
    printf("\n");
}

void ProcessBilling(void) {
    int choice;
    while (1) {
        printf("\n================ 住院与结算模块 ================\n");
        printf("1. 查看当前住院患者\n");
        printf("2. 手动办理住院\n");
        printf("3. 补扣未结算住院费（按日期差自动补全）\n");   /* V5 修复 Bug 29 */
        printf("4. 补缴押金\n");
        printf("5. 出院结算\n");
        //printf("0. 返回上级\n");                                 /* V5 修复 Bug 32 */
        printf("6. 返回上级\n");
        printf("请选择：");
        choice = GetSafeInt();
        switch (choice) {
        case 1:
            listActiveInpatients();
            PauseScreen();
            break;
        case 2: {
            int patientId = safeInputInt("请输入患者ID：");
            int doctorId = safeInputInt("请输入主治医生ID：");
            PatientNode* patient = findPatientById(patientId);
            DoctorNode* doctor = findDoctorById(doctorId);
            if (!patient || !doctor) {
                printf("患者或医生不存在。\n");
            }
            else {
                admitNewPatient(patient, doctor);
            }
            PauseScreen();
            break;
        }
        case 3:
            runDailyAutoCharge();
            PauseScreen();
            break;
        case 4:
            topUpDeposit();
            PauseScreen();
            break;
        case 5:
            dischargePatient();
            PauseScreen();
            break;
        case 0:           /* V5 修复 Bug 32 */
        case 6:
            return;
        default:
            printf("无效选择。\n");
            PauseScreen();
            break;
        }
    }
}