#include "ward.h"

static const char* getBedStatusName(int status) {
    switch (status) {
    case BED_FREE: return "空闲";
    case BED_OCCUPIED: return "占用";
    case BED_MAINTENANCE: return "维护";
    default: return "未知";
    }
}

void GetBedStatistics(int* totalFree, int* totalOccupied, int* totalMaintenance,
    int* normalFree, int* icuFree, int* vipFree) {
    BedNode* b = g_bedHead;
    if (totalFree) *totalFree = 0;
    if (totalOccupied) *totalOccupied = 0;
    if (totalMaintenance) *totalMaintenance = 0;
    if (normalFree) *normalFree = 0;
    if (icuFree) *icuFree = 0;
    if (vipFree) *vipFree = 0;

    while (b) {
        if (!b->isDeleted) {
            if (b->bedStatus == BED_FREE) {
                if (totalFree) (*totalFree)++;
                if (b->wardType == WARD_NORMAL && normalFree) (*normalFree)++;
                if (b->wardType == WARD_ICU && icuFree) (*icuFree)++;
                if (b->wardType == WARD_VIP && vipFree) (*vipFree)++;
            }
            else if (b->bedStatus == BED_OCCUPIED) {
                if (totalOccupied) (*totalOccupied)++;
            }
            else if (b->bedStatus == BED_MAINTENANCE) {
                if (totalMaintenance) (*totalMaintenance)++;
            }
        }
        b = b->next;
    }
}

static void listBeds(const char* deptFilter) {
    BedNode* b = g_bedHead;
    printf("\n%-8s %-8s %-10s %-12s %-8s %-10s\n",
        "病房ID", "床位ID", "病房类型", "关联科室", "床位状态", "患者ID");
    printf("---------------------------------------------------------------------\n");
    while (b) {
        if (!b->isDeleted) {
            if (!deptFilter || strcmp(deptFilter, "all") == 0 || strcmp(b->relatedDept, deptFilter) == 0) {
                printf("%-8d %-8d %-10s %-12s %-8s %-10d\n",
                    b->wardId, b->bedId, getWardTypeName(b->wardType),
                    b->relatedDept, getBedStatusName(b->bedStatus), b->patientId);
            }
        }
        b = b->next;
    }
}

int AssignBedToPatient(int patientId, const char* preferredDept, int preferredWardType) {
    BedNode* b = g_bedHead;
    while (b) {
        if (!b->isDeleted && b->bedStatus == BED_FREE) {
            if ((preferredWardType == 0 || b->wardType == preferredWardType) &&
                (!preferredDept || strcmp(preferredDept, "all") == 0 || strcmp(b->relatedDept, preferredDept) == 0)) {
                b->bedStatus = BED_OCCUPIED;
                b->patientId = patientId;
                return b->bedId;
            }
        }
        b = b->next;
    }
    return 0;
}

void ReleaseBed(int bedId) {
    BedNode* b = findBedById(bedId);
    if (!b) return;
    b->bedStatus = BED_FREE;
    b->patientId = 0;
}

void ChangeBedStatus(void) {
    /* 修复 Bug 07：限制床位状态转换，避免绕过住院流程造成数据不一致。
     * 规则：
     *   - 占用 → 仅允许切换为 维护，且要求该床当前没有"在院"记录；
     *     否则提示先走出院结算。
     *   - 空闲 ↔ 维护：允许互转。
     *   - 任何状态 → 占用：不允许手动设置（必须通过住院办理流程）。
     */
    int bedId = safeInputInt("请输入床位ID：");
    BedNode* b = findBedById(bedId);
    if (!b) {
        printf("未找到床位。\n");
        return;
    }
    while (1) {
        int newStatus;
        printf("当前状态=%s，输入新状态(0空闲/1占用/2维护)：", getBedStatusName(b->bedStatus));
        newStatus = GetSafeInt();
        if (newStatus < BED_FREE || newStatus > BED_MAINTENANCE) {
            printf("床位状态输入非法。\n");
            continue;
        }
        if (newStatus == b->bedStatus) {
            printf("床位状态未发生变化。\n");
            return;
        }
        if (newStatus == BED_OCCUPIED) {
            printf("不允许手动将床位设为占用，请通过住院办理入口分配。\n");
            return;
        }
        if (b->bedStatus == BED_OCCUPIED) {
            InpatientNode* ip = g_inpatientHead;
            int hasActive = 0;
            while (ip) {
                if (!ip->isDeleted && ip->isAdmitted &&
                    ip->wardId == b->wardId && ip->bedId == b->bedId) {
                    hasActive = 1;
                    break;
                }
                ip = ip->next;
            }
            if (hasActive) {
                printf("该床位仍有在院患者，请先在住院管理中完成出院结算。\n");
                return;
            }
            if (newStatus != BED_MAINTENANCE) {
                printf("占用床位仅允许切换为维护状态。\n");
                return;
            }
        }
        b->bedStatus = newStatus;
        if (b->bedStatus == BED_FREE) b->patientId = 0;
        printf("修改完成。\n");
        break;
    }
}

void ShowWardStatus(void) {
    int choice;
    while (1) {
        printf("\n================ 病房床位管理 ================\n");
        printf("1. 查看全部床位\n");
        printf("2. 按科室筛选查看\n");
        printf("3. 手动修改床位状态\n");
        printf("4. 返回上级\n");
        printf("请选择：");
        choice = GetSafeInt();
        switch (choice) {
        case 1:
            listBeds("all");
            PauseScreen();
            break;
        case 2: {
            char dept[MAX_DEPT_LEN];
            safeInputString("请输入科室名称（all表示全部）：", dept, MAX_DEPT_LEN);
            listBeds(dept);
            PauseScreen();
            break;
        }
        case 3:
            ChangeBedStatus();
            PauseScreen();
            break;
        case 4:
            return;
        default:
            printf("无效选择。\n");
            PauseScreen();
            break;
        }
    }
}

void InitDefaultWardsAndBeds(void) {
    initDefaultDepartmentsAndBeds();
}
