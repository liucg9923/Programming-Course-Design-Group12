#include "global.h"
#include "module_d.h"
#include "pharmacy.h"
#include "ward.h"
#include <locale.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

static void syncSystemTimeFromOS(void) {
    time_t now;
    struct tm* tmInfo;
    time(&now);
    tmInfo = localtime(&now);
    if (!tmInfo) return;

    snprintf(g_currentDate, MAX_DATE_LEN, "%04d%02d%02d",
        tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday);
    g_currentHour = tmInfo->tm_hour;
    g_currentMinute = tmInfo->tm_min;
    g_currentWeekday = (tmInfo->tm_wday + 6) % 7;
}

static int isLeapYear(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static int isValidDateString(const char* dateBuf) {
    int year, month, day;
    int daysInMonth[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (!dateBuf || strlen(dateBuf) != 8) return 0;
    for (int i = 0; i < 8; ++i) {
        if (dateBuf[i] < '0' || dateBuf[i] > '9') return 0;
    }
    year = (dateBuf[0] - '0') * 1000 + (dateBuf[1] - '0') * 100 + (dateBuf[2] - '0') * 10 + (dateBuf[3] - '0');
    month = (dateBuf[4] - '0') * 10 + (dateBuf[5] - '0');
    day = (dateBuf[6] - '0') * 10 + (dateBuf[7] - '0');
    if (year < 2000 || year > 2099) return 0;
    if (month < 1 || month > 12) return 0;
    if (isLeapYear(year)) daysInMonth[1] = 29;
    return day >= 1 && day <= daysInMonth[month - 1];
}

static int inputRangedInt(const char* prompt, int minValue, int maxValue) {
    while (1) {
        int value = safeInputInt(prompt);
        if (value >= minValue && value <= maxValue) return value;
        printf("输入超出范围，请重新输入。\n");
    }
}

static void setSystemTime(void) {
    char dateBuf[MAX_DATE_LEN];
    while (1) {
        safeInputString("请输入当前日期(YYYYMMDD)：", dateBuf, MAX_DATE_LEN);
        if (isValidDateString(dateBuf)) break;
        printf("日期格式或日期值不合法，请重新输入。\n");
    }
    g_currentHour = inputRangedInt("请输入当前小时(0-23)：", 0, 23);
    g_currentMinute = inputRangedInt("请输入当前分钟(0-59)：", 0, 59);
    g_currentWeekday = inputRangedInt("请输入星期(0=周一..6=周日)：", 0, 6);
    strncpy(g_currentDate, dateBuf, MAX_DATE_LEN - 1);
    g_currentDate[MAX_DATE_LEN - 1] = '\0';

    printf("系统时间已更新为 %s %02d:%02d 星期%d\n",
        g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday + 1);
}

static void SmartSuggestDemo(void) {
    char keyword[MAX_MED_NAME_LEN];
    DoctorNode* d;
    MedicineNode* m;
    int found = 0;

    safeInputString("请输入医生姓名/药品关键字：", keyword, MAX_MED_NAME_LEN);
    printf("\n医生联想结果：\n");
    d = g_doctorHead;
    while (d) {
        if (!d->isDeleted && (strContains(d->name, keyword) || strContains(d->department, keyword))) {
            printf("  [%d] %s %s %s\n", d->docId, d->name, d->level, d->department);
            found = 1;
        }
        d = d->next;
    }
    if (!found) printf("  无匹配医生。\n");

    found = 0;
    printf("\n药品联想结果：\n");
    m = g_medicineHead;
    while (m) {
        if (!m->isDeleted &&
            (strContains(m->officialName, keyword) ||
                strContains(m->tradeName, keyword) ||
                strContains(m->aliasName, keyword))) {
            printf("  [%d] %s / %s / %s\n", m->medId, m->officialName, m->tradeName, m->aliasName);
            found = 1;
        }
        m = m->next;
    }
    if (!found) printf("  无匹配药品。\n");
}

typedef struct DoctorRankNode {
    DoctorNode* doctor;
    int businessCount;
    struct DoctorRankNode* next;
} DoctorRankNode;

static void freeDoctorRankList(DoctorRankNode* head) {
    while (head) {
        DoctorRankNode* t = head;
        head = head->next;
        free(t);
    }
}

static void printHospitalRevenueByRule(void) {
    RecordNode* r = g_recordHead;
    Money reg = 0, exam = 0, med = 0, settle = 0;
    while (r) {
        if (!r->isDeleted && !r->isRedInk) {
            if (r->type == REC_TYPE_REGISTER) reg += r->totalCostCents;        /* 修复 Bug 03 */
            else if (r->type == REC_TYPE_EXAM) exam += r->totalCostCents;
            else if (r->type == REC_TYPE_PRESCRIBE) med += r->totalCostCents;
            else if (r->type == REC_TYPE_SETTLEMENT) settle += r->totalCostCents;
        }
        r = r->next;
    }
    printf("挂号收入："); printMoney(reg); printf("\n");                        /* 修复 Bug 03 */
    printf("检查收入："); printMoney(exam); printf("\n");
    printf("药费收入："); printMoney(med); printf("\n");
    printf("已结算住院费："); printMoney(settle); printf("\n");
    printf("医院总营业额："); printMoney(reg + exam + med + settle); printf("\n");
}

static void showManagementStats(void) {
    DepartmentNode* dep;
    int totalFree, totalOccupied, totalMaint, nFree, iFree, vFree;

    printf("\n========== 管理视角统计 ==========\n");
    printf("医生数：%d  患者数：%d  药品种类：%d  诊疗记录：%d\n",
        getDoctorCount(), getPatientCount(), getMedicineCount(), getRecordCount());
    printHospitalRevenueByRule();

    GetBedStatistics(&totalFree, &totalOccupied, &totalMaint, &nFree, &iFree, &vFree);
    printf("\n床位统计：空闲=%d 占用=%d 维护=%d\n", totalFree, totalOccupied, totalMaint);
    printf("普通空闲=%d ICU空闲=%d VIP空闲=%d\n", nFree, iFree, vFree);

    printf("\n当前住院患者：\n");
    {
        InpatientNode* ip = g_inpatientHead;
        int found = 0;
        while (ip) {
            if (!ip->isDeleted && ip->isAdmitted) {
                PatientNode* p = findPatientById(ip->patientId);
                printf("  [%d] %s 床位:%d 押金余额:", ip->patientId, p ? p->name : "未知", ip->bedId);
                printMoney(ip->depositBalanceCents);
                printf(" 已住:%d天\n", ip->daysStayed);
                found = 1;
            }
            ip = ip->next;
        }
        if (!found) printf("  暂无当前住院患者数据。\n");
    }

    printf("\n各科室业务量：\n");
    dep = g_departmentHead;
    while (dep) {
        int count = 0;
        RecordNode* r = g_recordHead;
        while (r) {
            DoctorNode* doc = findDoctorById(r->docId);
            if (!r->isDeleted && !r->isRedInk && doc && strcmp(doc->department, dep->deptName) == 0) {
                count++;
            }
            r = r->next;
        }
        printf("  %-12s %d\n", dep->deptName, count);
        dep = dep->next;
    }
}

static void showStaffStats(void) {
    DoctorNode* d = g_doctorHead;
    DoctorRankNode* rankHead = NULL;

    while (d) {
        if (!d->isDeleted) {
            int businessCount = 0;
            RecordNode* r = g_recordHead;
            DoctorRankNode* node, ** pp;
            while (r) {
                if (!r->isDeleted && !r->isRedInk && r->docId == d->docId &&
                    r->type != REC_TYPE_REGISTER) {
                    businessCount++;
                }
                r = r->next;
            }
            node = (DoctorRankNode*)malloc(sizeof(DoctorRankNode));
            if (node) {
                node->doctor = d;
                node->businessCount = businessCount;
                node->next = NULL;
                pp = &rankHead;
                while (*pp && ((*pp)->businessCount > businessCount ||
                    ((*pp)->businessCount == businessCount &&
                        (*pp)->doctor->currentLoad >= d->currentLoad))) {
                    pp = &((*pp)->next);
                }
                node->next = *pp;
                *pp = node;
            }
        }
        d = d->next;
    }

    printf("\n========== 医护视角统计 ==========\n");
    printf("医生繁忙度排行（按业务记录数）：\n");
    {
        DoctorRankNode* p = rankHead;
        int rank = 1;
        while (p && rank <= 15) {
            printf("%2d. [%d] %-10s %-10s 业务记录:%d 当前负载:%d\n",
                rank, p->doctor->docId, p->doctor->name, p->doctor->department,
                p->businessCount, p->doctor->currentLoad);
            p = p->next;
            rank++;
        }
    }

    printf("\n当前住院患者按主治医生汇总：\n");
    d = g_doctorHead;
    {
        int hasAny = 0;
        while (d) {
            if (!d->isDeleted) {
                int count = 0;
                InpatientNode* ip = g_inpatientHead;
                while (ip) {
                    if (!ip->isDeleted && ip->isAdmitted && ip->docId == d->docId) count++;
                    ip = ip->next;
                }
                if (count > 0) {
                    printf("  [%d] %s %s 当前在院患者:%d\n", d->docId, d->name, d->department, count);
                    hasAny = 1;
                }
            }
            d = d->next;
        }
        if (!hasAny) printf("  暂无当前住院患者数据。\n");
    }

    freeDoctorRankList(rankHead);
}

static PatientNode* selectPatientForStats(void) {
    while (1) {
        int choice;
        PatientNode* p = NULL;
        printf("\n1. 按患者ID查询\n2. 按患者姓名查询\n3. 返回上级\n请选择：");
        choice = GetSafeInt();
        if (choice == 1) {
            int id = safeInputInt("请输入患者ID：");
            if (id <= 0) {
                printf("患者ID必须为正整数。\n");
                PauseScreen();
                continue;
            }
            p = findPatientById(id);
            if (!p) {
                printf("未找到患者。\n");
                PauseScreen();
                continue;
            }
            return p;
        }
        else if (choice == 2) {
            char name[MAX_NAME_LEN];
            int id;
            safeInputString("请输入患者姓名：", name, MAX_NAME_LEN);
            if (strlen(name) == 0) {
                printf("姓名不能为空。\n");
                PauseScreen();
                continue;
            }
            id = ConfirmPatientByName(name);
            if (id <= 0) {
                printf("未找到匹配患者或已取消。\n");
                PauseScreen();
                continue;
            }
            p = findPatientById(id);
            if (!p) {
                printf("未找到患者。\n");
                PauseScreen();
                continue;
            }
            return p;
        }
        else if (choice == 3) {
            return NULL;
        }
        else {
            printf("无效选择。\n");
            PauseScreen();
        }
    }
}

static void showPatientStats(void) {
    PatientNode* p = selectPatientForStats();
    Money total = 0;
    if (!p) return;

    printf("\n========== 患者视角统计 ==========\n");
    printf("患者：[%d] %s 年龄:%d 账户余额:", p->patientId, p->name, p->age);
    printMoney(p->balanceCents);
    printf("\n历史消费清单：\n");

    {
        RecordNode* r = g_recordHead;
        int found = 0;
        while (r) {
            if (!r->isDeleted && !r->isRedInk && r->patientId == p->patientId) {
                PrintRecord(r);
                if (r->type == REC_TYPE_EXAM || r->type == REC_TYPE_PRESCRIBE || r->type == REC_TYPE_SETTLEMENT) {
                    total += r->totalCostCents;
                }
                found = 1;
            }
            r = r->next;
        }
        if (!found) printf("暂无历史记录。\n");
    }
    printf("历史已确认消费合计：");
    printMoney(total);
    printf("\n");

    {
        InpatientNode* ip = findActiveInpatient(p->patientId);
        if (ip) {
            printf("当前住院费用：已扣=");
            printMoney(ip->totalChargedCents);
            printf("，押金余额=");
            printMoney(ip->depositBalanceCents);
            printf("，已住%d天\n", ip->daysStayed);
        }
        else {
            printf("当前无住院信息。\n");
        }
    }
}

void StatisticModule(void) {
    int choice;
    while (1) {
        printf("\n================ 统计报表模块 ================\n");
        printf("1. 管理视角报表\n");
        printf("2. 医护视角报表\n");
        printf("3. 患者视角报表\n");
        printf("4. 返回上级\n");
        printf("请选择：");
        choice = GetSafeInt();
        switch (choice) {
        case 1: showManagementStats(); PauseScreen(); break;
        case 2: showStaffStats(); PauseScreen(); break;
        case 3: showPatientStats(); PauseScreen(); break;
        case 4: return;
        default: printf("无效选择。\n"); PauseScreen(); break;
        }
    }
}

int main(void) {
#ifdef _WIN32
    system("chcp 65001 > nul");
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    setlocale(LC_ALL, ".65001");
#else
    setlocale(LC_ALL, "");
#endif

    LoadAllData();
    initDefaultDepartmentsAndBeds();
    /* 修复 Bug 10：仅当持久化文件中没有有效日期时才用 OS 时间，
     * 否则保留用户上次"设置系统时间"的设置，便于演示模拟时间相关业务。 */
    if (!isValidDateString(g_currentDate)) {
        syncSystemTimeFromOS();
        printf("未检测到有效系统时间，已自动同步操作系统时间：%s %02d:%02d 星期%d\n",
            g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday + 1);
    }
    else {
        printf("已加载系统时间：%s %02d:%02d 星期%d\n",
            g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday + 1);
    }
    printf("数据加载完成：医生=%d 患者=%d 药品=%d 记录=%d 床位=%d\n",
        getDoctorCount(), getPatientCount(), getMedicineCount(), getRecordCount(), getBedCount());
    PauseScreen();

    while (1) {
        int choice;
        clearScreen();
        printf("====================================================\n");
        printf("    医疗管理系统HIS(Hospital Information System)    \n");
        printf("====================================================\n");
        printf("当前系统时间：%s %02d:%02d 星期%d\n",
            g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday + 1);
        printf("1. 挂号与充值\n");
        printf("2. 门诊诊室\n");
        printf("3. 住院管理\n");
        printf("4. 药房管理\n");
        printf("5. 病房床位\n");
        printf("6. 信息查询\n");
        printf("7. 统计报表\n");
        printf("8. 智能联想\n");
        printf("9. 设置系统时间\n");
        printf("10. 保存并退出\n");
        printf("请选择操作：");
        choice = GetSafeInt();

        switch (choice) {
        case 1: RegisterPatient(); PauseScreen(); break;
        case 2: SeeDoctor(); break;
        case 3: ProcessBilling(); break;
        case 4: PharmacyManagement(); break;
        case 5: ShowWardStatus(); break;
        case 6: SearchModule(); break;
        case 7: StatisticModule(); break;
        case 8: SmartSuggestDemo(); PauseScreen(); break;
        case 9: setSystemTime(); PauseScreen(); break;
        case 10:
            SaveAllData();
            printf("数据已保存，系统退出。\n");
            releaseAllData();
            return 0;
        default:
            printf("无效选择。\n");
            PauseScreen();
            break;
        }
    }
}