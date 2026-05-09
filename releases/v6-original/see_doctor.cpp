#include "outpatient_inpatient.h"
#include "pharmacy.h"

extern void __admitPatientFromVisit(PatientNode* patient, DoctorNode* doctor);

static RecordNode* findRecordByIdInternal(const char* recordId) {
    RecordNode* r = g_recordHead;
    while (r) {
        if (strcmp(r->recordId, recordId) == 0) return r;
        r = r->next;
    }
    return NULL;
}

static RecordNode* selectRegisteredPatient(void) {
    RecordNode* r = g_recordHead;
    int found = 0;
    printf("\n今日待诊挂号记录：\n");
    printf("%-16s %-6s %-10s %-6s %-10s %-12s\n",
        "记录ID", "患者ID", "患者姓名", "医生ID", "医生姓名", "科室");
    printf("-----------------------------------------------------------------------\n");
    while (r) {
        if (!r->isDeleted && !r->isRedInk &&
            r->type == REC_TYPE_REGISTER &&
            r->status == 0 &&
            strcmp(r->date, g_currentDate) == 0) {
            DoctorNode* doc = findDoctorById(r->docId);
            printf("%-16s %-6d %-10s %-6d %-10s %-12s\n",
                r->recordId, r->patientId, r->patientName,
                r->docId, r->docName, doc ? doc->department : "未知");
            found = 1;
        }
        r = r->next;
    }
    if (!found) {
        printf("今天暂无待诊患者。\n");
        return NULL;
    }
    {
        char recordId[MAX_REC_ID_LEN];
        safeInputString("请输入要接诊的挂号记录ID：", recordId, MAX_REC_ID_LEN);
        r = findRecordByIdInternal(recordId);
        if (!r || r->type != REC_TYPE_REGISTER || r->status != 0 || r->isDeleted || r->isRedInk) {
            printf("记录不存在或不可接诊。\n");
            return NULL;
        }
        return r;
    }
}

static RecordNode* createBasicRecord(PatientNode* patient, DoctorNode* doctor, int type, const char* diagnosis) {
    RecordNode* r = (RecordNode*)malloc(sizeof(RecordNode));
    if (!r) return NULL;
    memset(r, 0, sizeof(RecordNode));
    generateRecordId(r->recordId);
    r->patientId = patient->patientId;
    strncpy(r->patientName, patient->name, MAX_NAME_LEN - 1);
    r->docId = doctor->docId;
    strncpy(r->docName, doctor->name, MAX_NAME_LEN - 1);
    r->type = type;
    strncpy(r->diagnosis, diagnosis, MAX_DIAG_LEN - 1);
    strncpy(r->date, g_currentDate, MAX_DATE_LEN - 1);
    r->hour = g_currentHour;
    r->minute = g_currentMinute;
    appendRecord(r);
    return r;
}

/* ============ Bug 2 修复：开检查时扣患者余额 ============ */
static void doExamination(PatientNode* patient, DoctorNode* doctor) {
    char item[MAX_DIAG_LEN];
    Money fee;
    RecordNode* r;

    safeInputString("请输入检查项目名称：", item, MAX_DIAG_LEN);
    if (strlen(item) == 0) {
        printf("检查取消。\n");
        return;
    }
    fee = safeInputMoneyFen("请输入检查费用（元）：");
    if (fee < 0) {
        printf("费用不能为负。\n");
        return;
    }

    /* 余额校验 */
    if (patient->balanceCents < fee) {
        printf("患者账户余额不足（当前余额=");
        printMoney(patient->balanceCents);
        printf("，检查费=");
        printMoney(fee);
        printf("），检查取消。\n");
        return;
    }

    r = createBasicRecord(patient, doctor, REC_TYPE_EXAM, item);
    if (!r) {
        printf("记录创建失败。\n");
        return;
    }
    r->checkFeeCents = fee;
    r->totalCostCents = fee;

    /* 扣款 + 营收 */
    patient->balanceCents -= fee;
    g_hospitalRevenue += fee;

    printf("检查记录已生成：%s 金额=", r->recordId);
    printMoney(fee);
    printf("\n账户余额：");
    printMoney(patient->balanceCents);
    printf("\n");
}

