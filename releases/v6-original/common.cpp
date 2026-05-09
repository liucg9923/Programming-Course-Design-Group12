#include "global.h"
#include "ward.h"
#include <limits.h>
#include <errno.h>

PatientNode* g_patientHead = NULL;
DoctorNode* g_doctorHead = NULL;
MedicineNode* g_medicineHead = NULL;
RecordNode* g_recordHead = NULL;
BedNode* g_bedHead = NULL;
InpatientNode* g_inpatientHead = NULL;
DepartmentNode* g_departmentHead = NULL;

int   g_nextPatientId = 2001;
int   g_nextRecordSeq = 1;
int   g_nextWardId = 1;
int   g_nextDeptId = 1;
int   g_nextRegSeq = 0;                                /* 修复 Bug 04 */
char  g_lastRegDate[MAX_DATE_LEN] = "";                /* 修复 Bug 04 */
char  g_currentDate[MAX_DATE_LEN] = "20260423";
int   g_currentHour = 9;
int   g_currentMinute = 0;
int   g_currentWeekday = 2;
Money g_hospitalRevenue = 0;

static int parseIntLine(const char* s, int* ok) {
    char buf[MAX_INPUT_LEN];
    long val;
    char* end = NULL;
    size_t len;

    *ok = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;

    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
    if (len == 0 || len >= sizeof(buf)) return 0;
    memcpy(buf, s, len);
    buf[len] = '\0';

    errno = 0;
    val = strtol(buf, &end, 10);
    if (end == buf) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    if (errno == ERANGE || val < INT_MIN || val > INT_MAX) return 0;

    *ok = 1;
    return (int)val;
}

static Money parseMoneyLine(const char* s, int* ok) {
    char buf[MAX_INPUT_LEN];
    char* end = NULL;
    double val;
    size_t len;
    Money cents;

    *ok = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;

    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
    if (len == 0 || len >= sizeof(buf)) return 0;
    memcpy(buf, s, len);
    buf[len] = '\0';

    val = strtod(buf, &end);
    if (end == buf) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;

    cents = (Money)(val * 100.0 + (val >= 0 ? 0.5 : -0.5));
    *ok = 1;
    return cents;
}

void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

void trimNewline(char* s) {
    size_t len;
    if (!s) return;
    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

int GetSafeInt(void) {
    char line[MAX_INPUT_LEN];
    int ok;
    int value;

    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            continue;
        }
        value = parseIntLine(line, &ok);
        if (ok) return value;
        printf("[系统拦截] 请输入合法整数：");
    }
}

void GetSafeString(char* buffer, int max_len) {
    if (!buffer || max_len <= 0) return;
    if (fgets(buffer, max_len, stdin) == NULL) {
        clearerr(stdin);
        buffer[0] = '\0';
        return;
    }
    trimNewline(buffer);
    if ((int)strlen(buffer) == max_len - 1 && buffer[max_len - 2] != '\0') {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}
    }
}

int safeInputInt(const char* prompt) {
    if (prompt) printf("%s", prompt);
    return GetSafeInt();
}

void safeInputString(const char* prompt, char* buf, int maxLen) {
    if (prompt) printf("%s", prompt);
    GetSafeString(buf, maxLen);
}

Money safeInputMoneyFen(const char* promptYuan) {
    char line[MAX_INPUT_LEN];
    int ok;
    Money cents;

    while (1) {
        if (promptYuan) printf("%s", promptYuan);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            continue;
        }
        cents = parseMoneyLine(line, &ok);
        if (ok) return cents;
        printf("[系统拦截] 请输入合法金额，例如 123 或 45.67\n");
    }
}

void PauseScreen(void) {
    printf("\n按回车继续...");
    fflush(stdout);
    getchar();
}

int strContains(const char* text, const char* sub) {
    if (!text || !sub) return 0;
    return strstr(text, sub) != NULL;
}

void formatMoney(Money cents, char* buf, size_t size) {
    long long absVal = cents >= 0 ? cents : -cents;
    snprintf(buf, size, "%s%lld.%02lld元",
        cents < 0 ? "-" : "",
        absVal / 100,
        absVal % 100);
}

void printMoney(Money cents) {
    char buf[64];
    formatMoney(cents, buf, sizeof(buf));
    printf("%s", buf);
}

int isExpertDoctor(const DoctorNode* doc) {
    if (!doc) return 0;
    return strstr(doc->level, "主任") != NULL;
}

