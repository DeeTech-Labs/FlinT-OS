#include "touch.h"

bool touchBegin() {
    return false;
}
bool touchReady() {
    return false;
}
void touchMaintain() {}
void touchInterrupt() {}
uint64_t touchNextWake() {
    return UINT64_MAX;
}
