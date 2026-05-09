/*************************************************************
 * outpatient_inpatient.h  —  医疗管理系统 (HIS) 公共头文件（刘承庚模块）
 *************************************************************/
#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== 常量定义 ======================== */
#define MAX_NAME_LEN      40
#define MAX_DEPT_LEN      30
#define MAX_LEVEL_LEN     20
#define MAX_DIAG_LEN     120
#define MAX_MED_NAME_LEN  60
#define MAX_REG_ID_LEN    20
#define MAX_REC_ID_LEN    16

/* 挂号限制 */
#define DAILY_MAX_TICKETS       500
#define DAILY_MAX_PER_DOCTOR     20
#define DAILY_MAX_PER_PATIENT     5
#define DAILY_MAX_PER_DEPT        1

/* 挂号费（分） */
#define REG_FEE_NORMAL        2000
#define REG_FEE_EXPERT        5000

/* 住院相关 */
#define DEPOSIT_UNIT         10000
#define DEPOSIT_SAFE_LINE   100000
#define MAX_PRESCRIPTION_QTY    100

#define DAILY_FEE_NORMAL     20000
#define DAILY_FEE_ICU        50000
#define DAILY_FEE_VIP        80000

/* 诊疗记录类型 */
#define REC_TYPE_REGISTER      1
#define REC_TYPE_VISIT         2
#define REC_TYPE_EXAM          3
#define REC_TYPE_PRESCRIBE     4
#define REC_TYPE_INPATIENT     5

/* 病房类型 */
#define WARD_NORMAL   1
#define WARD_ICU      2
#define WARD_VIP      3

/* 床位状态 */
#define BED_FREE        0
#define BED_OCCUPIED    1
#define BED_MAINTENANCE 2

/* ======================== 数据结构 ======================== */
typedef struct PatientNode {
    int    patientId;
    char   name[MAX_NAME_LEN];
    int    age;
    char   lastRegId[MAX_REG_ID_LEN];
    int    balanceCents;
    int    isDeleted;
    struct PatientNode *next;
} PatientNode;

typedef struct DoctorNode {
    int    docId;
    char   name[MAX_NAME_LEN];
    char   level[MAX_LEVEL_LEN];
    char   department[MAX_DEPT_LEN];
    int    schedule[7];
    int    currentLoad;
    int    isDeleted;
    struct DoctorNode *next;
} DoctorNode;

typedef struct MedicineNode {
    int    medId;
    char   officialName[MAX_MED_NAME_LEN];
    char   tradeName[MAX_MED_NAME_LEN];
    char   aliasName[MAX_MED_NAME_LEN];
    int    priceCents;
    int    stock;
    char   relatedDept[MAX_DEPT_LEN];
    int    isDeleted;
    struct MedicineNode *next;
} MedicineNode;

typedef struct RecordNode {
    char   recordId[MAX_REC_ID_LEN];
    int    patientId;
    char   patientName[MAX_NAME_LEN];
    int    docId;
    char   docName[MAX_NAME_LEN];
    int    type;
    char   diagnosis[MAX_DIAG_LEN];
    int    checkFeeCents;
    int    medId;
    int    medCount;
    int    totalCostCents;
    int    status;
    int    isRedInk;
    int    isDeleted;
    char   date[12];
    struct RecordNode *next;
} RecordNode;

typedef struct BedNode {
    int    bedId;
    int    wardType;
    char   relatedDept[MAX_DEPT_LEN];
    int    bedStatus;
    int    patientId;
    int    isDeleted;
    struct BedNode *next;
} BedNode;

typedef struct InpatientNode {
    int    patientId;
    int    docId;
    int    bedId;
    int    depositCents;
    int    dailyFeeCents;
    char   admitDate[12];
    int    expectedDays;
    int    daysStayed;
    int    totalCharged;
    int    isAdmitted;
    int    isDeleted;
    struct InpatientNode *next;
} InpatientNode;

/* ======================== 全局变量 ======================== */
extern PatientNode   *g_patientHead;
extern DoctorNode    *g_doctorHead;
extern MedicineNode  *g_medicineHead;
extern RecordNode    *g_recordHead;
extern BedNode       *g_bedHead;
extern InpatientNode *g_inpatientHead;

extern int g_nextPatientId;
extern int g_nextRecordSeq;
extern int g_dailyRegCount;
extern char g_currentDate[12];
extern int  g_currentHour;
extern int  g_currentWeekday;
extern long long g_hospitalRevenue;

/* ======================== 工具函数 ======================== */
int  safeInputInt(const char *prompt);
void safeInputString(const char *prompt, char *buf, int maxLen);
void clearInputBuffer(void);

PatientNode   *findPatientById(int id);
int            findPatientsByName(const char *name, PatientNode *results[], int maxResults);
DoctorNode    *findDoctorById(int id);
MedicineNode  *findMedicineById(int id);
BedNode       *findBedById(int id);
InpatientNode *findActiveInpatient(int patientId);

void generateRecordId(char *buf);
void generateRegId(char *buf);
void appendRecord(RecordNode *newRec);

int  countDailyDoctorRegs(int docId);
int  countDailyPatientRegs(int patientId);
int  countDailyPatientDeptRegs(int patientId, const char *dept);

void printMoney(int cents);
int  isExpertDoctor(DoctorNode *doc);
int  getWardDailyFee(int wardType);
const char *getWardTypeName(int wardType);

/* ======================== 模块函数声明 ======================== */
/* 刘承庚模块 —— 严格按题签命名，仅暴露三个对外函数                     */
/*   RegisterPatient : 挂号（含患者建档/老患者挂号/限制校验/生成挂号号） */
/*   SeeDoctor       : 看诊（含诊断/检查/开药/转住院/记录撤销）          */
/*   ProcessBilling  : 住院（含办理/自动扣费/预警/出院结算）             */
void RegisterPatient(void);
void SeeDoctor(void);
void ProcessBilling(void);

#endif /* GLOBAL_H */
