#include "pharmacy.h"
#include <limits.h>   /* 修复 Bug 01：补 INT_MAX 的头文件，跨平台编译 */

MedicineNode* FindMedicineByIdNode(int medicineId) {
    return findMedicineById(medicineId);
}

int DeductMedicineStock(int medicineId, int quantity) {
    MedicineNode* m = findMedicineById(medicineId);
    if (!m) return -1;
    if (quantity <= 0 || quantity > MAX_PRESCRIPTION_QTY) return -1;
    if (m->stock < quantity) return -1;
    m->stock -= quantity;
    return 0;
}

void AddMedicineStock(int medicineId, int quantity) {
    MedicineNode* m = findMedicineById(medicineId);
    if (!m || quantity <= 0) return;
    m->stock += quantity;
}

int CountStockWarnings(void) {
    /* V5 修复 Bug 28：阈值由 10 提升到 STOCK_WARNING_THRESHOLD（50），
     * 与初始数据匹配，使预警功能可观察。 */
    int count = 0;
    MedicineNode* m = g_medicineHead;
    while (m) {
        if (!m->isDeleted && m->stock < STOCK_WARNING_THRESHOLD) count++;
        m = m->next;
    }
    return count;
}

static void listAllMedicines(void) {
    MedicineNode* m = g_medicineHead;
    printf("\n%-6s %-20s %-16s %-16s %-8s %-8s %-10s\n",
        "ID", "通用名", "商品名", "别名", "单价", "库存", "科室");
    printf("-------------------------------------------------------------------------------\n");
    while (m) {
        if (!m->isDeleted) {
            char money[32];
            formatMoney(m->priceCents, money, sizeof(money));
            printf("%-6d %-20s %-16s %-16s %-8s %-8d %-10s\n",
                m->medId, m->officialName, m->tradeName, m->aliasName,
                money, m->stock, m->relatedDept);
        }
        m = m->next;
    }
}

static void fuzzySearch(void) {
    char keyword[MAX_MED_NAME_LEN];
    MedicineNode* m = g_medicineHead;
    int found = 0;
    safeInputString("请输入药品关键字：", keyword, MAX_MED_NAME_LEN);
    if (strlen(keyword) == 0) {
        printf("请至少输入一个字符进行查询。\n");
        return;
    }
    while (m) {
        char idStr[16];
        sprintf(idStr, "%d", m->medId);
        if (!m->isDeleted &&
            (strContains(m->officialName, keyword) ||
                strContains(m->tradeName, keyword) ||
                strContains(m->aliasName, keyword) ||
                strContains(idStr, keyword))) {
            printf("[%d] %s / %s / %s 库存:%d 单价:",
                m->medId, m->officialName, m->tradeName, m->aliasName, m->stock);
            printMoney(m->priceCents);
            printf(" 科室:%s\n", m->relatedDept);
            found = 1;
        }
        m = m->next;
    }
    if (!found) printf("未找到匹配药品。\n");
}

static void stockIn(void) {
    int medId = safeInputInt("请输入药品ID：");
    int qty = safeInputInt("请输入入库数量：");
    MedicineNode* m = findMedicineById(medId);
    if (!m || qty <= 0) {
        printf("药品不存在或数量无效。\n");
        return;
    }
    if (m->stock > INT_MAX - qty) {
        printf("入库数量过大，会导致库存溢出，操作已取消。\n");
        return;
    }
    m->stock += qty;
    printf("入库成功，当前库存：%d\n", m->stock);
}

static void stockOut(void) {
    int medId = safeInputInt("请输入药品ID：");
    int qty = safeInputInt("请输入出库数量：");
    MedicineNode* m = findMedicineById(medId);
    if (!m || qty <= 0) {
        printf("药品不存在或数量无效。\n");
        return;
    }
    if (qty > MAX_PRESCRIPTION_QTY) {
        printf("每次出库不得超过 %d 盒。\n", MAX_PRESCRIPTION_QTY);
        return;
    }
    if (m->stock < qty) {
        printf("库存不足，当前库存 %d\n", m->stock);
        return;
    }
    m->stock -= qty;
    printf("出库成功，当前库存：%d\n", m->stock);
}

static void showWarnings(void) {
    /* V5 修复 Bug 28：阈值与 CountStockWarnings 同步 */
    MedicineNode* m = g_medicineHead;
    int count = 0;
    printf("（库存低于 %d 视为预警）\n", STOCK_WARNING_THRESHOLD);
    while (m) {
        if (!m->isDeleted && m->stock < STOCK_WARNING_THRESHOLD) {
            printf("[%d] %s 库存预警：%d\n", m->medId, m->tradeName, m->stock);
            count++;
        }
        m = m->next;
    }
    if (count == 0) printf("当前无库存预警。\n");
}

void PharmacyManagement(void) {
    int choice;
    while (1) {
        printf("\n================ 药房管理 ================\n");
        printf("1. 查看全部药品\n");
        printf("2. 药品模糊查询\n");
        printf("3. 药品入库\n");
        printf("4. 药品出库\n");
        printf("5. 库存预警\n");
        printf("6. 返回上级\n");
        printf("请选择：");
        choice = GetSafeInt();
        switch (choice) {
        case 1: listAllMedicines(); PauseScreen(); break;
        case 2: fuzzySearch(); PauseScreen(); break;
        case 3: stockIn(); PauseScreen(); break;
        case 4: stockOut(); PauseScreen(); break;
        case 5: showWarnings(); PauseScreen(); break;
        case 6: return;
        default: printf("无效选择。\n"); PauseScreen(); break;
        }
    }
}