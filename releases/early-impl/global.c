/*************************************************************
 * global.c  —  全局变量定义 & 通用工具函数
 *************************************************************/
#include "outpatient_inpatient.h"

/* ======================== 全局变量定义 ======================== */
PatientNode   *g_patientHead   = NULL;
DoctorNode    *g_doctorHead    = NULL;
MedicineNode  *g_medicineHead  = NULL;
RecordNode    *g_recordHead    = NULL;
BedNode       *g_bedHead       = NULL;
InpatientNode *g_inpatientHead = NULL;

int  g_nextPatientId  = 2001;
int  g_nextRecordSeq  = 1;
int  g_dailyRegCount  = 0;
char g_currentDate[12] = "20260416";
int  g_currentHour     = 9;
int  g_currentWeekday  = 3;   /* 0=周一..6=周日 */
long long g_hospitalRevenue = 0;

/* ======================== 工具函数 ======================== */
void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int safeInputInt(const char *prompt) {
    int val;
    printf("%s", prompt);
    if (scanf("%d", &val) != 1) {
        clearInputBuffer();
        printf("[错误] 请输入有效的整数！\n");
        return -1;
    }
    clearInputBuffer();
    return val;
}

void safeInputString(const char *prompt, char *buf, int maxLen) {
    printf("%s", prompt);
    if (fgets(buf, maxLen, stdin) == NULL) { buf[0] = '\0'; return; }
    int len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
}

PatientNode *findPatientById(int id) {
    PatientNode *p = g_patientHead;
    while (p) {
        if (p->patientId == id && !p->isDeleted) return p;
        p = p->next;
    }
    return NULL;
}

int findPatientsByName(const char *name, PatientNode *results[], int maxResults) {
    int count = 0;
    PatientNode *p = g_patientHead;
    while (p && count < maxResults) {
        if (!p->isDeleted && strcmp(p->name, name) == 0) results[count++] = p;
        p = p->next;
    }
    return count;
}

DoctorNode *findDoctorById(int id) {
    DoctorNode *d = g_doctorHead;
    while (d) {
        if (d->docId == id && !d->isDeleted) return d;
        d = d->next;
    }
    return NULL;
}

MedicineNode *findMedicineById(int id) {
    MedicineNode *m = g_medicineHead;
    while (m) {
        if (m->medId == id && !m->isDeleted) return m;
        m = m->next;
    }
    return NULL;
}

BedNode *findBedById(int id) {
    BedNode *b = g_bedHead;
    while (b) {
        if (b->bedId == id && !b->isDeleted) return b;
        b = b->next;
    }
    return NULL;
}

InpatientNode *findActiveInpatient(int patientId) {
    InpatientNode *ip = g_inpatientHead;
    while (ip) {
        if (ip->patientId == patientId && ip->isAdmitted && !ip->isDeleted) return ip;
        ip = ip->next;
    }
    return NULL;
}

void generateRecordId(char *buf) {
    sprintf(buf, "REC%.*s%04d", 4, g_currentDate, g_nextRecordSeq++);
}

void generateRegId(char *buf) {
    static int dailySeq = 0;
    dailySeq++;
    sprintf(buf, "REG%s%02d", g_currentDate, dailySeq);
}

void appendRecord(RecordNode *newRec) {
    if (!g_recordHead) { g_recordHead = newRec; return; }
    RecordNode *tail = g_recordHead;
    while (tail->next) tail = tail->next;
    tail->next = newRec;
}

int countDailyDoctorRegs(int docId) {
    int count = 0;
    RecordNode *r = g_recordHead;
    while (r) {
        if (r->type == REC_TYPE_REGISTER && r->docId == docId
            && !r->isRedInk && !r->isDeleted
            && strcmp(r->date, g_currentDate) == 0) count++;
        r = r->next;
    }
    return count;
}

int countDailyPatientRegs(int patientId) {
    int count = 0;
    RecordNode *r = g_recordHead;
    while (r) {
        if (r->type == REC_TYPE_REGISTER && r->patientId == patientId
            && !r->isRedInk && !r->isDeleted
            && strcmp(r->date, g_currentDate) == 0) count++;
        r = r->next;
    }
    return count;
}

int countDailyPatientDeptRegs(int patientId, const char *dept) {
    int count = 0;
    RecordNode *r = g_recordHead;
    while (r) {
        if (r->type == REC_TYPE_REGISTER && r->patientId == patientId
            && !r->isRedInk && !r->isDeleted
            && strcmp(r->date, g_currentDate) == 0) {
            DoctorNode *doc = findDoctorById(r->docId);
            if (doc && strcmp(doc->department, dept) == 0) count++;
        }
        r = r->next;
    }
    return count;
}

void printMoney(int cents) {
    if (cents < 0) printf("-%d.%02d元", (-cents) / 100, (-cents) % 100);
    else           printf("%d.%02d元", cents / 100, cents % 100);
}

int isExpertDoctor(DoctorNode *doc) {
    if (!doc) return 0;
    return (strstr(doc->level, "主任") != NULL);
}

int getWardDailyFee(int wardType) {
    switch (wardType) {
        case WARD_NORMAL: return DAILY_FEE_NORMAL;
        case WARD_ICU:    return DAILY_FEE_ICU;
        case WARD_VIP:    return DAILY_FEE_VIP;
        default:          return DAILY_FEE_NORMAL;
    }
}

const char *getWardTypeName(int wardType) {
    switch (wardType) {
        case WARD_NORMAL: return "普通病房";
        case WARD_ICU:    return "重症病房";
        case WARD_VIP:    return "VIP病房";
        default:          return "未知类型";
    }
}
