#include "outpatient_inpatient.h"
#include "module_d.h"

static PatientNode* createNewPatient(void) {
    PatientNode* p;
    char name[MAX_NAME_LEN];
    int age;
    int dupCount;

    safeInputString("请输入患者姓名：", name, MAX_NAME_LEN);
    if (strlen(name) == 0) {
        printf("姓名不能为空。\n");
        return NULL;
    }

    /* 修复 Bug 20：建档时提示同名患者数量，并要求确认 */
    dupCount = findPatientsByName(name, NULL, 0);
    if (dupCount > 0) {
        char yn[8];
        printf("系统中已有 %d 名同名患者，仍要继续建档？(y/n)：", dupCount);
        safeInputString("", yn, sizeof(yn));
        if (yn[0] != 'y' && yn[0] != 'Y') {
            printf("建档取消。\n");
            return NULL;
        }
    }

    age = safeInputInt("请输入患者年龄：");
    if (age <= 0 || age > 150) {
        printf("年龄不合法。\n");
        return NULL;
    }

    p = (PatientNode*)malloc(sizeof(PatientNode));
    if (!p) {
        printf("内存分配失败。\n");
        return NULL;
    }
    memset(p, 0, sizeof(PatientNode));
    p->patientId = g_nextPatientId++;
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->age = age;
    p->balanceCents = 0;
    p->isDeleted = 0;
    appendPatient(p);

    printf("建档成功：[%d] %s 年龄:%d\n", p->patientId, p->name, p->age);
    return p;
}

static PatientNode* findExistingPatientMenu(void) {
    int choice;
    while (1) {
        printf("\n1. 按患者ID查找\n2. 按患者姓名查找\n3. 返回上级\n请选择：");
        choice = GetSafeInt();
        if (choice == 1) {
            int id = safeInputInt("请输入患者ID：");
            if (id <= 0) {
                printf("患者ID必须为正整数。\n");
                continue;
            }
            return findPatientById(id);
        }
        else if (choice == 2) {
            char name[MAX_NAME_LEN];
            int id;
            safeInputString("请输入患者姓名：", name, MAX_NAME_LEN);
            if (strlen(name) == 0) {
                printf("姓名不能为空。\n");
                continue;
            }
            id = ConfirmPatientByName(name);
            if (id <= 0) return NULL;
            return findPatientById(id);
        }
        else if (choice == 3) {
            return NULL;
        }
        else {
            printf("无效选择。\n");
        }
    }
}

static void listDoctorsOnDuty(void) {
    DoctorNode* d = g_doctorHead;
    printf("\n今日出诊医生（星期%d）：\n", g_currentWeekday + 1);
    printf("%-6s %-10s %-10s %-12s %-6s\n", "工号", "姓名", "级别", "科室", "已挂号");
    printf("--------------------------------------------------------\n");
    while (d) {
        if (!d->isDeleted && d->schedule[g_currentWeekday]) {
            printf("%-6d %-10s %-10s %-12s %-6d\n",
                d->docId, d->name, d->level, d->department, countDailyDoctorRegs(d->docId));
        }
        d = d->next;
    }
}

static DoctorNode* selectDoctorOnDuty(void) {
    int docId;
    DoctorNode* d;
    listDoctorsOnDuty();
    docId = safeInputInt("请输入要挂号的医生工号：");
    d = findDoctorById(docId);
    if (!d) {
        printf("未找到该医生。\n");
        return NULL;
    }
    if (!d->schedule[g_currentWeekday]) {
        printf("该医生今天不出诊。\n");
        return NULL;
    }
    return d;
}

/* 修复 Bug 08：充值时生成审计记录 + 操作日志，便于事后追查 */
static void writeRechargeAudit(const PatientNode* patient, Money amount) {
    RecordNode* rec;
    char logMsg[256];
    if (!patient || amount <= 0) return;

    rec = (RecordNode*)malloc(sizeof(RecordNode));
    if (rec) {
        memset(rec, 0, sizeof(RecordNode));
        generateRecordId(rec->recordId);
        rec->patientId = patient->patientId;
        strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
        rec->docId = 0;
        strncpy(rec->docName, "前台", MAX_NAME_LEN - 1);
        rec->type = REC_TYPE_RECHARGE;
        strncpy(rec->diagnosis, "账户充值", MAX_DIAG_LEN - 1);
        rec->checkFeeCents = 0;
        rec->medId = 0;
        rec->medCount = 0;
        rec->totalCostCents = amount;
        rec->status = 0;
        strncpy(rec->date, g_currentDate, MAX_DATE_LEN - 1);
        rec->hour = g_currentHour;
        rec->minute = g_currentMinute;
        appendRecord(rec);
    }

    snprintf(logMsg, sizeof(logMsg),
        "[充值] 患者ID=%d 姓名=%s 金额=%lld分 余额=%lld分",
        patient->patientId, patient->name,
        (long long)amount, (long long)patient->balanceCents);
    WriteLog(LOG_OP_FILE, logMsg);
}

static int rechargeForPatient(PatientNode* patient, const char* prompt) {
    Money amount;
    if (!patient) {
        printf("患者不存在。\n");
        return 0;
    }
    amount = safeInputMoneyFen(prompt ? prompt : "请输入充值金额（元）：");
    if (amount <= 0) {
        printf("充值金额必须大于0。\n");
        return 0;
    }
    patient->balanceCents += amount;
    writeRechargeAudit(patient, amount);   /* 修复 Bug 08 */
    printf("充值成功。当前余额=");
    printMoney(patient->balanceCents);
    printf("\n");
    return 1;
}

static void rechargeEntry(void) {
    PatientNode* patient = findExistingPatientMenu();
    if (!patient) {
        printf("患者未确定，充值取消。\n");
        return;
    }
    rechargeForPatient(patient, "请输入充值金额（元）：");
}