int getWardDailyFee(int wardType) {
    switch (wardType) {
    case WARD_NORMAL: return DAILY_FEE_NORMAL;
    case WARD_ICU:    return DAILY_FEE_ICU;
    case WARD_VIP:    return DAILY_FEE_VIP;
    default:          return DAILY_FEE_NORMAL;
    }
}

const char* getWardTypeName(int wardType) {
    switch (wardType) {
    case WARD_NORMAL: return "普通病房";
    case WARD_ICU:    return "重症病房";
    case WARD_VIP:    return "VIP病房";
    default:          return "未知病房";
    }
}

PatientNode* findPatientById(int id) {
    PatientNode* p = g_patientHead;
    while (p) {
        if (!p->isDeleted && p->patientId == id) return p;
        p = p->next;
    }
    return NULL;
}

DoctorNode* findDoctorById(int id) {
    DoctorNode* d = g_doctorHead;
    while (d) {
        if (!d->isDeleted && d->docId == id) return d;
        d = d->next;
    }
    return NULL;
}

MedicineNode* findMedicineById(int id) {
    MedicineNode* m = g_medicineHead;
    while (m) {
        if (!m->isDeleted && m->medId == id) return m;
        m = m->next;
    }
    return NULL;
}

BedNode* findBedById(int id) {
    BedNode* b = g_bedHead;
    while (b) {
        if (!b->isDeleted && b->bedId == id) return b;
        b = b->next;
    }
    return NULL;
}

InpatientNode* findActiveInpatient(int patientId) {
    InpatientNode* ip = g_inpatientHead;
    while (ip) {
        if (!ip->isDeleted && ip->isAdmitted && ip->patientId == patientId) return ip;
        ip = ip->next;
    }
    return NULL;
}

DepartmentNode* findDepartmentByName(const char* deptName) {
    DepartmentNode* d = g_departmentHead;
    while (d) {
        if (strcmp(d->deptName, deptName) == 0) return d;
        d = d->next;
    }
    return NULL;
}

int findPatientsByName(const char* name, PatientNode* results[], int maxResults) {
    int count = 0;
    PatientNode* p = g_patientHead;
    while (p) {
        if (!p->isDeleted && strcmp(p->name, name) == 0) {
            if (results && count < maxResults) results[count] = p;
            count++;
        }
        p = p->next;
    }
    return count;
}

int findDoctorsByName(const char* name, DoctorNode* results[], int maxResults) {
    int count = 0;
    DoctorNode* d = g_doctorHead;
    while (d) {
        if (!d->isDeleted && strcmp(d->name, name) == 0) {
            if (results && count < maxResults) results[count] = d;
            count++;
        }
        d = d->next;
    }
    return count;
}

