#include "global.h"
#include <stdlib.h>
#include <windows.h>
#include <locale.h>

/* =======================
   全局数据表定义
   ======================= */

   /* 医生总表 */
Doctor doctor_table[MAX_DOCTOR];

/* 患者总表 */
Patient patient_table[MAX_PATIENT];

/* 诊疗记录总表 */
MedicalRecord record_table[MAX_RECORDS];

/* 药品总表 */
Medicine medicine_table[MAX_MED_TYPES];

/* 病房床位总表 */
WardBed bed_table[MAX_WARD * MAX_BED_PER_WARD];

/* 科室总表 */
Department department_table[MAX_DEPARTMENT];

/* =======================
   全局计数器定义
   ======================= */

   /* 当前医生数量 */
int doctor_count = 0;

/* 当前患者数量 */
int patient_count = 0;

/* 当前诊疗记录数量 */
int record_count = 0;

/* 当前药品数量 */
int medicine_count = 0;

/* 当前床位数量 */
int bed_count = 0;

/* 当前科室数量 */
int department_count = 0;

/* 医院当前营业额，单位：分 */
Money hospital_total_revenue = 0;

/* =======================
   模块函数声明
   ======================= */

   /* 数据加载模块 */
void LoadAllData();

/* 数据保存模块 */
void SaveAllData();

/* 挂号模块 */
void RegisterPatient();

/* 看诊模块 */
void SeeDoctor();

/* 住院/扣费模块 */
void ProcessBilling();

/* 药房管理模块 */
void PharmacyManagement();

/* 病房床位模块 */
void ShowWardStatus();

/* 查询检索模块 */
void SearchModule();

/* 统计报表模块 */
void StatisticModule();

/* =======================
   公共输入函数实现
   ======================= */

   /*
       功能：安全读取整数
       说明：
       1. 若输入不是整数，则提示重新输入
       2. 若输入整数后面带非法字符，也判定无效
       3. 自动清理当前行剩余输入
   */
int GetSafeInt() {
    int num;
    char ch;

    while (1) {
        if (scanf("%d", &num) == 1) {
            while ((ch = getchar()) != '\n' && ch != EOF) {
                if (ch != ' ' && ch != '\t') {
                    printf("[系统拦截] 输入包含非法字符，请重新输入纯数字: ");
                    while ((ch = getchar()) != '\n' && ch != EOF);
                    goto CONTINUE_INPUT;
                }
            }
            return num;
        }
        else {
            printf("[系统拦截] 格式无效，请输入纯数字: ");
            while ((ch = getchar()) != '\n' && ch != EOF);
        }

    CONTINUE_INPUT:
        ;
    }
}

/*
    功能：安全读取字符串
    参数：
    - buffer：目标字符串数组
    - max_len：数组最大长度

    说明：
    1. 使用 fgets 读取，避免溢出
    2. 自动去掉末尾换行符
    3. 若输入过长，则清理缓冲区剩余内容
*/
void GetSafeString(char* buffer, int max_len) {
    if (fgets(buffer, max_len, stdin) != NULL) {
        size_t len = strlen(buffer);

        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        else {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        }
    }
    else {
        buffer[0] = '\0';
    }
}

/*
    功能：智能联想演示接口
    说明：
    当前只是演示入口，后续可对接：
    1. 医生姓名模糊搜索
    2. 药品通用名/商品名/别名搜索
*/
void SmartSuggestDemo() {
    char keyword[50];

    printf("\n=== 智能联想引擎测试 ===\n");
    printf("请输入医生姓名或药品的部分关键字（如输入“阿”查找药品/医生）: ");

    GetSafeString(keyword, 50);

    printf("\n为您联想到以下结果：\n");
    printf("  [药品] %s 相关药品（后续接药房模块）\n", keyword);
    printf("  [医生] %s 相关医生（后续接搜索模块）\n", keyword);
    printf("---------------------------\n");
}

/* =======================
   主函数
   ======================= */

int main() {
    /*
        Windows 控制台中文兼容设置
        若仍乱码，可再检查：
        1. 源文件保存编码
        2. VS 控制台字体
    */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    setlocale(LC_ALL, ".UTF-8");

    /* 系统启动时默认营业额为0 */
    hospital_total_revenue = 0;

    /* 启动时读取数据 */
    LoadAllData();

    /* 主循环控制变量 */
    int running = 1;

    while (running) {
        /* Windows 下清屏 */
        system("cls");

        /* 主菜单界面 */
        printf("====================================================\n");
        printf("                 智慧医疗管理系统                   \n");
        printf("====================================================\n");
        printf("  1. 挂号中心（建立挂号记录）\n");
        printf("  2. 门诊诊室（看诊or检查or开药）\n");
        printf("  3. 住院与床位管理（办理住院or押金扣费）\n");
        printf("  4. 药房出入库管理\n");
        printf("  5. 多维度信息查询\n");
        printf("  6. 统计报表分析\n");
        printf("  7. 智能输入联想引擎\n");
        printf("  8. 安全存盘并退出系统\n");
        printf("----------------------------------------------------\n");

        ///* 彻底重写的清洁版菜单 */
        //printf("====================================================\n");
        //printf("                 智慧医疗管理系统                   \n");
        //printf("====================================================\n");
        //printf("  1. 挂号中心\n");
        //printf("  2. 门诊诊室\n");
        //printf("  3. 住院管理\n");
        //printf("  4. 药房管理\n");
        //printf("  5. 信息查询\n");
        //printf("  6. 统计报表\n");
        //printf("  7. 智能联想\n");
        //printf("  8. 退出系统\n");
        //printf("----------------------------------------------------\n");
        //printf("请选择操作指令: ");

        /* 读取菜单选项 */
        int choice = GetSafeInt();

        /* 根据菜单选项进入对应功能 */
        switch (choice) {
        case 1:
            RegisterPatient();
            break;

        case 2:
            SeeDoctor();
            break;

        case 3:
            ProcessBilling();
            break;

        case 4:
            PharmacyManagement();
            break;

        case 5:
            SearchModule();
            break;

        case 6:
            StatisticModule();
            break;

        case 7:
            SmartSuggestDemo();
            printf("\n按回车键继续...");
            getchar();
            break;

        case 8:
            SaveAllData();
            printf("\n数据安全写入完成，程序退出。\n");
            running = 0;
            break;

        default:
            printf("指令未识别，请重新输入。\n");
            printf("按回车键继续...");
            getchar();
        }
    }

    return 0;
}

/* =======================
   临时桩函数
   ======================= */

   /* 启动时加载全部数据 */
void LoadAllData() {
    printf("[System] 数据加载模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 退出前保存全部数据 */
void SaveAllData() {
    printf("[System] 数据保存模块待接入...\n");
}

/* 挂号模块入口 */
void RegisterPatient() {
    printf("[Module A] 挂号模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 看诊模块入口 */
void SeeDoctor() {
    printf("[Module A] 看诊模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 住院与财务模块入口 */
void ProcessBilling() {
    printf("[Module A] 住院财务模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 药房模块入口 */
void PharmacyManagement() {
    printf("[Module C] 药房模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 病房床位模块入口 */
void ShowWardStatus() {
    printf("[Module C] 床位模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 查询模块入口 */
void SearchModule() {
    printf("[Module D] 搜索模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}

/* 统计模块入口 */
void StatisticModule() {
    printf("[Leader] 统计报表模块待接入...\n");
    printf("按回车键继续...");
    getchar();
}