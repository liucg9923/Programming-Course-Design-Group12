#include "module_d.h"
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <limits.h>
#include <mach-o/dyld.h>          /* V5 修复 Bug 30：_NSGetExecutablePath */
#else
#include <unistd.h>
#include <limits.h>
#endif

static FILE* openDataFile(const char* filename, const char* mode) {
    char path[1024];
#ifdef _WIN32
    char exePath[1024];
    DWORD len = GetModuleFileNameA(NULL, exePath, (DWORD)sizeof(exePath));
    if (len > 0 && len < sizeof(exePath)) {
        char* slash = strrchr(exePath, '\\');
        if (slash) {
            *slash = '\0';
            snprintf(path, sizeof(path), "%s\\%s", exePath, filename);
            {
                FILE* fp = fopen(path, mode);
                if (fp) return fp;
            }
        }
    }
#elif defined(__APPLE__)
    /* V5 修复 Bug 30：macOS 没有 /proc/self/exe，改用 _NSGetExecutablePath */
    char exePath[PATH_MAX];
    uint32_t bufSize = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &bufSize) == 0) {
        char* slash = strrchr(exePath, '/');
        if (slash) {
            *slash = '\0';
            snprintf(path, sizeof(path), "%s/%s", exePath, filename);
            {
                FILE* fp = fopen(path, mode);
                if (fp) return fp;
            }
        }
    }
#else
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        char* slash = strrchr(exePath, '/');
        if (slash) {
            *slash = '\0';
            snprintf(path, sizeof(path), "%s/%s", exePath, filename);
            {
                FILE* fp = fopen(path, mode);
                if (fp) return fp;
            }
        }
    }
#endif
    return fopen(filename, mode);
}

static int split_ws(char* line, char* fields[], int max_fields) {
    /* 修复 Bug 05：分隔符仅用 \t\r\n（不含空格），
     * 否则字段内的中文空格会被误拆，引发坏数据日志和数据丢失。 */
    int count = 0;
    char* tok = strtok(line, "\t\r\n");
    while (tok && count < max_fields) {
        fields[count++] = tok;
        tok = strtok(NULL, "\t\r\n");
    }
    return count;
}

static void saveDoctors(void) {
    FILE* fp = openDataFile(FILE_DOCTOR, "w");
    DoctorNode* d = g_doctorHead;
    if (!fp) return;
    while (d) {
        fprintf(fp, "%d\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            d->docId, d->name, d->level, d->department,
            d->schedule[0], d->schedule[1], d->schedule[2], d->schedule[3],
            d->schedule[4], d->schedule[5], d->schedule[6],
            d->currentLoad, d->isDeleted);
        d = d->next;
    }
    fclose(fp);
}

static void savePatients(void) {
    FILE* fp = openDataFile(FILE_PATIENT, "w");
    PatientNode* p = g_patientHead;
    if (!fp) return;
    while (p) {
        fprintf(fp, "%d\t%s\t%d\t%s\t%lld\t%d\n",
            p->patientId, p->name, p->age, p->lastRegId,
            (long long)p->balanceCents, p->isDeleted);
        p = p->next;
    }
    fclose(fp);
}

static void saveMedicines(void) {
    FILE* fp = openDataFile(FILE_MEDICINE, "w");
    MedicineNode* m = g_medicineHead;
    if (!fp) return;
    while (m) {
        fprintf(fp, "%d\t%s\t%s\t%s\t%lld\t%d\t%s\t%d\n",
            m->medId, m->officialName, m->tradeName, m->aliasName,
            (long long)m->priceCents, m->stock, m->relatedDept, m->isDeleted);
        m = m->next;
    }
    fclose(fp);
}

static void saveRecords(void) {
    FILE* fp = openDataFile(FILE_RECORD, "w");
    RecordNode* r = g_recordHead;
    if (!fp) return;
    while (r) {
        fprintf(fp, "%s\t%d\t%s\t%d\t%s\t%d\t%s\t%lld\t%d\t%d\t%lld\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n",
            r->recordId, r->patientId, r->patientName, r->docId, r->docName,
            r->type, r->diagnosis, (long long)r->checkFeeCents, r->medId, r->medCount,
            (long long)r->totalCostCents, r->status, r->wardId, r->bedId, r->date,
            r->hour, r->minute, r->isRedInk, r->isDeleted);
        r = r->next;
    }
    fclose(fp);
}

static void saveBeds(void) {
    FILE* fp = openDataFile(FILE_WARD_BED, "w");
    BedNode* b = g_bedHead;
    if (!fp) return;
    while (b) {
        fprintf(fp, "%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
            b->wardId, b->bedId, b->wardType, b->relatedDept,
            b->bedStatus, b->patientId, b->isDeleted);
        b = b->next;
    }
    fclose(fp);
}