void appendPatient(PatientNode* node) {
    PatientNode* tail;
    if (!node) return;
    node->next = NULL;
    if (!g_patientHead) {
        g_patientHead = node;
        return;
    }
    tail = g_patientHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void appendDoctor(DoctorNode* node) {
    DoctorNode* tail;
    if (!node) return;
    node->next = NULL;
    if (!g_doctorHead) {
        g_doctorHead = node;
        return;
    }
    tail = g_doctorHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void appendMedicine(MedicineNode* node) {
    MedicineNode* tail;
    if (!node) return;
    node->next = NULL;
    if (!g_medicineHead) {
        g_medicineHead = node;
        return;
    }
    tail = g_medicineHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void appendRecord(RecordNode* node) {
    RecordNode* tail;
    if (!node) return;
    node->next = NULL;
    if (!g_recordHead) {
        g_recordHead = node;
        return;
    }
    tail = g_recordHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void appendBed(BedNode* node) {
    BedNode* tail;
    if (!node) return;
    node->next = NULL;
    if (!g_bedHead) {
        g_bedHead = node;
        return;
    }
    tail = g_bedHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void appendInpatient(InpatientNode* node) {
    InpatientNode* tail;
    if (!node) return;
    node->next = NULL;
    if (!g_inpatientHead) {
        g_inpatientHead = node;
        return;
    }
    tail = g_inpatientHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void appendDepartmentIfAbsent(const char* deptName) {
    DepartmentNode* node, * tail;
    if (!deptName || !*deptName) return;
    if (findDepartmentByName(deptName)) return;
    node = (DepartmentNode*)malloc(sizeof(DepartmentNode));
    if (!node) return;
    memset(node, 0, sizeof(DepartmentNode));
    node->deptId = g_nextDeptId++;
    strncpy(node->deptName, deptName, MAX_DEPT_LEN - 1);
    node->next = NULL;
    if (!g_departmentHead) {
        g_departmentHead = node;
        return;
    }
    tail = g_departmentHead;
    while (tail->next) tail = tail->next;
    tail->next = node;
}

void generateRecordId(char* buf) {
    /* 修复 Bug 17：sprintf -> snprintf，避免缓冲区溢出 */
    snprintf(buf, MAX_REC_ID_LEN, "REC%s%04d", g_currentDate, g_nextRecordSeq++);
}

void generateRegId(char* buf) {
    /* 修复 Bug 04：dailySeq 改为持久化的 g_nextRegSeq，并按日期重置 */
    if (strcmp(g_lastRegDate, g_currentDate) != 0) {
        g_nextRegSeq = 0;
        strncpy(g_lastRegDate, g_currentDate, MAX_DATE_LEN - 1);
        g_lastRegDate[MAX_DATE_LEN - 1] = '\0';
    }
    g_nextRegSeq++;
    snprintf(buf, MAX_REG_ID_LEN, "REG%s%02d", g_currentDate, g_nextRegSeq);
}

int countDailyHospitalRegs(void) {
    int count = 0;
    RecordNode* r = g_recordHead;
    while (r) {
        if (!r->isDeleted && !r->isRedInk &&
            r->type == REC_TYPE_REGISTER &&
            strcmp(r->date, g_currentDate) == 0) {
            count++;
        }
        r = r->next;
    }
    return count;
}

int countDailyDoctorRegs(int docId) {
    int count = 0;
    RecordNode* r = g_recordHead;
    while (r) {
        if (!r->isDeleted && !r->isRedInk &&
            r->type == REC_TYPE_REGISTER &&
            r->docId == docId &&
            strcmp(r->date, g_currentDate) == 0) {
            count++;
        }
        r = r->next;
    }
    return count;
}

int countDailyPatientRegs(int patientId) {
    int count = 0;
    RecordNode* r = g_recordHead;
    while (r) {
        if (!r->isDeleted && !r->isRedInk &&
            r->type == REC_TYPE_REGISTER &&
            r->patientId == patientId &&
            strcmp(r->date, g_currentDate) == 0) {
            count++;
        }
        r = r->next;
    }
    return count;
}

int countDailyPatientDeptRegs(int patientId, const char* dept) {
    int count = 0;
    RecordNode* r = g_recordHead;
    while (r) {
        if (!r->isDeleted && !r->isRedInk &&
            r->type == REC_TYPE_REGISTER &&
            r->patientId == patientId &&
            strcmp(r->date, g_currentDate) == 0) {
            DoctorNode* doc = findDoctorById(r->docId);
            if (doc && strcmp(doc->department, dept) == 0) count++;
        }
        r = r->next;
    }
    return count;
}

int getDoctorCount(void) {
    int count = 0;
    DoctorNode* d = g_doctorHead;
    while (d) { if (!d->isDeleted) count++; d = d->next; }
    return count;
}

int getPatientCount(void) {
    int count = 0;
    PatientNode* p = g_patientHead;
    while (p) { if (!p->isDeleted) count++; p = p->next; }
    return count;
}

int getMedicineCount(void) {
    int count = 0;
    MedicineNode* m = g_medicineHead;
    while (m) { if (!m->isDeleted) count++; m = m->next; }
    return count;
}

int getRecordCount(void) {
    int count = 0;
    RecordNode* r = g_recordHead;
    while (r) { if (!r->isDeleted) count++; r = r->next; }
    return count;
}

int getBedCount(void) {
    int count = 0;
    BedNode* b = g_bedHead;
    while (b) { if (!b->isDeleted) count++; b = b->next; }
    return count;
}

int getDepartmentCount(void) {
    int count = 0;
    DepartmentNode* d = g_departmentHead;
    while (d) { count++; d = d->next; }
    return count;
}

void releaseAllData(void) {
    PatientNode* p;
    DoctorNode* d;
    MedicineNode* m;
    RecordNode* r;
    BedNode* b;
    InpatientNode* ip;
    DepartmentNode* dep;

    p = g_patientHead;
    while (p) { PatientNode* t = p; p = p->next; free(t); }
    d = g_doctorHead;
    while (d) { DoctorNode* t = d; d = d->next; free(t); }
    m = g_medicineHead;
    while (m) { MedicineNode* t = m; m = m->next; free(t); }
    r = g_recordHead;
    while (r) { RecordNode* t = r; r = r->next; free(t); }
    b = g_bedHead;
    while (b) { BedNode* t = b; b = b->next; free(t); }
    ip = g_inpatientHead;
    while (ip) { InpatientNode* t = ip; ip = ip->next; free(t); }
    dep = g_departmentHead;
    while (dep) { DepartmentNode* t = dep; dep = dep->next; free(t); }

    g_patientHead = NULL;
    g_doctorHead = NULL;
    g_medicineHead = NULL;
    g_recordHead = NULL;
    g_bedHead = NULL;
    g_inpatientHead = NULL;
    g_departmentHead = NULL;

    /* 修复 Bug 16：重置营业额，避免重新加载时累积错误值 */
    g_hospitalRevenue = 0;
    g_nextRegSeq = 0;
    g_lastRegDate[0] = 0;
}

static void addDefaultBed(int wardId, int bedId, int wardType, const char* dept) {
    BedNode* b = (BedNode*)malloc(sizeof(BedNode));
    if (!b) return;
    memset(b, 0, sizeof(BedNode));
    b->wardId = wardId;
    b->bedId = bedId;
    b->wardType = wardType;
    strncpy(b->relatedDept, dept, MAX_DEPT_LEN - 1);
    b->bedStatus = BED_FREE;
    b->patientId = 0;
    appendBed(b);
}

void initDefaultDepartmentsAndBeds(void) {
    if (!findDepartmentByName("内科")) appendDepartmentIfAbsent("内科");
    if (!findDepartmentByName("外科")) appendDepartmentIfAbsent("外科");
    if (!findDepartmentByName("儿科")) appendDepartmentIfAbsent("儿科");
    if (!findDepartmentByName("急诊科")) appendDepartmentIfAbsent("急诊科");
    if (!findDepartmentByName("骨科")) appendDepartmentIfAbsent("骨科");
    if (!findDepartmentByName("妇产科")) appendDepartmentIfAbsent("妇产科");
    if (!findDepartmentByName("眼科")) appendDepartmentIfAbsent("眼科");
    if (!findDepartmentByName("心内科")) appendDepartmentIfAbsent("心内科");
    if (!findDepartmentByName("呼吸内科")) appendDepartmentIfAbsent("呼吸内科");

    if (g_bedHead) return;

    addDefaultBed(1, 101, WARD_NORMAL, "内科");
    addDefaultBed(1, 102, WARD_NORMAL, "内科");
    addDefaultBed(1, 103, WARD_NORMAL, "内科");
    addDefaultBed(1, 104, WARD_NORMAL, "内科");

    addDefaultBed(2, 201, WARD_NORMAL, "外科");
    addDefaultBed(2, 202, WARD_NORMAL, "外科");
    addDefaultBed(2, 203, WARD_NORMAL, "外科");
    addDefaultBed(2, 204, WARD_NORMAL, "外科");

    addDefaultBed(3, 301, WARD_ICU, "急诊科");
    addDefaultBed(3, 302, WARD_ICU, "急诊科");
    addDefaultBed(3, 303, WARD_ICU, "急诊科");
    addDefaultBed(3, 304, WARD_ICU, "急诊科");

    addDefaultBed(4, 401, WARD_VIP, "内科");
    addDefaultBed(4, 402, WARD_VIP, "内科");
    addDefaultBed(4, 403, WARD_VIP, "外科");
    addDefaultBed(4, 404, WARD_VIP, "外科");

    addDefaultBed(5, 501, WARD_NORMAL, "儿科");
    addDefaultBed(5, 502, WARD_NORMAL, "儿科");
    addDefaultBed(5, 503, WARD_NORMAL, "儿科");
    addDefaultBed(5, 504, WARD_NORMAL, "儿科");

    addDefaultBed(6, 601, WARD_NORMAL, "骨科");
    addDefaultBed(6, 602, WARD_NORMAL, "骨科");
    addDefaultBed(6, 603, WARD_NORMAL, "骨科");
    addDefaultBed(6, 604, WARD_NORMAL, "骨科");

    addDefaultBed(7, 701, WARD_NORMAL, "妇产科");
    addDefaultBed(7, 702, WARD_NORMAL, "妇产科");
    addDefaultBed(7, 703, WARD_VIP, "妇产科");
    addDefaultBed(7, 704, WARD_VIP, "妇产科");

    addDefaultBed(8, 801, WARD_NORMAL, "心内科");
    addDefaultBed(8, 802, WARD_NORMAL, "心内科");
    addDefaultBed(8, 803, WARD_NORMAL, "呼吸内科");
    addDefaultBed(8, 804, WARD_NORMAL, "呼吸内科");
}
