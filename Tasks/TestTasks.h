#define STRINGIFY(x) #x
#define TASKNAME_SENSING STRINGIFY(sensing)
#define TASKNAME_FAN STRINGIFY(fan)
#define TASKNAME_FAN_REPORT STRINGIFY(fan_rep)
#define TASKNAME_SPRAYER STRINGIFY(sprayer)
#define TASKNAME_SPRAYER_REPORT STRINGIFY(spr_rep)
#define TASKNAME_MONITOR STRINGIFY(monitor)

void syncTimeHelperTask();
void sensingTask();
void fanTask();
void sprayerTask();
void monitorTask();
