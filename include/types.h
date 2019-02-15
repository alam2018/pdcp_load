/*! 
    \file types.h
    Common type definitions.
 */

//******************************************************************************
//
// Copyright (c) 2013 Alcatel-Lucent.  All Rights Reserved
//
// THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Alcatel-Lucent.
// The copyright notice above does not evidence any
// actual or intended publication of such source code.
// update : 15 Aug 13 inserted UPID by Sunil/Horst
// update : 20 Aug 13 inserted COREID by Sunil/Horst
// update : PSC: 3. Nov 15 inserted GNU types uint32_t ...    
// update : PSC: 18. May 16 inserted ANSI_COLORs for logging
//******************************************************************************

/*!*****************************************************************************
*
*  \file      types.h
*
*  \brief     Common types.
*
*  \author    Puneet Chawla;
*  \version   0.2
*  \date      2015/10/13
*
*  bug        None
*
*  todo       None
*
*  \copyright Copyright (c) 2013 Alcatel-Lucent.  All Rights Reserved
*
*******************************************************************************/

#ifndef TYPES_H
#define TYPES_H

#include "stdint.h"


//******************************************************************************
// Name Constants
//******************************************************************************

typedef enum
{
    FAILURE = 0,
    SUCCESS,
    MAX_STATUS
} BOOL_STATUS;

typedef enum
{
    FALSE = 0,
    TRUE,
} BOOL;

#define SYSCALLFAIL -1

#define SYSCALLFAIL -1
#define MAX_STATUS_NAME_LEN 255


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// enum for communicating and storing the split decision cf td500, 
// Do not start with 0, as this is default in debug mode only!
typedef enum
{
    UNSET = 10,
    Edge_Cloud_Bypass = 11,
    Edge_Cloud_Bypass_with_CA_User_Plane = 12,
    Edge_Cloud_Bypass_with_MC_4G_5G = 13,
    Edge_Cloud_Bypass_with_MC_MultiRAT = 14,
    Local_Data = 15,
    Reject = 16,
    Split_A_for_add_on_Link_and_Split_C_for_Macro_links_with_MC_4G_5G_ = 17,
    Split_A_for_add_on_Link_and_Split_C_for_Macro_links_with_MC_MultiRAT = 18,
    Split_A_for_the_add_on_Link_and_Split_C_for_the_Macro_Links_with_MC_4G_5G =
 19,
    Split_A_with_MC_4G_5G = 20,
    Split_A_with_MC_4G_5G_RAN_RTT_gt_10ms = 21,
    Split_A_with_MC_MultiRAT = 22,
    Split_A_with_MC_MultiRAT_with_Macro_Links_in_Edge_Cloud = 23,
    Split_A_with_MC_MultiRAT_RAN_RTT_gt_10ms = 24,
    Split_A_RAN_RTT_gt_10ms = 25,
    Split_B = 26,
    Split_B_with_CA_User_Plane = 27,
    Split_B_with_CA_User_Plane_and_interconnection_of_partial_PHY_schedulers = 28,
    Split_B_with_interconnection_of_partial_PHY_schedulers = 29,
    Split_C = 30,
    Split_C_with_CA_and_CoMP_User_Plane = 31,
    Split_C_with_CA_User_Plane = 32,
    Split_C_with_CoMP_User_Plane = 33,
    Split_C_with_MC_4G_5G = 34,
    Split_C_with_MC_MultiRAT = 35,
    MAXIMUM_SPLITS = 36,
} SPLIT;


//******************************************************************************
// Common Types
//******************************************************************************

// Basic Types
// PSC changed according to
// http://www.gnu.org/software/libc/manual/html_node/Integers.html
// 
typedef signed char         SINT8;
typedef char                INT8;
typedef short               INT16;
typedef int                 INT32;
typedef long long           INT64;
typedef long                LONG;


typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef unsigned long       ULONG;

typedef void                VOID;
typedef intptr_t            INTPTR;
typedef uintptr_t           UINTPTR;

typedef int         (*FUNCPTR)     (); /*! ptr to function returning int */
typedef void        (*VOIDFUNCPTR) (); /*! ptr to function returning void */
typedef double      (*DBLFUNCPTR)  (); /*! ptr to function returning double */
typedef float       (*FLTFUNCPTR)  (); /*! ptr to function returning float */

typedef float       FLOAT;             /*! for POSIX safety */

// Complex User Defined Types

typedef struct
{
    UINT16      drbId;      /*! identifies the DRB ID of interest */
    UINT16      ueIdx;      /*! identifies the UE of interest */
} LCID;


typedef UINT32              BBUID; // BBUID is the primary key, so finding the 
                                   // BBUIP from the BBUID is easy, but not the
                                   // other way round. The convention is that 
                                   // the first LRM_UP_BBU_REGISTER_REQ gets 
                                   // the BBUID=0, and the second one the 1, .. 
typedef UINT32              BBUIP; // BBUIP is a single number (inet_addr()) 
                                   // of the 4 bytes IP # in dotted format.
typedef UINT32		        UPID;  // lrm-up interface , needs to identify
                                   // which ups' are running in which core in 
                                   // the BBU
typedef UINT32		        COREID;// Indentifies on which core the up is
                                   // running

#endif // TYPES_H
