/****************************************************************
* FILENAME:     COMM_handlers.h
* DESCRIPTION:  Communication handlers
* AUTHOR:       Mitja Nemec
*
****************************************************************/

#ifndef     __INCLUDE_COMM_HANDLERS_H__
#define     __INCLUDE_COMM_HANDLERS_H__

#include    "F28x_Project.h"
#include    "define.h"
#include    "globals.h"

#include    "DLOG_gen.h"
#include    "REF_gen.h"

#include    "LRTME_stack.h"

#include    "PCB_util.h"

#define     COMM_BAUDRATE       (1000000L)

extern void COMM_initialization(void);

extern void COMM_runtime(void);

#endif /* INCLUDE_COMM_HANDLERS_H_ */
