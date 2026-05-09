#pragma once
#ifndef MODULE_D_H
#define MODULE_D_H

#include "global.h"

#define FILE_DOCTOR      "doctor.txt"
#define FILE_PATIENT     "patient.txt"
#define FILE_RECORD      "record.txt"
#define FILE_MEDICINE    "medicine.txt"
#define FILE_MONEY       "money.txt"
#define FILE_WARD_BED    "ward_bed.txt"
#define FILE_INPATIENT   "inpatient.txt"
#define FILE_SYSTEM      "system_state.txt"
#define FILE_REVENUE     "revenue.txt"
#define FILE_README      "README.txt"

#define LOG_OP_FILE      "log_operation.txt"
#define LOG_BAD_FILE     "log_bad_data.txt"
#define LOG_DEL_FILE     "log_delete.txt"

void WriteLog(const char* log_file, const char* message);
void LogBadLine(const char* filename, int line_no, const char* raw_line);
void PrintRecord(const RecordNode* r);
int  ConfirmPatientByName(const char* name);
int  ConfirmDoctorByName(const char* name);

#endif