static void balanceQueryEntry(void) {
    PatientNode* patient = findExistingPatientMenu();
    if (!patient) {
        printf("患者未确定，查询取消。\n");
        return;
    }
    printf("患者：[%d] %s 当前余额=", patient->patientId, patient->name);
    printMoney(patient->balanceCents);
    printf("\n");
}

/* V5 修复 Bug 25：移除 static，让 billing.cpp 也能使用 */
int ensureEnoughBalance(PatientNode* patient, Money need, const char* scene) {
    char yn[8];
    if (!patient) return 0;
    if (patient->balanceCents >= need) return 1;

    printf("当前余额不足，%s需要 ", scene ? scene : "本次操作");
    printMoney(need);
    printf("，当前余额=");
    printMoney(patient->balanceCents);
    printf("\n");
    safeInputString("是否立即充值？(y/n)：", yn, 8);
    if (yn[0] == 'y' || yn[0] == 'Y') {
        if (!rechargeForPatient(patient, "请输入充值金额（元）：")) return 0;
    }
    if (patient->balanceCents < need) {
        printf("充值后余额仍不足，本次操作取消。\n");
        return 0;
    }
    return 1;
}

static void doRegistrationForPatient(PatientNode* patient) {
    DoctorNode* doctor = NULL;
    RecordNode* rec = NULL;
    char regId[MAX_REG_ID_LEN];
    Money regFee;

    if (!patient) {
        printf("患者未确定，挂号取消。\n");
        return;
    }

    if (countDailyHospitalRegs() >= DAILY_MAX_TICKETS) {
        printf("今日全院挂号已达上限 %d。\n", DAILY_MAX_TICKETS);
        return;
    }
    if (countDailyPatientRegs(patient->patientId) >= DAILY_MAX_PER_PATIENT) {
        printf("该患者今日挂号已达上限 %d。\n", DAILY_MAX_PER_PATIENT);
        return;
    }

    doctor = selectDoctorOnDuty();
    if (!doctor) return;

    if (countDailyDoctorRegs(doctor->docId) >= DAILY_MAX_PER_DOCTOR) {
        printf("该医生今日挂号已达上限 %d。\n", DAILY_MAX_PER_DOCTOR);
        return;
    }
    if (countDailyPatientDeptRegs(patient->patientId, doctor->department) >= DAILY_MAX_PER_DEPT) {
        printf("同一患者同一天同一科室只能挂 1 个号。\n");
        return;
    }

    regFee = isExpertDoctor(doctor) ? REG_FEE_EXPERT : REG_FEE_NORMAL;
    if (!ensureEnoughBalance(patient, regFee, "挂号缴费")) {
        return;
    }
    patient->balanceCents -= regFee;
    g_hospitalRevenue += regFee;            /* 修复 Bug 02：挂号费计入营业额 */

    generateRegId(regId);
    strncpy(patient->lastRegId, regId, MAX_REG_ID_LEN - 1);

    rec = (RecordNode*)malloc(sizeof(RecordNode));
    if (!rec) {
        printf("内存分配失败。\n");
        patient->balanceCents += regFee;
        g_hospitalRevenue -= regFee;        /* 同步回滚营业额 */
        return;
    }
    memset(rec, 0, sizeof(RecordNode));
    generateRecordId(rec->recordId);
    rec->patientId = patient->patientId;
    strncpy(rec->patientName, patient->name, MAX_NAME_LEN - 1);
    rec->docId = doctor->docId;
    strncpy(rec->docName, doctor->name, MAX_NAME_LEN - 1);
    rec->type = REC_TYPE_REGISTER;
    strncpy(rec->diagnosis, "门诊挂号", MAX_DIAG_LEN - 1);
    rec->checkFeeCents = regFee;
    rec->totalCostCents = regFee;
    rec->status = 0;
    strncpy(rec->date, g_currentDate, MAX_DATE_LEN - 1);
    rec->hour = g_currentHour;
    rec->minute = g_currentMinute;
    appendRecord(rec);

    doctor->currentLoad++;

    printf("\n挂号成功！\n");
    printf("挂号记录ID：%s\n", rec->recordId);
    printf("挂号号：%s\n", patient->lastRegId);
    printf("患者：[%d] %s\n", patient->patientId, patient->name);
    printf("医生：[%d] %s %s %s\n", doctor->docId, doctor->name, doctor->level, doctor->department);
    printf("挂号费：");
    printMoney(regFee);
    printf("\n扣费后余额：");
    printMoney(patient->balanceCents);
    printf("\n");
}

void RegisterPatient(void) {
    while (1) {
        PatientNode* patient = NULL;
        printf("\n================ 挂号与充值模块 ================\n");
        printf("1. 新患者建档并挂号\n");
        printf("2. 旧患者直接挂号\n");
        printf("3. 充值入口\n");
        printf("4. 余额查询\n");
        printf("5. 返回上级\n");
        printf("请选择：");

        switch (GetSafeInt()) {
        case 1:
            patient = createNewPatient();
            if (patient) doRegistrationForPatient(patient);
            return;
        case 2:
            patient = findExistingPatientMenu();
            if (patient) doRegistrationForPatient(patient);
            else printf("患者未确定，挂号取消。\n");
            return;
        case 3:
            /* V5 修复 Bug 24：充值/余额查询完成后留在子菜单，
             * 方便连续操作（先充值再挂号）。 */
            rechargeEntry();
            PauseScreen();
            break;
        case 4:
            balanceQueryEntry();
            PauseScreen();
            break;
        case 0:           /* V5 修复 Bug 32：0 也接受为"返回上级" */
        case 5:
            return;
        default:
            printf("无效选择。\n");
            PauseScreen();
            break;
        }
    }
}