static void saveInpatients(void) {
    FILE* fp = openDataFile(FILE_INPATIENT, "w");
    InpatientNode* ip = g_inpatientHead;
    if (!fp) return;
    while (ip) {
        fprintf(fp, "%d\t%d\t%d\t%d\t%lld\t%lld\t%lld\t%lld\t%s\t%s\t%d\t%d\t%d\t%d\t%d\n",
            ip->patientId, ip->docId, ip->wardId, ip->bedId,
            (long long)ip->depositTotalCents, (long long)ip->depositBalanceCents,
            (long long)ip->dailyFeeCents, (long long)ip->totalChargedCents,
            ip->admitDate, ip->lastChargeDate, ip->admitHour,
            ip->expectedDays, ip->daysStayed, ip->isAdmitted, ip->isDeleted);
        ip = ip->next;
    }
    fclose(fp);
}

static void saveSystemState(void) {
    /* 修复 Bug 04：增加 g_nextRegSeq 和 g_lastRegDate 字段
     * 文件格式（共 11 字段，空格分隔）：
     *   nextPatientId nextRecordSeq nextWardId nextDeptId
     *   nextRegSeq lastRegDate
     *   currentDate currentHour currentMinute currentWeekday hospitalRevenue
     * lastRegDate 为空时输出 "-"。 */
    FILE* fp = openDataFile(FILE_SYSTEM, "w");
    const char* lastRegDateOut = (g_lastRegDate[0] != '\0') ? g_lastRegDate : "-";
    if (!fp) return;
    fprintf(fp, "%d %d %d %d %d %s %s %d %d %d %lld\n",
        g_nextPatientId, g_nextRecordSeq, g_nextWardId, g_nextDeptId,
        g_nextRegSeq, lastRegDateOut,
        g_currentDate, g_currentHour, g_currentMinute, g_currentWeekday,
        (long long)g_hospitalRevenue);
    fclose(fp);
}

void WriteLog(const char* log_file, const char* message) {
    FILE* fp = fopen(log_file, "a");
    time_t now;
    struct tm* tmInfo;
    if (!fp) return;
    time(&now);
    tmInfo = localtime(&now);
    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
        tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
        tmInfo->tm_hour, tmInfo->tm_min, tmInfo->tm_sec, message);
    fclose(fp);
}

void LogBadLine(const char* filename, int line_no, const char* raw_line) {
    char msg[512];
    snprintf(msg, sizeof(msg), "[坏数据] 文件=%s 行=%d 内容=%s", filename, line_no, raw_line);
    WriteLog(LOG_BAD_FILE, msg);
}

static void loadDoctors(void) {
    FILE* fp = openDataFile(FILE_DOCTOR, "r");
    char line[1024];
    int lineNo = 0;
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char* fields[16];
        int n;
        DoctorNode* d;
        lineNo++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        n = split_ws(line, fields, 16);
        if (n != 12 && n != 13) {
            LogBadLine(FILE_DOCTOR, lineNo, line);
            continue;
        }
        d = (DoctorNode*)malloc(sizeof(DoctorNode));
        if (!d) continue;
        memset(d, 0, sizeof(DoctorNode));
        d->docId = atoi(fields[0]);
        strncpy(d->name, fields[1], MAX_NAME_LEN - 1);
        strncpy(d->level, fields[2], MAX_LEVEL_LEN - 1);
        strncpy(d->department, fields[3], MAX_DEPT_LEN - 1);
        d->schedule[0] = atoi(fields[4]);
        d->schedule[1] = atoi(fields[5]);
        d->schedule[2] = atoi(fields[6]);
        d->schedule[3] = atoi(fields[7]);
        d->schedule[4] = atoi(fields[8]);
        d->schedule[5] = atoi(fields[9]);
        d->schedule[6] = atoi(fields[10]);
        d->currentLoad = atoi(fields[11]);
        d->isDeleted = (n >= 13) ? atoi(fields[12]) : 0;
        appendDoctor(d);
        appendDepartmentIfAbsent(d->department);
    }
    fclose(fp);
}

static void loadPatients(void) {
    FILE* fp = openDataFile(FILE_PATIENT, "r");
    char line[1024];
    int lineNo = 0;
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char* fields[8];
        int n;
        PatientNode* p;
        lineNo++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        n = split_ws(line, fields, 8);
        if (n != 5 && n != 6) {
            LogBadLine(FILE_PATIENT, lineNo, line);
            continue;
        }
        p = (PatientNode*)malloc(sizeof(PatientNode));
        if (!p) continue;
        memset(p, 0, sizeof(PatientNode));
        p->patientId = atoi(fields[0]);
        strncpy(p->name, fields[1], MAX_NAME_LEN - 1);
        p->age = atoi(fields[2]);
        strncpy(p->lastRegId, fields[3], MAX_REG_ID_LEN - 1);
        p->balanceCents = atoll(fields[4]);
        p->isDeleted = (n >= 6) ? atoi(fields[5]) : 0;
        appendPatient(p);
        if (p->patientId >= g_nextPatientId) g_nextPatientId = p->patientId + 1;
    }
    fclose(fp);
}

