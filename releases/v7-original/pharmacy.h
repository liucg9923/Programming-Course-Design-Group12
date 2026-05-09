#pragma once
#ifndef PHARMACY_H
#define PHARMACY_H

#include "global.h"

void PharmacyManagement(void);
MedicineNode* FindMedicineByIdNode(int medicineId);
int  DeductMedicineStock(int medicineId, int quantity);
void AddMedicineStock(int medicineId, int quantity);
int  CountStockWarnings(void);

#endif
