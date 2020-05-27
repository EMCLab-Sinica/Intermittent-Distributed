/*
 * maps.h
 *
 *  Description : This header file is used to define the address maps for atomic commit
 *              ** What we need to protect for atomicity
 *                  * Address of consistency version (map0 and map1)
 *                  * Validity time interval of data (validStart and validEnd)
 *              ** call init() to reset data
 */

#define DB_MAX_OBJ 16
#define NUMCOMMIT 15
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#pragma DATA_SECTION(mapSwitcher, ".map") //each bit indicates address map for a object
static int mapSwitcher[15];//16bit * 15 = 240 maximum objects


/* Protected data for atomicity */
#pragma NOINIT(map0)
static void* map0[DB_MAX_OBJ];
#pragma NOINIT(validBegin0)
static unsigned long validBegin0[DB_MAX_OBJ];
#pragma NOINIT(validEnd0)
static unsigned long validEnd0[DB_MAX_OBJ];

#pragma NOINIT(map1)
static void* map1[DB_MAX_OBJ];
#pragma NOINIT(validBegin1)
static unsigned long validBegin1[DB_MAX_OBJ];
#pragma NOINIT(validEnd1)
static unsigned long validEnd1[DB_MAX_OBJ];

/* internal functions */
static unsigned long max(unsigned long a, unsigned long b){
    if (a > b)
        return a;
    else
        return b;
}

/* map functions */
void init();
void* access(int numObj);
void accessCache(int numObj);
void* accessData(int numObj);
void commit(int numObj, void* commitaddress, unsigned long vBegin, unsigned long vEnd);
void dumpAll();
unsigned long getBegin(int numObj);
unsigned long getEnd(int numObj);