/* ============ Bug 2 修复：开药时扣患者余额 ============ */
static void doPrescription(PatientNode* patient, DoctorNode* doctor) {
    int medId, qty;
    MedicineNode* m;
    RecordNode* r;
    Money totalCost;

    medId = safeInputInt("请输入药品ID：");
    m = findMedicineById(medId);
    if (!m || m->isDeleted) {
        printf("药品不存在。\n");
        return;
    }
    qty = safeInputInt("请输入开药数量（盒）：");
    if (qty <= 0 || qty > MAX_PRESCRIPTION_QTY) {
        printf("数量必须在 1~%d。\n", MAX_PRESCRIPTION_QTY);
        return;
    }

    totalCost = m->priceCents * qty;

    /* 余额校验（先于扣库存，避免后续需要回滚）*/
    if (patient->balanceCents < totalCost) {
        printf("患者账户余额不足（当前余额=");
        printMoney(patient->balanceCents);
        printf("，药费=");
        printMoney(totalCost);
        printf("），开药取消。\n");
        return;
    }

    /* 扣库存 */
    if (DeductMedicineStock(medId, qty) != 0) {
        printf("库存不足或数量非法。\n");
        return;
    }

    r = createBasicRecord(patient, doctor, REC_TYPE_PRESCRIBE, "处方开药");
    if (!r) {
        m->stock += qty;   /* 回滚库存 */
        printf("记录创建失败。\n");
        return;
    }
    r->medId = m->medId;
    r->medCount = qty;
    r->totalCostCents = totalCost;

    /* 扣款 + 营收 */
    patient->balanceCents -= totalCost;
    g_hospitalRevenue += totalCost;

    printf("处方记录已生成：%s 药品=%s 数量=%d 总额=",
        r->recordId, m->tradeName, qty);
    printMoney(totalCost);
    printf("\n账户余额：");
    printMoney(patient->balanceCents);
    printf("\n");
}

