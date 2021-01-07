/*
 * config.h
 *
 *  Created on: 2018�~2��13��
 *      Author: Meenchen
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define NODEADDR 1
#define SYNCTIME_NODE 5
#define CYCLE_PER_US 16

    //#define ONNVM //read/write on NVM
    //#define ONVM // read/write on VM: need to copy all data once require to read after power resumes
#define OUR //read from NVM/VM, write to VM
    //#define ONEVERSION //all read on NVM/commit to NVM

#define MAX_GLOBAL_TASKS 10    // max task "globally"
#define NUMDATA 40
#define MAX_READERS 5
#define MAX_TASK_READ_OBJ 5

// Application settings
#define  DHT11_NODE 1

#endif /* CONFIG_H_ */
