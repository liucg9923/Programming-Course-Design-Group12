#include "module_d.h"
#include "pharmacy.h"

static const char* getRecordTypeName(int type) {
    switch (type) {
    case REC_TYPE_REGISTER: return "挂号";
    case REC_TYPE_VISIT: return "看诊";
    case REC_TYPE_EXAM: return "检查";
    case REC_TYPE_PRESCRIBE: return "开药";
    case REC_TYPE_INPATIENT: return "住院";
    case REC_TYPE_SETTLEMENT: return "结算";
    case REC_TYPE_RECHARGE: return "充值";    /* 修复 Bug 08 */
    default: return "未知";
    }
}

void PrintRecord(const RecordNode* r) {
    printf("------------------------------------------------------------\n");
    printf("记录ID: %s | 类型: %s | 日期: %s %02d:%02d\n",
        r->recordId, getRecordTypeName(r->type), r->date, r->hour, r->minute);
    printf("患者: [%d] %s | 医生: [%d] %s\n",
        r->patientId, r->patientName, r->docId, r->docName);
    printf("诊断/说明: %s\n", r->diagnosis);
    printf("检查费: "); printMoney(r->checkFeeCents);
    printf(" | 药品ID: %d | 数量: %d | 总额: ", r->medId, r->medCount);
    printMoney(r->totalCostCents);
    printf("\n病房ID: %d | 床位ID: %d | 状态: %d | 撤销:%d | 删除:%d\n",
        r->wardId, r->bedId, r->status, r->isRedInk, r->isDeleted);
}

int ConfirmPatientByName(const char* name) {
    PatientNode* p = g_patientHead;
    int matched = 0;
    while (p) {
        if (!p->isDeleted && strcmp(p->name, name) == 0) {
            printf("  [%d] %s 年龄:%d 余额:", p->patientId, p->name, p->age);
            printMoney(p->balanceCents);
            printf("\n");
            matched++;
        }
        p = p->next;
    }
    if (matched == 0) return -1;
    printf("请输入要选择的患者ID（0取消）：");
    return GetSafeInt();
}

int ConfirmDoctorByName(const char* name) {
    DoctorNode* d = g_doctorHead;
    int matched = 0;
    while (d) {
        if (!d->isDeleted && strcmp(d->name, name) == 0) {
            printf("  [%d] %s %s %s 负载:%d\n",
                d->docId, d->name, d->level, d->department, d->currentLoad);
            matched++;
        }
        d = d->next;
    }
    if (matched == 0) return -1;
    printf("请输入要选择的医生ID（0取消）：");
    return GetSafeInt();
}

void DeleteRecord(void) {
    char recordId[MAX_REC_ID_LEN];
    RecordNode* r = g_recordHead;
    safeInputString("请输入要软删除的记录ID：", recordId, MAX_REC_ID_LEN);
    while (r) {
        if (strcmp(r->recordId, recordId) == 0 && !r->isDeleted) {
            r->isDeleted = 1;
            WriteLog(LOG_DEL_FILE, "软删除一条诊疗记录");
            printf("已软删除记录 %s\n", recordId);
            return;
        }
        r = r->next;
    }
    printf("未找到可删除记录。\n");
}

/* V5 修复 Bug 33：恢复软删除记录 */
void RestoreDeletedRecord(void) {
    char recordId[MAX_REC_ID_LEN];
    RecordNode* r = g_recordHead;
    safeInputString("请输入要恢复的记录ID：", recordId, MAX_REC_ID_LEN);
    while (r) {
        if (strcmp(r->recordId, recordId) == 0 && r->isDeleted) {
            r->isDeleted = 0;
            WriteLog(LOG_DEL_FILE, "恢复一条软删除诊疗记录");
            printf("已恢复记录 %s\n", recordId);
            return;
        }
        r = r->next;
    }
    printf("未找到该 ID 的已删除记录。\n");
}

static void searchByPatient(void) {
    int choice;
    PatientNode* p = NULL;
    printf("\n1. 按患者ID  2. 按患者姓名\n请选择：");
    choice = GetSafeInt();
    if (choice == 1) {
        int id = safeInputInt("请输入患者ID：");
        p = findPatientById(id);
    }
    else if (choice == 2) {
        char name[MAX_NAME_LEN];
        int id;
        safeInputString("请输入患者姓名：", name, MAX_NAME_LEN);
        id = ConfirmPatientByName(name);
        if (id > 0) p = findPatientById(id);
    }
    if (!p) {
        printf("未找到患者。\n");
        return;
    }

    printf("\n患者信息：[%d] %s 年龄:%d 账户余额:", p->patientId, p->name, p->age);
    printMoney(p->balanceCents);
    printf("\n\n相关记录：\n");
    {
        RecordNode* r = g_recordHead;
        int found = 0;
        while (r) {
            if (!r->isDeleted && r->patientId == p->patientId) {
                PrintRecord(r);
                found = 1;
            }
            r = r->next;
        }
        if (!found) printf("暂无记录。\n");
    }
    {
        InpatientNode* ip = findActiveInpatient(p->patientId);
        if (ip) {
            printf("\n当前住院：床位 %d，押金余额 ", ip->bedId);
            printMoney(ip->depositBalanceCents);
            printf("，已住 %d 天\n", ip->daysStayed);
        }
    }
}