/* ============ Bug 3 修复：更正记录时回滚旧记录 + 应用新记录 ============ */
static void modifyRecordByRevoke(void) {
    char recordId[MAX_REC_ID_LEN];
    RecordNode* oldRec;
    RecordNode* newRec;
    PatientNode* patient = NULL;

    safeInputString("请输入要更正的记录ID：", recordId, MAX_REC_ID_LEN);
    oldRec = findRecordByIdInternal(recordId);
    if (!oldRec || oldRec->isDeleted) {
        printf("未找到记录。\n");
        return;
    }
    if (oldRec->isRedInk) {
        printf("该记录已经被撤销，不能再次更正。\n");
        return;
    }
    /* 本模块只支持更正"检查/开药/看诊"这类有财务影响的记录；
     * 挂号/住院/出院结算等业务记录应通过其他入口处理。 */
    if (oldRec->type != REC_TYPE_EXAM &&
        oldRec->type != REC_TYPE_PRESCRIBE &&
        oldRec->type != REC_TYPE_VISIT) {
        printf("该记录类型不支持在看诊模块更正（仅支持检查/开药/看诊）。\n");
        return;
    }

    patient = findPatientById(oldRec->patientId);

    /* ----- 第 1 步：回滚旧记录的财务与库存影响 ----- */
    if (patient) {
        patient->balanceCents += oldRec->totalCostCents;
    }
    g_hospitalRevenue -= oldRec->totalCostCents;

    if ((oldRec->type == REC_TYPE_PRESCRIBE || oldRec->type == REC_TYPE_VISIT) &&
        oldRec->medId > 0 && oldRec->medCount > 0) {
        MedicineNode* oldMed = findMedicineById(oldRec->medId);
        if (oldMed) oldMed->stock += oldRec->medCount;
    }

    /* ----- 第 2 步：准备新记录（先复制旧记录结构，再覆盖） ----- */
    newRec = (RecordNode*)malloc(sizeof(RecordNode));
    if (!newRec) {
        /* 内存分配失败 → 撤销第 1 步的回滚 */
        if (patient) patient->balanceCents -= oldRec->totalCostCents;
        g_hospitalRevenue += oldRec->totalCostCents;
        if ((oldRec->type == REC_TYPE_PRESCRIBE || oldRec->type == REC_TYPE_VISIT) &&
            oldRec->medId > 0 && oldRec->medCount > 0) {
            MedicineNode* oldMed = findMedicineById(oldRec->medId);
            if (oldMed) oldMed->stock -= oldRec->medCount;
        }
        printf("内存分配失败。\n");
        return;
    }
    memcpy(newRec, oldRec, sizeof(RecordNode));
    memset(newRec->recordId, 0, sizeof(newRec->recordId));
    generateRecordId(newRec->recordId);
    newRec->isRedInk = 0;
    newRec->isDeleted = 0;
    newRec->status = oldRec->status;
    strncpy(newRec->date, g_currentDate, MAX_DATE_LEN - 1);
    newRec->hour = g_currentHour;
    newRec->minute = g_currentMinute;
    newRec->next = NULL;

    printf("原诊断/说明：%s\n", oldRec->diagnosis);
    safeInputString("请输入新的诊断/说明：", newRec->diagnosis, MAX_DIAG_LEN);

    if (newRec->type == REC_TYPE_EXAM) {
        Money newFee = safeInputMoneyFen("请输入新的检查费用（元）：");
        if (newFee < 0) {
            /* 回滚第 1 步 */
            if (patient) patient->balanceCents -= oldRec->totalCostCents;
            g_hospitalRevenue += oldRec->totalCostCents;
            free(newRec);
            printf("费用不合法，更正取消。\n");
            return;
        }
        newRec->checkFeeCents = newFee;
        newRec->totalCostCents = newFee;
        newRec->medId = 0;
        newRec->medCount = 0;
    }
    else if (newRec->type == REC_TYPE_PRESCRIBE) {
        int newMedId = safeInputInt("请输入新的药品ID：");
        int newQty = safeInputInt("请输入新的数量：");
        MedicineNode* newMed = findMedicineById(newMedId);
        if (!newMed || newQty <= 0 || newQty > MAX_PRESCRIPTION_QTY) {
            /* 回滚第 1 步 */
            if (patient) patient->balanceCents -= oldRec->totalCostCents;
            g_hospitalRevenue += oldRec->totalCostCents;
            if ((oldRec->type == REC_TYPE_PRESCRIBE || oldRec->type == REC_TYPE_VISIT) &&
                oldRec->medId > 0 && oldRec->medCount > 0) {
                MedicineNode* oldMed = findMedicineById(oldRec->medId);
                if (oldMed) oldMed->stock -= oldRec->medCount;
            }
            free(newRec);
            printf("药品或数量无效，更正取消。\n");
            return;
        }
        if (newMed->stock < newQty) {
            /* 回滚第 1 步 */
            if (patient) patient->balanceCents -= oldRec->totalCostCents;
            g_hospitalRevenue += oldRec->totalCostCents;
            if ((oldRec->type == REC_TYPE_PRESCRIBE || oldRec->type == REC_TYPE_VISIT) &&
                oldRec->medId > 0 && oldRec->medCount > 0) {
                MedicineNode* oldMed = findMedicineById(oldRec->medId);
                if (oldMed) oldMed->stock -= oldRec->medCount;
            }
            free(newRec);
            printf("新药品库存不足，更正取消。\n");
            return;
        }
        newRec->medId = newMedId;
        newRec->medCount = newQty;
        newRec->totalCostCents = newMed->priceCents * newQty;
        newRec->checkFeeCents = 0;
    }

    /* ----- 第 3 步：检查患者余额是否足以支付新记录 ----- */
    if (patient && patient->balanceCents < newRec->totalCostCents) {
        /* 余额不够 → 全部回滚 */
        patient->balanceCents -= oldRec->totalCostCents;
        g_hospitalRevenue += oldRec->totalCostCents;
        if ((oldRec->type == REC_TYPE_PRESCRIBE || oldRec->type == REC_TYPE_VISIT) &&
            oldRec->medId > 0 && oldRec->medCount > 0) {
            MedicineNode* oldMed = findMedicineById(oldRec->medId);
            if (oldMed) oldMed->stock -= oldRec->medCount;
        }
        free(newRec);
        printf("患者余额不足以支付更正后费用，更正取消。\n");
        return;
    }

    /* ----- 第 4 步：应用新记录的财务与库存影响 ----- */
    if (patient) patient->balanceCents -= newRec->totalCostCents;
    g_hospitalRevenue += newRec->totalCostCents;

    if (newRec->type == REC_TYPE_PRESCRIBE && newRec->medId > 0 && newRec->medCount > 0) {
        MedicineNode* newMed = findMedicineById(newRec->medId);
        if (newMed) newMed->stock -= newRec->medCount;
    }

    /* ----- 第 5 步：标记旧记录红冲，追加新记录 ----- */
    oldRec->isRedInk = 1;
    appendRecord(newRec);

    printf("更正完成：\n");
    printf("  旧记录（已红冲）：%s 金额=", oldRec->recordId);
    printMoney(oldRec->totalCostCents);
    printf("\n  新记录：%s 金额=", newRec->recordId);
    printMoney(newRec->totalCostCents);
    printf("\n");
    if (patient) {
        printf("  患者余额：");
        printMoney(patient->balanceCents);
        printf("\n");
    }
}

