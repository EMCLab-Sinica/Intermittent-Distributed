#define STRINGIFY(x) #x
#define TASKNAME_SENSING STRINGIFY(sensing)
#define TASKNAME_SPRAYER STRINGIFY(sprayer)
#define TASKNAME_MONITOR STRINGIFY(monitor)
#define TASKNAME_REPORT STRINGIFY(report)

void setupTasks();
void sensingTask();
void sprayerTask();
void monitorTask();
void reportTask();
