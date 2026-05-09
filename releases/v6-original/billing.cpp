#include "outpatient_inpatient.h"
#include "ward.h"
#include "module_d.h"   /* 修复 Bug 09：使用 WriteLog 记录现金押金操作 */

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
            if (patient->balanceCents < deposit) {
                printf("账户余额不足。\n");
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

static void runDailyAutoCharge(void) {
    InpatientNode* ip = g_inpatientHead;
    int charged = 0;
    while (ip) {
        if (!ip->isDeleted && ip->isAdmitted) {
            if (strcmp(ip->lastChargeDate, g_currentDate) != 0) {
                ip->depositBalanceCents -= ip->dailyFeeCents;
                ip->totalChargedCents += ip->dailyFeeCents;
                g_hospitalRevenue += ip->dailyFeeCents;   /* 建议 B 修复：实时入营收 */
                ip->daysStayed += 1;
                strncpy(ip->lastChargeDate, g_currentDate, MAX_DATE_LEN - 1);
                charged++;
            }
            if (ip->depositBalanceCents < DEPOSIT_SAFE_LINE) {
                printf("预警：患者 %d 押金余额低于1000元，当前=", ip->patientId);
                printMoney(ip->depositBalanceCents);
                printf("\n");
            }
        }
        ip = ip->next;
    }
    printf("自动扣费完成，本次处理 %d 名住院患者。\n", charged);
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

    if (g_currentHour >= 8 && strcmp(ip->lastChargeDate, g_currentDate) != 0) {
        todayCharge = ip->dailyFeeCents;
        ip->depositBalanceCents -= todayCharge;
        ip->totalChargedCents += todayCharge;
        g_hospitalRevenue += todayCharge;                        /* 建议 B：出院当天实时入营收 */
        ip->daysStayed += 1;
        strncpy(ip->lastChargeDate, g_currentDate, MAX_DATE_LEN - 1);
        printf("08:00后出院，补收当天住院费=");
        printMoney(todayCharge);
        printf("\n");
    }
    else if (g_currentHour < 8) {
        printf("08:00前出院，不收取当天住院费。\n");
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
            }
            else {
                Money cashPay = safeInputMoneyFen("请输入现场补缴金额（元）：");
                if (cashPay < needPay) {
                    printf("补缴不足，出院取消。\n");
                    return;
                }
                refund = cashPay - needPay;
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
        printf("3. 执行每日08:00自动扣费\n");
        printf("4. 补缴押金\n");
        printf("5. 出院结算\n");
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
        case 6:
            return;
        default:
            printf("无效选择。\n");
            PauseScreen();
            break;
        }
    }
}