static void loadMedicines(void) {
    FILE* fp = openDataFile(FILE_MEDICINE, "r");
    char line[1024];
    int lineNo = 0;
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char* fields[10];
        int n;
        MedicineNode* m;
        lineNo++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        n = split_ws(line, fields, 10);
        if (n != 7 && n != 8) {
            LogBadLine(FILE_MEDICINE, lineNo, line);
            continue;
        }
        m = (MedicineNode*)malloc(sizeof(MedicineNode));
        if (!m) continue;
        memset(m, 0, sizeof(MedicineNode));
        m->medId = atoi(fields[0]);
        strncpy(m->officialName, fields[1], MAX_MED_NAME_LEN - 1);
        strncpy(m->tradeName, fields[2], MAX_MED_NAME_LEN - 1);
        strncpy(m->aliasName, fields[3], MAX_MED_NAME_LEN - 1);
        m->priceCents = atoll(fields[4]);
        m->stock = atoi(fields[5]);
        strncpy(m->relatedDept, fields[6], MAX_DEPT_LEN - 1);
        m->isDeleted = (n >= 8) ? atoi(fields[7]) : 0;
        appendMedicine(m);
        appendDepartmentIfAbsent(m->relatedDept);
    }
    fclose(fp);
}

static void loadRecords(void) {
    FILE* fp = openDataFile(FILE_RECORD, "r");
    char line[2048];
    int lineNo = 0;
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char* fields[24];
        int n;
        RecordNode* r;
        lineNo++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        n = split_ws(line, fields, 24);
        if (n != 12 && n != 19) {
            LogBadLine(FILE_RECORD, lineNo, line);
            continue;
        }
        r = (RecordNode*)malloc(sizeof(RecordNode));
        if (!r) continue;
        memset(r, 0, sizeof(RecordNode));
        strncpy(r->recordId, fields[0], MAX_REC_ID_LEN - 1);
        r->patientId = atoi(fields[1]);
        strncpy(r->patientName, fields[2], MAX_NAME_LEN - 1);
        r->docId = atoi(fields[3]);
        strncpy(r->docName, fields[4], MAX_NAME_LEN - 1);
        r->type = atoi(fields[5]);
        /* V5 修复 Bug 31：校验 type 字段范围（1~7） */
        if (r->type < 1 || r->type > 7) {
            LogBadLine(FILE_RECORD, lineNo, line);
            free(r);
            continue;
        }
        strncpy(r->diagnosis, fields[6], MAX_DIAG_LEN - 1);
        r->checkFeeCents = atoll(fields[7]);
        r->medId = atoi(fields[8]);
        r->medCount = atoi(fields[9]);
        r->totalCostCents = atoll(fields[10]);
        r->status = atoi(fields[11]);
        r->wardId = 0;
        r->bedId = 0;
        strncpy(r->date, g_currentDate, MAX_DATE_LEN - 1);
        r->hour = 0;
        r->minute = 0;
        r->isRedInk = 0;
        r->isDeleted = 0;
        if (n == 19) {
            r->wardId = atoi(fields[12]);
            r->bedId = atoi(fields[13]);
            strncpy(r->date, fields[14], MAX_DATE_LEN - 1);
            r->hour = atoi(fields[15]);
            r->minute = atoi(fields[16]);
            r->isRedInk = atoi(fields[17]);
            r->isDeleted = atoi(fields[18]);
        }
        appendRecord(r);
        g_nextRecordSeq++;
    }
    fclose(fp);
}

static void loadBeds(void) {
    FILE* fp = openDataFile(FILE_WARD_BED, "r");
    char line[1024];
    int lineNo = 0;
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char* fields[10];
        int n;
        BedNode* b;
        lineNo++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        n = split_ws(line, fields, 10);
        if (n != 7) {
            LogBadLine(FILE_WARD_BED, lineNo, line);
            continue;
        }
        b = (BedNode*)malloc(sizeof(BedNode));
        if (!b) continue;
        memset(b, 0, sizeof(BedNode));
        b->wardId = atoi(fields[0]);
        b->bedId = atoi(fields[1]);
        b->wardType = atoi(fields[2]);
        strncpy(b->relatedDept, fields[3], MAX_DEPT_LEN - 1);
        b->bedStatus = atoi(fields[4]);
        b->patientId = atoi(fields[5]);
        b->isDeleted = atoi(fields[6]);
        appendBed(b);
        if (b->wardId >= g_nextWardId) g_nextWardId = b->wardId + 1;
    }
    fclose(fp);
}

