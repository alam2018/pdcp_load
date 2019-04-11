/*
 * cldMng_msg.c
 *
 *  Created on: Dec 7, 2018
 *      Author: user
 */


#define _GNU_SOURCE
#include <stdint.h>
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "sys/socket.h"
#include "sys/ioctl.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "net/if.h"
#include "unistd.h"
#include "arpa/inet.h"
#include <netdb.h>
#include <sys/select.h>
#include <fcntl.h>//non blocking function
#include <sys/time.h>
#include "errno.h"
#include <time.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <sched.h>
#include "signal.h"

#include "types.h"
#include "math.h"
#include "list.h"
#include "pdcp.h"
#include "log.h"
#include "pdcp_support.h"

#include "socket_msg.h"


//Function deceleration
void cldMsgHandler(UINT32 messageId, INT32 sockFd);
void resource_req_init (INT32 sockFd);
void resource_req_update (INT32 sockFd, int cpu_req, double  down_BW_req, double  up_BW_req, int querry_type);


extern EXT_MSG_T ExtRecMsg;
extern UINT32	sockExtHeaderSize;// = sizeof(ExtRecMsg.msgId) + sizeof (ExtRecMsg.msgSize);// implicitly calculating header size
extern fd_set fdgroup;
extern double downlink_mips;
extern double uplink_mips;
extern double downlink_bw, uplink_bw;
extern int meas_count_downlink, meas_count_uplink;
extern int cpu_avail;
extern double down_bw_avail, up_bw_avail;

bool cld_reg_success;

void cld_reg (INT32 sockFd)
{
	CLOUD_MANAGER_REGISTRATION_T mng_reg;

	mng_reg.module_id = MODULE_ID;
	mng_reg.reg_resp = -45;
	cld_reg_success = false;

//	MsgInsertFunc (CLOUD_MANAGER_REGISTRATION, sizeof (CLOUD_MANAGER_REGISTRATION_T), &mng_reg, &temppdcpSendBuffer);
	execute_MsgInsertFunc (CLOUD_MANAGER_REGISTRATION, sizeof (CLOUD_MANAGER_REGISTRATION_T), &mng_reg);
//	MsgSend (sockFd);
	execute_MsgSend(sockFd);
}


VOID cld_MsgReceive(INT32 connectedSockFd)
{
	INT32 retValue = -1;
	temppdcpReceiveBuffer = pdcpReceiveBuffer;
	memset(pdcpReceiveBuffer,0,BUFFER_SIZE);
	retValue = recv(connectedSockFd,temppdcpReceiveBuffer,sockExtHeaderSize,0); //LRM receives message on templrmReceiverBufer


	if (retValue == SYSCALLFAIL )
	{
		perror("recv");
		printf("recv() returned Error \n");
		retValue = 0;
		return;
	} 	else if (retValue == 0)
	{
		printf("Connection is closed on FD (%u)\n",(unsigned)connectedSockFd);
		close (connectedSockFd);
		FD_CLR(connectedSockFd, &fdgroup);
	} else
	{
		 // if the Message has some data in the payload
		ExtRecMsg.msgId = ((EXT_MSG_T*)temppdcpReceiveBuffer)->msgId;
		ExtRecMsg.msgSize = ((EXT_MSG_T*)temppdcpReceiveBuffer)->msgSize;
		temppdcpReceiveBuffer += sockExtHeaderSize;

		 if (ExtRecMsg.msgSize)
		 {
			 retValue = recv(connectedSockFd,temppdcpReceiveBuffer, ExtRecMsg.msgSize, 0);
			 if (retValue == SYSCALLFAIL )
			 {
				 perror("recv2");
				 printf("recv() returned Error \n");
			 }
		 }

//		 // call message handler here
		 cldMsgHandler(ExtRecMsg.msgId, connectedSockFd);
	}
}



void cldMsgHandler(UINT32 messageId, INT32 sockFd)
{


	switch ( messageId )
	{
	case CLOUD_MANAGER_REGISTRATION:
	{
		CLOUD_MANAGER_REGISTRATION_T mng_reg;
		memcpy(&mng_reg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
		if (mng_reg.reg_resp == 1)
		{
			printf ("Registration to cloud manager is successful\n");
			printf ("Asking for resource to the cloud manager\n");
			resource_req_init (sockFd);
			cld_reg_success = true;
		}
	}

	break;

	case CLOUD_MANAGER_RESOURCE_HANDLE:
	{
		CLOUD_MANAGER_RESOURCE_HANDLE_T res_req;
		memcpy(&res_req,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

		if (res_req.res_querry_enable == 2)
		{
//			res_req.res_querry_enable = 3;
			res_req.resource_req = (int) (downlink_mips / meas_count_downlink + uplink_mips / meas_count_uplink);
			res_req.down_BW_req = (double) (downlink_bw / meas_count_downlink);
			res_req.up_BW_req = (double) (uplink_bw / meas_count_uplink);
			resource_req_update (sockFd, res_req.resource_req, res_req.down_BW_req, res_req.up_BW_req, res_req.res_querry_enable);
			downlink_mips = 0;
			downlink_bw = 0;
			meas_count_downlink = 0;
			uplink_mips = 0;
			uplink_bw = 0;
			meas_count_uplink = 0;
		} else if (res_req.res_querry_enable == 3)
		{
			cpu_avail = res_req.resource_rsp;
			down_bw_avail = res_req.down_BW_rsp;
			up_bw_avail = res_req.up_BW_rsp;
		} else
			printf ("Wrong allocation msg from cloud manager");
	}
	break;

	}
}

void resource_req_init (INT32 sockFd)
{
	CLOUD_MANAGER_RESOURCE_HANDLE_T res_req;

	res_req.module_id = MODULE_ID;
	res_req.resource_req = 1000;
	res_req.resource_rsp = -45;
	res_req.res_querry_enable = 1;

	execute_MsgInsertFunc (CLOUD_MANAGER_RESOURCE_HANDLE, sizeof (CLOUD_MANAGER_RESOURCE_HANDLE_T), &res_req);
	execute_MsgSend(sockFd);
}

void resource_req_update (INT32 sockFd, int cpu_req, double down_BW_req, double up_BW_req, int querry_type)
{
	CLOUD_MANAGER_RESOURCE_HANDLE_T res_req;

	res_req.module_id = MODULE_ID;
	res_req.resource_req = cpu_req;
	res_req.resource_rsp = -45;
	res_req.down_BW_req = down_BW_req;
	res_req.up_BW_req = up_BW_req;
	res_req.res_querry_enable = querry_type;

	execute_MsgInsertFunc (CLOUD_MANAGER_RESOURCE_HANDLE, sizeof (CLOUD_MANAGER_RESOURCE_HANDLE_T), &res_req);
	execute_MsgSend(sockFd);
}
