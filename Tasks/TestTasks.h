#define STRINGIFY(x) #x
#define TASKNAME_SENSING STRINGIFY(sensing)
#define TASKNAME_FAN STRINGIFY(fan)
#define TASKNAME_SPRAYER STRINGIFY(sprayer)

void localAccessTask();
void syncTimeHelperTask();
void sensingTask();
void fanTask();