static void searchByDoctor(void) {
    int choice;
    DoctorNode* d = NULL;
    printf("\n1. 按医生ID  2. 按医生姓名\n请选择：");
    choice = GetSafeInt();
    if (choice == 1) {
        int id = safeInputInt("请输入医生ID：");
        d = findDoctorById(id);
    }
    else if (choice == 2) {
        char name[MAX_NAME_LEN];
        int id;
        safeInputString("请输入医生姓名：", name, MAX_NAME_LEN);
        id = ConfirmDoctorByName(name);
        if (id > 0) d = findDoctorById(id);
    }
    if (!d) {
        printf("未找到医生。\n");
        return;
    }

    printf("\n医生信息：[%d] %s %s %s 当前负载:%d\n",
        d->docId, d->name, d->level, d->department, d->currentLoad);
    printf("出诊：%d %d %d %d %d %d %d\n",
        d->schedule[0], d->schedule[1], d->schedule[2], d->schedule[3],
        d->schedule[4], d->schedule[5], d->schedule[6]);

    {
        RecordNode* r = g_recordHead;
        int found = 0;
        while (r) {
            if (!r->isDeleted && r->docId == d->docId) {
                PrintRecord(r);
                found = 1;
            }
            r = r->next;
        }
        if (!found) printf("该医生暂无记录。\n");
    }
}

static void searchByDepartment(void) {
    char dept[MAX_DEPT_LEN];
    DoctorNode* d;
    MedicineNode* m;
    BedNode* b;
    RecordNode* r;

    safeInputString("请输入科室名称：", dept, MAX_DEPT_LEN);
    printf("\n科室[%s] 医生：\n", dept);
    d = g_doctorHead;
    while (d) {
        if (!d->isDeleted && strcmp(d->department, dept) == 0) {
            printf("  [%d] %s %s 负载:%d\n", d->docId, d->name, d->level, d->currentLoad);
        }
        d = d->next;
    }

    printf("\n关联药品：\n");
    m = g_medicineHead;
    while (m) {
        if (!m->isDeleted && strcmp(m->relatedDept, dept) == 0) {
            printf("  [%d] %s/%s 库存:%d 单价:", m->medId, m->officialName, m->tradeName, m->stock);
            printMoney(m->priceCents);
            printf("\n");
        }
        m = m->next;
    }

    printf("\n关联床位：\n");
    b = g_bedHead;
    while (b) {
        if (!b->isDeleted && strcmp(b->relatedDept, dept) == 0) {
            printf("  床位:%d 病房:%d 类型:%s 状态:%d 当前患者:%d\n",
                b->bedId, b->wardId, getWardTypeName(b->wardType), b->bedStatus, b->patientId);
        }
        b = b->next;
    }

    printf("\n科室业务记录：\n");
    r = g_recordHead;
    while (r) {
        DoctorNode* doc = findDoctorById(r->docId);
        if (!r->isDeleted && doc && strcmp(doc->department, dept) == 0) {
            PrintRecord(r);
        }
        r = r->next;
    }
}

static void searchByMedicine(void) {
    char keyword[MAX_MED_NAME_LEN];
    MedicineNode* m = g_medicineHead;
    int found = 0;
    safeInputString("请输入药品ID或关键字：", keyword, MAX_MED_NAME_LEN);

    printf("\n匹配药品：\n");
    while (m) {
        char idBuf[32];
        sprintf(idBuf, "%d", m->medId);
        if (!m->isDeleted &&
            (strcmp(idBuf, keyword) == 0 ||
                strContains(m->officialName, keyword) ||
                strContains(m->tradeName, keyword) ||
                strContains(m->aliasName, keyword))) {
            printf("  [%d] %s / %s / %s 库存:%d 单价:",
                m->medId, m->officialName, m->tradeName, m->aliasName, m->stock);
            printMoney(m->priceCents);
            printf("  科室:%s\n", m->relatedDept);
            found = 1;
        }
        m = m->next;
    }
    if (!found) {
        printf("无匹配药品。\n");
        return;
    }

    printf("\n涉及该药品的记录：\n");
    {
        RecordNode* r = g_recordHead;
        int any = 0;
        while (r) {
            MedicineNode* med = findMedicineById(r->medId);
            if (!r->isDeleted && med &&
                (strContains(med->officialName, keyword) ||
                    strContains(med->tradeName, keyword) ||
                    strContains(med->aliasName, keyword) ||
                    med->medId == atoi(keyword))) {
                PrintRecord(r);
                any = 1;
            }
            r = r->next;
        }
        if (!any) printf("暂无相关记录。\n");
    }
}

void SearchModule(void) {
    int choice;
    while (1) {
        printf("\n================ 信息查询模块 ================\n");
        printf("1. 按患者查询\n");
        printf("2. 按医生查询\n");
        printf("3. 按科室查询\n");
        printf("4. 按药品查询\n");
        printf("5. 软删除记录\n");
        printf("6. 恢复软删除记录\n");      /* V5 修复 Bug 33 */
        //printf("0. 返回上级\n");                       /* V5 修复 Bug 32 */
        printf("7. 返回上级\n");
        printf("请选择：");
        choice = GetSafeInt();
        switch (choice) {
        case 1: searchByPatient(); PauseScreen(); break;
        case 2: searchByDoctor(); PauseScreen(); break;
        case 3: searchByDepartment(); PauseScreen(); break;
        case 4: searchByMedicine(); PauseScreen(); break;
        case 5: DeleteRecord(); PauseScreen(); break;
        case 6: RestoreDeletedRecord(); PauseScreen(); break;   /* V5 */
        case 0:                /* V5 修复 Bug 32 */
        case 7: return;
        default: printf("无效选择。\n"); PauseScreen(); break;
        }
    }
}
