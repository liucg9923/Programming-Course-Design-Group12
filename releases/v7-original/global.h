#pragma once
#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif


#ifdef __cplusplus
extern "C" {
#endif

    typedef long long Money;

#define MAX_NAME_LEN       64
#define MAX_DEPT_LEN       64
#define MAX_LEVEL_LEN      32
#define MAX_DIAG_LEN       128
#define MAX_MED_NAME_LEN   128
#define MAX_REG_ID_LEN     32
#define MAX_REC_ID_LEN     32
#define MAX_DATE_LEN       16
#define MAX_INPUT_LEN      256

#define DAILY_MAX_TICKETS       500
#define DAILY_MAX_PER_DOCTOR     20
#define DAILY_MAX_PER_PATIENT     5
#define DAILY_MAX_PER_DEPT        1

#define REG_FEE_NORMAL        2000
#define REG_FEE_EXPERT        5000

#define DEPOSIT_UNIT         10000
#define DEPOSIT_SAFE_LINE   100000
#define MAX_PRESCRIPTION_QTY   100
#define STOCK_WARNING_THRESHOLD 50   /* V5 修复 Bug 28：库存低于此值触发预警 */

#define DAILY_FEE_NORMAL     20000
#define DAILY_FEE_ICU        50000
#define DAILY_FEE_VIP        80000

#define REC_TYPE_REGISTER       1
#define REC_TYPE_VISIT          2
#define REC_TYPE_EXAM           3
#define REC_TYPE_PRESCRIBE      4
#define REC_TYPE_INPATIENT      5
#define REC_TYPE_SETTLEMENT     6
#define REC_TYPE_RECHARGE       7   /* 修复 Bug 08：账户充值流水（用于审计） */

#define WARD_NORMAL   1
#define WARD_ICU      2
#define WARD_VIP      3

#define BED_FREE        0
#define BED_OCCUPIED    1
#define BED_MAINTENANCE 2

    typedef struct PatientNode {
        int    patientId;
        char   name[MAX_NAME_LEN];
        int    age;
        char   lastRegId[MAX_REG_ID_LEN];
        Money  balanceCents;
        int    isDeleted;
        struct PatientNode* next;
    } PatientNode;

    typedef struct DoctorNode {
        int    docId;
        char   name[MAX_NAME_LEN];
        char   level[MAX_LEVEL_LEN];
        char   department[MAX_DEPT_LEN];
        int    schedule[7];
        int    currentLoad;
        int    isDeleted;
        struct DoctorNode* next;
    } DoctorNode;

    typedef struct MedicineNode {
        int    medId;
        char   officialName[MAX_MED_NAME_LEN];
        char   tradeName[MAX_MED_NAME_LEN];
        char   aliasName[MAX_MED_NAME_LEN];
        Money  priceCents;
        int    stock;
        char   relatedDept[MAX_DEPT_LEN];
        int    isDeleted;
        struct MedicineNode* next;
    } MedicineNode;

    typedef struct RecordNode {
        char   recordId[MAX_REC_ID_LEN];
        int    patientId;
        char   patientName[MAX_NAME_LEN];
        int    docId;
        char   docName[MAX_NAME_LEN];
        int    type;
        char   diagnosis[MAX_DIAG_LEN];
        Money  checkFeeCents;
        int    medId;
        int    medCount;
        int    wardId;
        int    bedId;
        Money  totalCostCents;
        int    status;
        int    isRedInk;
        int    isDeleted;
        char   date[MAX_DATE_LEN];
        int    hour;
        int    minute;
        struct RecordNode* next;
    } RecordNode;

    typedef struct BedNode {
        int    wardId;
        int    bedId;
        int    wardType;
        char   relatedDept[MAX_DEPT_LEN];
        int    bedStatus;
        int    patientId;
        int    isDeleted;
        struct BedNode* next;
    } BedNode;

    typedef struct InpatientNode {
        int    patientId;
        int    docId;
        int    wardId;
        int    bedId;
        Money  depositTotalCents;
        Money  depositBalanceCents;
        Money  dailyFeeCents;
        Money  totalChargedCents;
        char   admitDate[MAX_DATE_LEN];
        char   lastChargeDate[MAX_DATE_LEN];
        int    admitHour;
        int    expectedDays;
        int    daysStayed;
        int    isAdmitted;
        int    isDeleted;
        struct InpatientNode* next;
    } InpatientNode;

    typedef struct DepartmentNode {
        int    deptId;
        char   deptName[MAX_DEPT_LEN];
        struct DepartmentNode* next;
    } DepartmentNode;

    /* 全局链表头指针 */
    extern PatientNode* g_patientHead;
    extern DoctorNode* g_doctorHead;
    extern MedicineNode* g_medicineHead;
    extern RecordNode* g_recordHead;
    extern BedNode* g_bedHead;
    extern InpatientNode* g_inpatientHead;
    extern DepartmentNode* g_departmentHead;

    /* 系统状态 */
    extern int   g_nextPatientId;
    extern int   g_nextRecordSeq;
    extern int   g_nextWardId;
    extern int   g_nextDeptId;
    extern int   g_nextRegSeq;        /* 修复 Bug 04：跨进程持久化的当日挂号序号 */
    extern char  g_lastRegDate[MAX_DATE_LEN]; /* 修复 Bug 04：g_nextRegSeq 对应的日期 */
    extern char  g_currentDate[MAX_DATE_LEN];
    extern int   g_currentHour;
    extern int   g_currentMinute;
    extern int   g_currentWeekday;  /* 0=周一..6=周日 */
    extern Money g_hospitalRevenue;

    /* 公共输入 */
    void clearInputBuffer(void);
    int  GetSafeInt(void);
    void GetSafeString(char* buffer, int max_len);
    int  safeInputInt(const char* prompt);
    void safeInputString(const char* prompt, char* buf, int maxLen);
    Money safeInputMoneyFen(const char* promptYuan);
    void PauseScreen(void);

    /* 通用工具 */
    void trimNewline(char* s);
    int  strContains(const char* text, const char* sub);
    void printMoney(Money cents);
    void formatMoney(Money cents, char* buf, size_t size);
    int  isExpertDoctor(const DoctorNode* doc);
    int  getWardDailyFee(int wardType);
    const char* getWardTypeName(int wardType);

    /* 链表/查询 */
    PatientNode* findPatientById(int id);
    DoctorNode* findDoctorById(int id);
    MedicineNode* findMedicineById(int id);
    BedNode* findBedById(int id);
    InpatientNode* findActiveInpatient(int patientId);
    DepartmentNode* findDepartmentByName(const char* deptName);

    int  findPatientsByName(const char* name, PatientNode* results[], int maxResults);
    int  findDoctorsByName(const char* name, DoctorNode* results[], int maxResults);

    void appendPatient(PatientNode* node);
    void appendDoctor(DoctorNode* node);
    void appendMedicine(MedicineNode* node);
    void appendRecord(RecordNode* node);
    void appendBed(BedNode* node);
    void appendInpatient(InpatientNode* node);
    void appendDepartmentIfAbsent(const char* deptName);

    void generateRecordId(char* buf);
    void generateRegId(char* buf);

    int  countDailyHospitalRegs(void);
    int  countDailyDoctorRegs(int docId);
    int  countDailyPatientRegs(int patientId);
    int  countDailyPatientDeptRegs(int patientId, const char* dept);

    int  getDoctorCount(void);
    int  getPatientCount(void);
    int  getMedicineCount(void);
    int  getRecordCount(void);
    int  getBedCount(void);
    int  getDepartmentCount(void);

    void releaseAllData(void);
    void initDefaultDepartmentsAndBeds(void);

    /* 模块入口 */
    void RegisterPatient(void);
    void SeeDoctor(void);
    void ProcessBilling(void);
    void PharmacyManagement(void);
    void ShowWardStatus(void);
    void SearchModule(void);
    void StatisticModule(void);
    void LoadAllData(void);
    void SaveAllData(void);
    void DeleteRecord(void);
    void RestoreDeletedRecord(void);    /* V5 新增：恢复软删除记录 */
    int  ensureEnoughBalance(PatientNode* patient, Money need, const char* scene); /* V5 跨模块复用 */

#ifdef __cplusplus
}
#endif

#endif