void SeeDoctor(void) {
    int menu;
    while (1) {
        printf("\n================ 看诊模块 ================\n");
        printf("1. 接诊患者\n");
        printf("2. 更正既有记录（先撤销再新增）\n");
        printf("3. 返回上级\n");
        printf("请选择：");
        menu = GetSafeInt();

        if (menu == 3) return;
        if (menu == 2) {
            modifyRecordByRevoke();
            PauseScreen();
            continue;
        }
        if (menu != 1) {
            printf("无效选择。\n");
            PauseScreen();
            continue;
        }

        {
            RecordNode* regRec = selectRegisteredPatient();
            PatientNode* patient;
            DoctorNode* doctor;
            RecordNode* visitRec;
            char diagnosis[MAX_DIAG_LEN];
            int choice;

            if (!regRec) {
                PauseScreen();
                continue;
            }

            patient = findPatientById(regRec->patientId);
            doctor = findDoctorById(regRec->docId);
            if (!patient || !doctor) {
                printf("患者或医生数据异常。\n");
                PauseScreen();
                continue;
            }

            safeInputString("请输入诊断结论：", diagnosis, MAX_DIAG_LEN);
            if (strlen(diagnosis) == 0) {
                printf("诊断不能为空。\n");
                PauseScreen();
                continue;
            }

            visitRec = createBasicRecord(patient, doctor, REC_TYPE_VISIT, diagnosis);
            if (!visitRec) {
                printf("创建看诊记录失败。\n");
                PauseScreen();
                continue;
            }
            regRec->status = 1;

            while (1) {
                printf("\n当前患者：[%d] %s，账户余额=", patient->patientId, patient->name);
                printMoney(patient->balanceCents);
                printf("\n诊断：%s\n", diagnosis);
                printf("1. 开检查\n");
                printf("2. 开药\n");
                printf("3. 转住院\n");
                printf("4. 结束本次看诊\n");
                printf("请选择：");
                choice = GetSafeInt();
                if (choice == 1) {
                    doExamination(patient, doctor);
                }
                else if (choice == 2) {
                    doPrescription(patient, doctor);
                }
                else if (choice == 3) {
                    __admitPatientFromVisit(patient, doctor);
                }
                else if (choice == 4) {
                    break;
                }
                else {
                    printf("无效选择。\n");
                }
            }

            printf("本次看诊完成。看诊记录ID=%s\n", visitRec->recordId);
            PauseScreen();
        }
    }
}