static void loadInpatients(void) {
    FILE* fp = openDataFile(FILE_INPATIENT, "r");
    char line[1024];
    int lineNo = 0;
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char* fields[18];
        int n;
        InpatientNode* ip;
        lineNo++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        n = split_ws(line, fields, 18);
        if (n != 15) {
            LogBadLine(FILE_INPATIENT, lineNo, line);
            continue;
        }
        ip = (InpatientNode*)malloc(sizeof(InpatientNode));
        if (!ip) continue;
        memset(ip, 0, sizeof(InpatientNode));
        ip->patientId = atoi(fields[0]);
        ip->docId = atoi(fields[1]);
        ip->wardId = atoi(fields[2]);
        ip->bedId = atoi(fields[3]);
        ip->depositTotalCents = atoll(fields[4]);
        ip->depositBalanceCents = atoll(fields[5]);
        ip->dailyFeeCents = atoll(fields[6]);
        ip->totalChargedCents = atoll(fields[7]);
        strncpy(ip->admitDate, fields[8], MAX_DATE_LEN - 1);
        strncpy(ip->lastChargeDate, fields[9], MAX_DATE_LEN - 1);
        ip->admitHour = atoi(fields[10]);
        ip->expectedDays = atoi(fields[11]);
        ip->daysStayed = atoi(fields[12]);
        ip->isAdmitted = atoi(fields[13]);
        ip->isDeleted = atoi(fields[14]);
        appendInpatient(ip);
    }
    fclose(fp);
}

static void loadSystemState(void) {
    /* 修复 Bug 04 + Bug 06：兼容新旧两种格式
     *   新格式（11 字段）：nextPatientId nextRecordSeq nextWardId nextDeptId
     *                     nextRegSeq lastRegDate
     *                     currentDate currentHour currentMinute currentWeekday hospitalRevenue
     *   旧格式（9 字段） ：nextPatientId nextRecordSeq nextWardId nextDeptId
     *                     currentDate currentHour currentMinute currentWeekday hospitalRevenue
     */
    FILE* fp = openDataFile(FILE_SYSTEM, "r");
    char line[512];
    char lastRegDate[MAX_DATE_LEN];
    long long revenue;
    int n;
    if (!fp) return;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return;
    }
    n = sscanf(line,
        "%d %d %d %d %d %15s %15s %d %d %d %lld",
        &g_nextPatientId, &g_nextRecordSeq, &g_nextWardId, &g_nextDeptId,
        &g_nextRegSeq, lastRegDate,
        g_currentDate, &g_currentHour, &g_currentMinute, &g_currentWeekday,
        &revenue);
    if (n == 11) {
        g_hospitalRevenue = (Money)revenue;
        if (strcmp(lastRegDate, "-") == 0) {
            g_lastRegDate[0] = 0;
        }
        else {
            strncpy(g_lastRegDate, lastRegDate, MAX_DATE_LEN - 1);
            g_lastRegDate[MAX_DATE_LEN - 1] = 0;
        }
        fclose(fp);
        return;
    }
    /* 兼容旧 9 字段格式 */
    n = sscanf(line, "%d %d %d %d %15s %d %d %d %lld",
        &g_nextPatientId, &g_nextRecordSeq, &g_nextWardId, &g_nextDeptId,
        g_currentDate, &g_currentHour, &g_currentMinute, &g_currentWeekday,
        &revenue);
    if (n == 9) {
        g_hospitalRevenue = (Money)revenue;
        g_nextRegSeq = 0;
        g_lastRegDate[0] = 0;
    }
    if (n != 9 && n != 11) {
        /* 修复 Bug 06：注释由 GBK 乱码改回 UTF-8 中文。文件为空或损坏时，保留链表默认值。 */
    }
    fclose(fp);
}

void SaveAllData(void) {
    char msg[256];
    saveDoctors();
    savePatients();
    saveMedicines();
    saveRecords();
    saveBeds();
    saveInpatients();
    saveSystemState();

    snprintf(msg, sizeof(msg),
        "[保存] doctors=%d patients=%d medicines=%d records=%d beds=%d",
        getDoctorCount(), getPatientCount(), getMedicineCount(), getRecordCount(), getBedCount());
    WriteLog(LOG_OP_FILE, msg);
}

void LoadAllData(void) {
    releaseAllData();
    g_nextPatientId = 2001;
    g_nextRecordSeq = 1;
    g_nextWardId = 1;
    g_nextDeptId = 1;
    loadDoctors();
    loadPatients();
    loadMedicines();
    loadRecords();
    loadBeds();
    loadInpatients();
    loadSystemState();
    if (!g_bedHead) initDefaultDepartmentsAndBeds();
}
