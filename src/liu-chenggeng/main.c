/*************************************************************
 * main.c  —  医疗管理系统 临时主程序（测试驱动）
 *
 * 用于测试刘承庚负责的三大对外函数：
 *   RegisterPatient() / SeeDoctor() / ProcessBilling()
 *
 * 后续由钟佳凌和周溢程替换为正式的初始化和持久化模块。
 *************************************************************/
#include "outpatient_inpatient.h"

/* ============ 临时演示数据 ============ */

static void addDoctor(int id, const char *name, const char *level,
                      const char *dept, int s0, int s1, int s2, int s3,
                      int s4, int s5, int s6, int load) {
    DoctorNode *d = (DoctorNode *)malloc(sizeof(DoctorNode));
    memset(d, 0, sizeof(DoctorNode));
    d->docId = id;
    strncpy(d->name, name, MAX_NAME_LEN - 1);
    strncpy(d->level, level, MAX_LEVEL_LEN - 1);
    strncpy(d->department, dept, MAX_DEPT_LEN - 1);
    d->schedule[0] = s0; d->schedule[1] = s1; d->schedule[2] = s2;
    d->schedule[3] = s3; d->schedule[4] = s4; d->schedule[5] = s5;
    d->schedule[6] = s6;
    d->currentLoad = load;
    if (!g_doctorHead) g_doctorHead = d;
    else { DoctorNode *t = g_doctorHead; while (t->next) t = t->next; t->next = d; }
}

static void addPatient(int id, const char *name, int age,
                       const char *regId, int balance) {
    PatientNode *p = (PatientNode *)malloc(sizeof(PatientNode));
    memset(p, 0, sizeof(PatientNode));
    p->patientId = id;
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->age = age;
    strncpy(p->lastRegId, regId, MAX_REG_ID_LEN - 1);
    p->balanceCents = balance;
    if (!g_patientHead) g_patientHead = p;
    else { PatientNode *t = g_patientHead; while (t->next) t = t->next; t->next = p; }
    if (id >= g_nextPatientId) g_nextPatientId = id + 1;
}

static void addMedicine(int id, const char *off, const char *tra,
                        const char *ali, int price, int stock, const char *dept) {
    MedicineNode *m = (MedicineNode *)malloc(sizeof(MedicineNode));
    memset(m, 0, sizeof(MedicineNode));
    m->medId = id;
    strncpy(m->officialName, off, MAX_MED_NAME_LEN - 1);
    strncpy(m->tradeName,    tra, MAX_MED_NAME_LEN - 1);
    strncpy(m->aliasName,    ali, MAX_MED_NAME_LEN - 1);
    m->priceCents = price;
    m->stock = stock;
    strncpy(m->relatedDept, dept, MAX_DEPT_LEN - 1);
    if (!g_medicineHead) g_medicineHead = m;
    else { MedicineNode *t = g_medicineHead; while (t->next) t = t->next; t->next = m; }
}

static void addBed(int id, int wardType, const char *dept) {
    BedNode *b = (BedNode *)malloc(sizeof(BedNode));
    memset(b, 0, sizeof(BedNode));
    b->bedId = id;
    b->wardType = wardType;
    strncpy(b->relatedDept, dept, MAX_DEPT_LEN - 1);
    b->bedStatus = BED_FREE;
    if (!g_bedHead) g_bedHead = b;
    else { BedNode *t = g_bedHead; while (t->next) t = t->next; t->next = b; }
}

