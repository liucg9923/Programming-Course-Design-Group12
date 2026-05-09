#pragma once
#ifndef WARD_H
#define WARD_H

#include "global.h"

void ShowWardStatus(void);
int  AssignBedToPatient(int patientId, const char* preferredDept, int preferredWardType);
void ReleaseBed(int bedId);
void ChangeBedStatus(void);
void GetBedStatistics(int* totalFree, int* totalOccupied, int* totalMaintenance,
    int* normalFree, int* icuFree, int* vipFree);
void InitDefaultWardsAndBeds(void);

#endif
