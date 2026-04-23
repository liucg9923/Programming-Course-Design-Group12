#pragma once
#ifndef GLOBAL_H
#define GLOBAL_H

/*
    global.h
    -------------------------------------------------
    作用：
    1. 定义系统使用的全局常量
    2. 定义统一的金额类型
    3. 定义核心结构体
    4. 声明全局数据表和计数器
    5. 声明公共函数

    设计要求：
    - 采用“扁平化结构体 + ID关联”的方式
    - 金额统一使用 long long，单位为“分”
    - 所有模块都必须引用本头文件中的统一定义
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

/* =======================
   全局容量常量定义
   ======================= */

   /* 患者最大数量，包含门诊和住院患者 */
#define MAX_PATIENT 300

/* 医生最大数量 */
#define MAX_DOCTOR 50

/* 系统可存储的诊疗记录最大数量 */
#define MAX_RECORDS 5000

/* 药品种类最大数量 */
#define MAX_MED_TYPES 100

/* 病房最大数量 */
#define MAX_WARD 20

/* 每个病房的最大床位数 */
#define MAX_BED_PER_WARD 5

/* 科室最大数量 */
#define MAX_DEPARTMENT 10

/* 押金预警线：1000元，单位为“分” */
#define DEPOSIT_LIMIT 100000

/* =======================
   基础类型定义
   ======================= */

   /* 金额类型：统一使用“分”存储，避免浮点误差 */
typedef long long Money;

/* =======================
   医生信息结构体
   ======================= */
typedef struct {
    int staff_id;                 // 医生工号，唯一标识医生
    char name[50];                // 医生姓名，允许重名
    char level[30];               // 医生级别：主任医师/副主任医师/主治医师/住院医师
    char department[50];          // 所属科室名称

    /*
        出诊日期数组：
        下标 0~6 对应周一到周日
        值为 1 表示当天出诊
        值为 0 表示当天不出诊
    */
    int work_days[7];

    int current_load;             // 当前接诊量，用于繁忙度统计
} Doctor;

/* =======================
   患者信息结构体
   ======================= */
typedef struct {
    int patient_id;               // 患者ID，唯一
    char name[50];                // 患者姓名，允许重名
    int age;                      // 患者年龄

    /*
        最近一次挂号号
        为尽量少改原代码，先保留在患者结构体中
    */
    char reg_id[30];

    /*
        账户余额/押金相关金额
        为尽量少改原代码，先保留
    */
    Money account_balance;
} Patient;

/* =======================
   药品信息结构体
   ======================= */
typedef struct {
    int med_id;                   // 药品ID，唯一
    char official_name[100];      // 通用名/主名称
    char trade_name[100];         // 商品名
    char alias_name[100];         // 别名
    Money unit_price;             // 单价，单位：分
    int stock;                    // 当前库存数量
    char dept_related[50];        // 关联科室
} Medicine;

/* =======================
   诊疗记录结构体
   ======================= */
typedef struct {
    char record_id[30];           // 记录唯一编号

    int p_id;                     // 患者ID，对应 patient_table
    int d_id;                     // 医生ID，对应 doctor_table

    /*
        记录类型：
        1 - 挂号
        2 - 看诊
        3 - 检查
        4 - 开药
        5 - 住院
    */
    int type;

    char diagnosis[100];          // 看诊时的诊断结果
    Money check_fee;              // 检查费用，单位：分
    int med_id;                   // 药品ID
    int med_count;                // 药品数量
    int ward_id;                  // 病房ID
    int bed_id;                   // 床位ID

    /* 记录开始时间 */
    int start_month;
    int start_day;
    int start_hour;
    int start_min;

    /* 记录结束时间 */
    int end_month;
    int end_day;
    int end_hour;
    int end_min;

    /*
        撤销标记：
        0 - 正常
        1 - 已撤销
    */
    int is_revoked;

    /*
        删除标记：
        0 - 正常
        1 - 已删除（软删除）
    */
    int is_deleted;

    Money total_cost;             // 当前记录总费用，单位：分
} MedicalRecord;

/* =======================
   病房床位结构体
   ======================= */
typedef struct {
    int ward_id;                  // 病房ID
    int bed_id;                   // 床位ID

    /*
        病房类型：
        1 - 普通病房
        2 - 重症病房
        3 - VIP病房
    */
    int ward_type;

    /*
        床位状态：
        0 - 空闲
        1 - 占用
        2 - 维护中
    */
    int status;

    int p_id;                     // 当前床位对应的患者ID
    char dept_name[50];           // 关联科室名称
} WardBed;

/* =======================
   科室信息结构体
   ======================= */
typedef struct {
    int dept_id;                  // 科室ID
    char dept_name[50];           // 科室名称
} Department;

/* =======================
   全局数据表声明
   ======================= */

   /* 医生总表 */
extern Doctor doctor_table[MAX_DOCTOR];

/* 患者总表 */
extern Patient patient_table[MAX_PATIENT];

/* 诊疗记录总表 */
extern MedicalRecord record_table[MAX_RECORDS];

/* 药品总表 */
extern Medicine medicine_table[MAX_MED_TYPES];

/* 病房床位总表 */
extern WardBed bed_table[MAX_WARD * MAX_BED_PER_WARD];

/* 科室总表 */
extern Department department_table[MAX_DEPARTMENT];

/* =======================
   全局计数器声明
   ======================= */

   /* 当前医生数量 */
extern int doctor_count;

/* 当前患者数量 */
extern int patient_count;

/* 当前记录数量 */
extern int record_count;

/* 当前药品数量 */
extern int medicine_count;

/* 当前床位数量 */
extern int bed_count;

/* 当前科室数量 */
extern int department_count;

/* 医院当前营业额，单位：分 */
extern Money hospital_total_revenue;

/* =======================
   公共函数声明
   ======================= */

   /* 安全读取整数 */
int GetSafeInt();

/* 安全读取字符串 */
void GetSafeString(char* buffer, int max_len);

/* 智能联想演示接口 */
void SmartSuggestDemo();

#endif