static void initMockData(void) {
    printf("正在加载演示数据...\n");

    addDoctor(1001, "张伟", "主任医师",   "内科",   1,0,1,0,1,0,0, 5);
    addDoctor(1002, "王芳", "副主任医师", "外科",   0,1,0,1,0,1,0, 3);
    addDoctor(1003, "李娜", "主治医师",   "儿科",   1,1,1,1,1,0,0, 8);
    addDoctor(1004, "刘强", "住院医师",   "急诊科", 1,1,1,1,1,1,1, 12);
    addDoctor(1005, "陈静", "主任医师",   "妇产科", 0,0,1,0,1,0,1, 2);
    addDoctor(1011, "孙博", "主治医师",   "骨科",   1,0,1,0,0,0,0, 2);
    addDoctor(1012, "马丽", "主任医师",   "心内科", 1,1,1,1,1,0,0, 5);

    addPatient(2001, "赵一", 25, "REG2026040101", 500000);
    addPatient(2002, "钱二", 34, "REG2026040102", 12500);
    addPatient(2003, "孙三", 62, "REG2026040103", 880000);
    addPatient(2004, "李四", 12, "REG2026040104", 3000);
    addPatient(2005, "周五", 45, "REG2026040105", 150000);
    addPatient(2006, "吴六", 29, "REG2026040106", 2000000);

    addMedicine(3001, "阿莫西林胶囊",   "益萨林",     "消炎药", 1850, 500, "内科");
    addMedicine(3002, "对乙酰氨基酚片", "泰诺林",     "退烧片", 2500, 200, "儿科");
    addMedicine(3003, "生理盐水",       "舒泰",       "盐水",   500,  1000,"全科");
    addMedicine(3005, "布洛芬缓释胶囊", "芬必得",     "布洛芬", 2880, 300, "外科");
    addMedicine(3013, "硝苯地平控释片", "拜新同",     "降压药", 6200, 200, "心内科");

    addBed(101, WARD_NORMAL, "内科");
    addBed(102, WARD_NORMAL, "内科");
    addBed(201, WARD_NORMAL, "外科");
    addBed(301, WARD_ICU,    "急诊科");
    addBed(401, WARD_VIP,    "内科");
    addBed(501, WARD_NORMAL, "儿科");
    addBed(601, WARD_NORMAL, "骨科");

    printf("  加载完成。\n\n");
}

static void releaseAllData(void) {
    PatientNode *p = g_patientHead;
    while (p) { PatientNode *t = p; p = p->next; free(t); }
    DoctorNode *d = g_doctorHead;
    while (d) { DoctorNode *t = d; d = d->next; free(t); }
    MedicineNode *m = g_medicineHead;
    while (m) { MedicineNode *t = m; m = m->next; free(t); }
    RecordNode *r = g_recordHead;
    while (r) { RecordNode *t = r; r = r->next; free(t); }
    BedNode *b = g_bedHead;
    while (b) { BedNode *t = b; b = b->next; free(t); }
    InpatientNode *ip = g_inpatientHead;
    while (ip) { InpatientNode *t = ip; ip = ip->next; free(t); }
}

static void setSystemTime(void) {
    printf("\n当前: %s %02d:00 (星期%d)\n",
           g_currentDate, g_currentHour, g_currentWeekday + 1);
    safeInputString("请输入新日期 (yyyymmdd): ", g_currentDate, 12);
    g_currentHour = safeInputInt("请输入当前小时 (0~23): ");
    g_currentWeekday = safeInputInt("请输入星期 (0=周一..6=周日): ");
    if (g_currentWeekday < 0 || g_currentWeekday > 6) g_currentWeekday = 0;
    if (g_currentHour < 0 || g_currentHour > 23) g_currentHour = 9;
    printf("已更新: %s %02d:00 (星期%d)\n",
           g_currentDate, g_currentHour, g_currentWeekday + 1);
}

int main(void) {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      医 疗 管 理 系 统 (HIS)             ║\n");
    printf("║      第12组 · 程序设计基础课程设计        ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    initMockData();

    int running = 1;
    while (running) {
        printf("\n┌──────────────────────────────────────────┐\n");
        printf("│              主 菜 单                    │\n");
        printf("├──────────────────────────────────────────┤\n");
        printf("│  1. RegisterPatient  挂号                │\n");
        printf("│  2. SeeDoctor        看诊                │\n");
        printf("│  3. ProcessBilling   住院管理            │\n");
        printf("│  4. 设置系统时间                         │\n");
        printf("│  0. 退出                                 │\n");
        printf("└──────────────────────────────────────────┘\n");
        printf("  当前: %s %02d:00 星期%d | 今日挂号: %d/%d\n",
               g_currentDate, g_currentHour, g_currentWeekday + 1,
               g_dailyRegCount, DAILY_MAX_TICKETS);

        int choice = safeInputInt("请选择: ");
        switch (choice) {
            case 1: RegisterPatient(); break;
            case 2: SeeDoctor();       break;
            case 3: ProcessBilling();  break;
            case 4: setSystemTime();   break;
            case 0: running = 0;       break;
            default: printf("[错误] 无效选择！\n"); break;
        }
    }

    releaseAllData();
    printf("再见！\n");
    return 0;
}
