#include "targetAgent.h"

TargetAgent::TargetAgent(int id, int row, int col, bool isCritical)
    : id(id), row(row), col(col), alive(true), isCritical(isCritical) {}
