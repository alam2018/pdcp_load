/*
 * socket_handle.c
 *
 *  Created on: Oct 5, 2017
 *      Author: idefix
 */

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

#include "pdcp.h"
#include "pdcp_primitives.h"
#include "platform_types.h"
#include "types.h"
#include "socket_msg.h"
#include "pdcp_support.h"
#include "pdcp_rohc.h"



//ROHC_COMPRESSION related
//extern struct rohc_comp *compressor;  /* the ROHC compressor */
//extern static struct rohc_comp * create_compressor(void);



extern mem_block_t       *pdcp_pdu_p;
extern uint16_t           pdcp_pdu_size;


/*
 * Function forward deceleration
 */

static VOID MsgHandler(UINT32 messageId, INT32 sockFd);




//******************************************************************************
// Message Instances
//******************************************************************************

PDCP_RUN_FUNC_T 				pdcpRunFuncMsg;
PDCP_DATA_REQ_FUNC_T 			pdcpDataReqFuncMsg;
PDCP_DATA_IND_T 				pdcpDataIndMsg;
PDCP_REMOVE_UE_T 				pdcpRemUEMsg;
RRC_PDCP_CONFIG_ASN1_REQ_T		rrcPdcpConfigAsn1Msg;
RRC_PDCP_CONFIG_REQ_T			rrcPdcpConfigReqMsg;
RLC_STATUS_FOR_PDCP_T			rlcStatusPdcpMsg;
PDCP_REG_REQ_T					pdcpRegReqMsg;
RLC_DATA_REQ_T					rlcDataReqMsg;
MAC_ENB_GET_RRC_STATUS_RSP_T	macENBStatusRsp;
PDCP_CONFIG_SET_SECURITY_REQ_T	pdcpConfigSetSecurityReqMsg;


_tSchedBuffer activeRequests[MAX_NO_CONN_TO_PDCP];
int responseBufferSize;
extern fd_set readFds;
extern struct timeval timeout;
bool result_get;
rlc_op_status_t return_result_RLC;
bool mac_status_get;
extern fd_set fdgroup;

//extern conn_info connInfo[MAX_NO_CONN_TO_PDCP];
int connIndex = 0;
extern int db_index, buffer_index;

#ifdef BUFFER_READ_DELAY_REPORT
FILE* buffer_delay;
void buffer_read_delay_report ()
{
	buffer_delay = fopen("buffer_read_delay_report1.csv", "w+");
}
#endif

/*FILE ** write_report;// = (FILE **) malloc(96 * sizeof(FILE*));
void prepare_file_for_report ()
{
	write_report= (FILE **) malloc(96 * sizeof(FILE*));
	int i;
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
	    char buf[1024];

	    // now we build the file name
	    strcpy(buf, REPORT_NAME);
	    sprintf(buf+29,"_%03d", i+1);
	    strcat(buf, ".csv");

	    // open file
	    write_report[i] = fopen(buf, "w+");
	}

}*/

void report_init ()
{
	rohc_time_downlink = 0;
	sequencing_time_downlink = 0;
	encrypt_time = 0;

	rohc_start.tv_sec = 0;
	rohc_start.tv_nsec = 0;

	rohc_end.tv_sec = 0;
	rohc_end.tv_nsec = 0;

	seq_downlink_start.tv_sec = 0;
	seq_downlink_start.tv_nsec = 0;

	seq_downlink_end.tv_sec = 0;
	seq_downlink_end.tv_nsec = 0;

	encrypt_start.tv_sec = 0;
	encrypt_start.tv_nsec = 0;

	encrypt_end.tv_sec = 0;
	encrypt_end.tv_nsec = 0;


	rohc_time_uplink = 0;
	sequencing_time_uplink = 0;
	decrypt_time = 0;

	rohc_uplink_start.tv_sec = 0;
	rohc_uplink_start.tv_nsec = 0;

	rohc_uplink_end.tv_sec = 0;
	rohc_uplink_end.tv_nsec = 0;

	seq_uplink_start.tv_sec = 0;
	seq_uplink_start.tv_nsec = 0;

	seq_uplink_end.tv_sec = 0;
	seq_uplink_end.tv_nsec = 0;

	decrypt_start.tv_sec = 0;
	decrypt_start.tv_nsec = 0;

	decrypt_end.tv_sec = 0;
	decrypt_end.tv_nsec = 0;
}


static VOID MsgSend(int sendFD)
{
	INT32  retValue = 0;

	int sizeOfMsg = responseBufferSize;
	retValue = send(sendFD,pdcpSendBuffer,sizeOfMsg,0);
	printf("Sent msg on Fd (%d) \n\n",sendFD );


	if (SYSCALLFAIL == retValue)
	{
	  perror("send failed");
	}

	// reset the sendBuffer flag
	temppdcpSendBuffer = pdcpSendBuffer;
	// Set to zero.
	responseBufferSize = 0;
	memset(pdcpSendBuffer,0,BUFFER_SIZE);
	return ;

}

/*!----------------------------------------------------------------------------
*Construction of buffer for sending message
*
------------------------------------------------------------------------------*/
 static VOID MsgInsertFunc (
                              UINT32 MsgId ,
                              UINT32 MsgSize,
                              VOID* MsgData,
                              UINT8 **buffer
		             )
{
    EXT_MSG_T lrmExtMsg;

    lrmExtMsg.msgId = MsgId;
    lrmExtMsg.msgSize = MsgSize; //Indicates the size of the message payload
    // Set/re-set the external structure
    memset(lrmExtMsg.dataPacket,0, BUFFER_SIZE);//SRK

    memcpy(lrmExtMsg.dataPacket,MsgData,lrmExtMsg.msgSize);
    memset(*buffer,0,BUFFER_SIZE);

    memcpy(*buffer,&lrmExtMsg.msgId, sizeof(lrmExtMsg.msgId));
    *buffer += sizeof(lrmExtMsg.msgId);

    memcpy(*buffer,&lrmExtMsg.msgSize, sizeof(lrmExtMsg.msgSize));
    *buffer += sizeof(lrmExtMsg.msgSize);

    memcpy(*buffer, lrmExtMsg.dataPacket, lrmExtMsg.msgSize);
    *buffer += lrmExtMsg.msgSize;

    responseBufferSize = (sizeof(lrmExtMsg.msgId))+ (sizeof(lrmExtMsg.msgSize))+ (lrmExtMsg.msgSize);

    return ;
}




//Message received from UP. Both Control message and scheduler message is handled here
bool sckClose = false;
EXT_MSG_T ExtRecMsg;
UINT32	sockExtHeaderSize = sizeof(ExtRecMsg.msgId) + sizeof (ExtRecMsg.msgSize);// implicitly calculating header size

int bufferCount;

int  delCon(int nSockFd, int Addr)
{
	if (nSockFd<0)
		return -1; // Bad parameter
	activeRequests[nSockFd].sockFD = 0;
//	activeRequests[nSockFd].entity = NULL;
//	activeRequests[nSockFd].usage = false;
//	free (activeRequests[nSockFd].pData);
	return 0;
}


int  AddCon(int nSockFd, int Addr)
{
	if (nSockFd<=0)
		return -1; // Bad parameter
//	isNewCon = true;
	int i,n;
	for (i=0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		if(activeRequests[i].sockFD ==0)//entry is free
		{
			total_connection ++;
			for (n=0; n<MAX_BUFFER_REC_WINDOW; n++)
			{
				activeRequests[i].sockBufferDatabase[n].pData = (UINT8*)malloc(BUFFER_SIZE);
						if (NULL == activeRequests[i].sockBufferDatabase[n].pData)
						{
							printf(("Failed to allocate memory for 'pdcpReceiveBuffer',QUIT\n"));
							printf("PDCP module start failed!!! \n");
							exit(EXIT_FAILURE);
						}
			}

			activeRequests[i].sockFD = nSockFd;
			activeRequests[i].dbIndex = i;
			strcpy(activeRequests[i].entity, "OAI");
			activeRequests[i].sizeUsd =0;
			printf ("Connection established with OAI at socket %d\n\n", nSockFd);

#ifdef ROHC_COMPRESSION
			/* create a ROHC compressor with small CIDs and the largest MAX_CID
			 * possible for small CIDs */
			printf("\ncreate the ROHC compressor %d\n", i);
			compressor[i] = create_compressor();
			if(compressor == NULL)
			{
				fprintf(stderr, "failed create the ROHC compressor\n");
			}

			//! [create ROHC decompressor #1]
				/* Create a ROHC decompressor to operate:
				 *  - with large CIDs,
				 *  - with the maximum of 5 streams (MAX_CID = 4),
				 *  - in Unidirectional mode (U-mode).
				 */
			//! [create ROHC decompressor #1]
				printf("\ncreate the ROHC decompressor %d\n", i);
			//! [create ROHC decompressor #2]
				decompressor[i] = create_decompressor();
				if(decompressor[i] == NULL)
				{
					fprintf(stderr, "failed create the ROHC decompressor\n");
				}
#endif
			return i;
			}
		}
return -2;
}

int findCon(int nSockFd)
{
	int sockfound=0;
	if (nSockFd<=0)
		return -1; // Bad parameter

	int i;
	for (i=0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		if(activeRequests[i].sockFD ==nSockFd)//entry is there
		{
			sockfound =activeRequests[i].IPaddr;
			activeRequests[i].sockBufferDatabase[0].pData -= sockExtHeaderSize;
			return i;//if an empty buffer exists for this fd use it
		}

	}
//else make a new one;
return AddCon (nSockFd, sockfound);
}

VOID MsgReceive(INT32 connectedSockFd, int bufferIndex)

{
	INT32 retValue = -1;
	sckClose = false;
	// memorise the start address of the send buffer
	int schedID = findCon(connectedSockFd);
	connIndex = schedID;
	bufferCount = bufferIndex;

	if (bufferCount > MAX_BUFFER_REC_WINDOW)
	{
		printf ("Exceeded buffer receiving window");
		exit (0);
	}

	if (activeRequests[schedID].sockBufferDatabase[bufferCount].isBufferUsed == false)
		{
//			temppdcpReceiveBuffer = activeRequests[schedID].sockBufferDatabase[bufferCount].pData;
			memset(activeRequests[schedID].sockBufferDatabase[bufferCount].pData,0,BUFFER_SIZE);
		} else
		{
			printf ("Serious inconsistency with the connection database buffer. Please check\n");
			exit (0);
		}

//	retValue = recv(connectedSockFd,temppdcpReceiveBuffer,sockExtHeaderSize,0); //LRM receives message on templrmReceiverBufer
	retValue = recv(connectedSockFd,activeRequests[schedID].sockBufferDatabase[bufferCount].pData,sockExtHeaderSize,0); //LRM receives message on templrmReceiverBufer

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
		delCon(schedID, 0);
	} else
	{
		 // if the Message has some data in the payload
/*		ExtRecMsg.msgId = ((EXT_MSG_T*)temppdcpReceiveBuffer)->msgId;
		ExtRecMsg.msgSize = ((EXT_MSG_T*)temppdcpReceiveBuffer)->msgSize;
		temppdcpReceiveBuffer += sockExtHeaderSize;*/

		ExtRecMsg.msgId = ((EXT_MSG_T*)activeRequests[schedID].sockBufferDatabase[bufferCount].pData)->msgId;
		ExtRecMsg.msgSize = ((EXT_MSG_T*)activeRequests[schedID].sockBufferDatabase[bufferCount].pData)->msgSize;
		activeRequests[schedID].sockBufferDatabase[bufferCount].pData += sockExtHeaderSize;

		activeRequests[schedID].sockBufferDatabase[bufferCount].msgSize = ExtRecMsg.msgSize;
		activeRequests[schedID].sockBufferDatabase[bufferCount].isBufferUsed = true;
		activeRequests[schedID].msgID = ExtRecMsg.msgId;

		 if (ExtRecMsg.msgSize)
		 {
			 retValue = recv(connectedSockFd,activeRequests[schedID].sockBufferDatabase[bufferCount].pData, ExtRecMsg.msgSize, 0);
//			 retValue = recv(connectedSockFd,temppdcpReceiveBuffer, ExtRecMsg.msgSize, 0);
			 if (retValue == SYSCALLFAIL )
			 {
				 perror("recv2");
				 printf("recv() returned Error \n");
			 }
		 }

//		 // call message handler here
//		 MsgHandler(ExtRecMsg.msgId, connectedSockFd);
	}
}


static VOID MsgHandler(UINT32 messageId, INT32 sockFd)
//static VOID MsgHandler(UINT32 messageId)
{
	switch ( messageId )
	{
		case PDCP_RUN_FUNC:
		{
			/*! \fn void pdcp_run(const protocol_ctxt_t* const  ctxt_pP)
			* \brief Runs PDCP entity to let it handle incoming/outgoing SDUs
			* \param[in]  ctxt_pP           Running context.
			*/
			memcpy(&pdcpRunFuncMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
			protocol_ctxt_t   ctxt;
			PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, pdcpRunFuncMsg.module_id, pdcpRunFuncMsg.enb_flag, pdcpRunFuncMsg.rnti, pdcpRunFuncMsg.frame, pdcpRunFuncMsg.subframe,pdcpRunFuncMsg.eNB_index);
			pdcp_run(&ctxt);
		}

		break;

		case PDCP_DATA_REQ_FUNC:
		{
			/*! \fn boolean_t pdcp_data_req(const protocol_ctxt_t* const  , srb_flag_t , rb_id_t , mui_t , confirm_t ,sdu_size_t , unsigned char* , pdcp_transmission_mode_t )
			* \brief This functions handles data transfer requests coming either from RRC or from IP
			* \param[in] ctxt_pP        Running context.
			* \param[in] rab_id         Radio Bearer ID
			* \param[in] muiP
			* \param[in] confirmP
			* \param[in] sdu_buffer_size Size of incoming SDU in bytes
			* \param[in] sdu_buffer      Buffer carrying SDU
			* \param[in] mode            flag to indicate whether the userplane data belong to the control plane or data plane or transparent
			* \return TRUE on success, FALSE otherwise
			* \note None
			* @ingroup _pdcp
			*/

//			memcpy(&pdcpDataReqFuncMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

			if (db_index !=-1 && buffer_index !=-1 &&
					activeRequests[db_index].sockBufferDatabase[buffer_index].isBufferUsed == true)
			{
				memcpy(&pdcpDataReqFuncMsg,activeRequests[db_index].sockBufferDatabase[buffer_index].pData,
						activeRequests[db_index].sockBufferDatabase[buffer_index].msgSize);
			} else
			{
				printf ("Either the database or buffer index is wrong. Exiting the program");
				exit (0);
			}


/*			if (connInfo[connIndex].socID == -99)
			{
				connInfo[connIndex].socID = sockFd;
				connInfo[connIndex].bearerID = pdcpDataReqFuncMsg.rb_id;
				connInfo[connIndex].pdcpInsID = connIndex;
				connInfo[connIndex].dataDirection = pdcpDataReqFuncMsg.ctxt_pP.enb_flag;  //If 0 then dataflow uplink direction elseIF 1 downlink direction
			}*/

/*			if (pdcpDataReqFuncMsg.rohc_packet.len > 60 || pdcpDataReqFuncMsg.sdu_buffer_size > 2000)
			{
				printf ("Check");
			}*/


			total_processed_bytes += pdcpDataReqFuncMsg.sdu_buffer_size;

#ifdef BUFFER_READ_DELAY_REPORT
			clock_gettime(CLOCK_MONOTONIC, &packet_rec);
			double packet_delay = (double)(packet_rec.tv_sec - pdcpDataReqFuncMsg.packet_send.tv_sec)*SEC_TO_NANO_SECONDS +
					(double)(packet_rec.tv_nsec - pdcpDataReqFuncMsg.packet_send.tv_nsec);

			fprintf (buffer_delay,"Delay for receiving packet; %f\n", packet_delay/1000);
#endif

#ifdef ROHC_COMPRESSION
				boolean_t result;
				uint8_t  sdu_buffer[SDU_BUFFER_SIZE];
				memcpy (sdu_buffer, &pdcpDataReqFuncMsg.rohc_packet.dataBuffer, pdcpDataReqFuncMsg.sdu_buffer_size);
//				memcpy (sdu_buffer, &pdcpDataReqFuncMsg.rohc_packet.dataBuffer, SDU_BUFFER_SIZE);
			int tempPacketLength = pdcpDataReqFuncMsg.rohc_packet.len;
			rohc_main(compressor[db_index]);

			int ROHC_recover = pdcpDataReqFuncMsg.rohc_packet.len - tempPacketLength;

			pdcpDataReqFuncMsg.sdu_buffer_size = pdcpDataReqFuncMsg.sdu_buffer_size + ROHC_recover;

#ifdef create_report
//			clock_gettime(CLOCK_MONOTONIC, &rohc_end);
			rohc_time_downlink = (double)(rohc_end.tv_sec - rohc_start.tv_sec)*SEC_TO_NANO_SECONDS + (double)(rohc_end.tv_nsec - rohc_start.tv_nsec);
			update_rohc_db (rohc_time_downlink, db_index);
#endif

			result = pdcp_data_req (&pdcpDataReqFuncMsg.ctxt_pP, pdcpDataReqFuncMsg.srb_flagP,
					pdcpDataReqFuncMsg.rb_id, pdcpDataReqFuncMsg.muiP, pdcpDataReqFuncMsg.confirmP,
					pdcpDataReqFuncMsg.sdu_buffer_size, sdu_buffer, pdcpDataReqFuncMsg.mode);
			memcpy (&pdcpDataReqFuncMsg.rohc_packet.dataBuffer, sdu_buffer, pdcp_pdu_size);
#else
			boolean_t result = pdcp_data_req (&pdcpDataReqFuncMsg.ctxt_pP, pdcpDataReqFuncMsg.srb_flagP,
					pdcpDataReqFuncMsg.rb_id, pdcpDataReqFuncMsg.muiP, pdcpDataReqFuncMsg.confirmP,
					pdcpDataReqFuncMsg.sdu_buffer_size, pdcpDataReqFuncMsg.buffer, pdcpDataReqFuncMsg.mode);
#endif

#ifdef create_report
/*			if (connIndex <= MAX_NO_CONN_TO_PDCP)
			{
				fprintf (write_report[connIndex],"PDCP instance; %d; ROHC time; %f; Downlink PDCP PDU creation and sequencing time; %f; Downlink encryption time; %f\n",
						connIndex, rohc_time_downlink/1000, sequencing_time_downlink/1000, encrypt_time/1000);
			} else
				printf ("Something wrong with measurement report creation\n");*/

			report_init ();

#endif

			pdcpDataReqFuncMsg.pdcp_result = result;
			pdcpDataReqFuncMsg.pdcp_pdu_size = pdcp_pdu_size;
			pdcpDataReqFuncMsg.sdu_buffer_size = pdcp_pdu_size;
//			pdcpDataReqFuncMsg.pdcp_pdu.pool_id = *(pdcp_pdu_p->pool_id);
//			memcpy(pdcpDataReqFuncMsg.pdcp_pdu.pool_id,pdcp_pdu_p->pool_id, sizeof (unsigned char));
//			pdcpDataReqFuncMsg.pdcp_pdu.data = *pdcp_pdu_p->data;
//			memcpy(pdcpDataReqFuncMsg.pdcp_pdu.data,pdcp_pdu_p->data, pdcpDataReqFuncMsg.pdcp_pdu_size);
//			memcpy(pdcpDataReqFuncMsg.pdcp_pdu,pdcp_pdu_p, pdcpDataReqFuncMsg.pdcp_pdu_size);


//			int mem_block_alam_size = 2 * sizeof (mem_block_alam) + sizeof (unsigned char ) +
//					(SDU_BUFFER_SIZE + MAX_PDCP_HEADER_TRAILER_SIZE) * sizeof (unsigned char );
/*			int mem_block_alam_size = sizeof (unsigned char ) +	(SDU_BUFFER_SIZE + MAX_PDCP_HEADER_SIZE + MAX_PDCP_TAILER_SIZE) * sizeof (unsigned char );
			int msgSize = sizeof (protocol_ctxt_t) + sizeof (srb_flag_t) + sizeof (rb_id_t) + sizeof (mui_t) +
					sizeof (confirm_t) + sizeof (sdu_size_t) + pdcpDataReqFuncMsg.sdu_buffer_size +
					sizeof (pdcp_transmission_mode_t) + sizeof (uint16_t) + mem_block_alam_size + sizeof (boolean_t);

			int tstSize = sizeof (PDCP_DATA_REQ_FUNC_T);*/

//			responseBufferSize = sizeof (UINT32) + sizeof (UINT32) + msgSize;
//			responseBufferSize = sizeof (UINT32) + sizeof (UINT32) + tstSize;
			MsgInsertFunc (PDCP_DATA_REQ_FUNC, sizeof (PDCP_DATA_REQ_FUNC_T), &pdcpDataReqFuncMsg, &temppdcpSendBuffer);
/*			int i;
			for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
			{
				if (activeRequests[i].sockFD > 0)
				{
					MsgSend (activeRequests[i].sockFD);
				}
			}*/

			MsgSend (sockFd);

/*			if (pdcpDataReqFuncMsg.rohc_packet.len > 60 || pdcpDataReqFuncMsg.sdu_buffer_size > 2000)
			{
				printf ("Check");
			}*/
		}

		break;

		case PDCP_DATA_IND:
		{
			/*! \fn boolean_t pdcp_data_ind(const protocol_ctxt_t* const, srb_flag_t, MBMS_flag_t, rb_id_t, sdu_size_t, mem_block_t*, boolean_t)
			* \brief This functions handles data transfer indications coming from RLC
			* \param[in] ctxt_pP        Running context.
			* \param[in] Shows if rb is SRB
			* \param[in] Tells if MBMS traffic
			* \param[in] rab_id Radio Bearer ID
			* \param[in] sdu_buffer_size Size of incoming SDU in bytes
			* \param[in] sdu_buffer Buffer carrying SDU
			* \param[in] is_data_plane flag to indicate whether the userplane data belong to the control plane or data plane
			* \return TRUE on success, FALSE otherwise
			* \note None
			* @ingroup _pdcp
			*/

/*
			memcpy(&pdcpDataIndMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
			pdcp_data_ind ( &pdcpDataIndMsg.ctxt_pP, pdcpDataIndMsg.srb_flagP, pdcpDataIndMsg.MBMS_flagP, pdcpDataIndMsg.rb_id, pdcpDataIndMsg.sdu_buffer_size, &pdcpDataIndMsg.sdu_buffer);
*/

//			memcpy(&pdcpDataIndMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

			if (db_index !=-1 && buffer_index !=-1)
			{
				memcpy(&pdcpDataIndMsg,activeRequests[db_index].sockBufferDatabase[buffer_index].pData,
						activeRequests[db_index].sockBufferDatabase[buffer_index].msgSize);
			} else
			{
				printf ("Either the database or buffer index is wrong. Exiting the program");
				exit (0);
			}

/*			if (connInfo[connIndex].socID == -99)
			{
				connInfo[connIndex].socID = sockFd;
				connInfo[connIndex].bearerID = pdcpDataIndMsg.rb_id;
				connInfo[connIndex].pdcpInsID = connIndex;
				connInfo[connIndex].dataDirection = pdcpDataIndMsg.ctxt_pP.enb_flag;  //If 0 then dataflow uplink direction elseIF 1 downlink direction
			}*/

			//Be careful. If PDCP is running on both downlink and uplink mode, disable this calculation from one side
			total_processed_bytes += pdcpDataIndMsg.sdu_buffer_size;

#ifdef ROHC_COMPRESSION							//Here this def is used for ROHC decompression
				boolean_t result;
				uint8_t  sdu_buffer[SDU_BUFFER_SIZE];
//				memcpy (sdu_buffer, &pdcpDataIndMsg.rohc_packet.dataBuffer, pdcpDataIndMsg.sdu_buffer_size);
			int tempPacketLength = pdcpDataIndMsg.rohc_packet.len;
			rohc_decompmain(decompressor[db_index]);

			int ROHC_recover = pdcpDataIndMsg.rohc_packet.len - tempPacketLength;

			pdcpDataIndMsg.sdu_buffer_size = pdcpDataIndMsg.sdu_buffer_size + ROHC_recover;

/*			result = pdcp_data_ind_uplink (db_index, &pdcpDataIndMsg.ctxt_pP, pdcpDataIndMsg.srb_flagP,
					pdcpDataIndMsg.MBMS_flagP, pdcpDataIndMsg.rb_id, pdcpDataIndMsg.sdu_buffer_size, sdu_buffer);*/

			result = pdcp_data_ind_uplink (db_index, &pdcpDataIndMsg.ctxt_pP, pdcpDataIndMsg.srb_flagP,
					pdcpDataIndMsg.MBMS_flagP, pdcpDataIndMsg.rb_id, pdcpDataIndMsg.sdu_buffer_size, pdcpDataIndMsg.rohc_packet.dataBuffer);
#else
			boolean_t result = pdcp_data_ind_uplink (db_index, &pdcpDataIndMsg.ctxt_pP, pdcpDataIndMsg.srb_flagP,
					pdcpDataIndMsg.MBMS_flagP, pdcpDataIndMsg.rb_id, pdcpDataIndMsg.sdu_buffer_size, pdcpDataIndMsg.buffer);
#endif

#ifdef create_uplink_report
			if (connIndex <= TOTAL_PDCP_INSTANCE)
			{
				fprintf (write_report[connIndex],"PDCP instance; %d; Uplink ROHC time(us); %f; Uplink PDCP SDU creation(us); %f; Uplink decryption time(us); %f\n",
						connIndex, rohc_time_uplink/1000, sequencing_time_uplink/1000, decrypt_time/1000);
			} else
				printf ("Something wrong with measurement report creation\n");

			report_init ();

#endif

			memset (&pdcpDataIndMsg, 0, sizeof (PDCP_DATA_IND_T));

		}

		break;

		case PDCP_REMOVE_UE:
		{
			/* \brief  Function for RRC to configure a Radio Bearer clear all PDCP resources for a particular UE
			* \param[in]  ctxt_pP           Running context.
			* \return     A status about the processing, OK or error code.
			*/

			memcpy(&pdcpRemUEMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
			pdcp_remove_UE (&pdcpRemUEMsg.ctxt_pP);
		}

		break;

		case RRC_PDCP_CONFIG_ASN1_REQ:
		{
			/*! \fn bool rrc_pdcp_config_asn1_req (const protocol_ctxt_t* const , SRB_ToAddModList_t* srb2add_list, DRB_ToAddModList_t* drb2add_list, DRB_ToReleaseList_t*  drb2release_list)
			* \brief  Function for RRC to configure a Radio Bearer.
			* \param[in]  ctxt_pP           Running context.
			* \param[in]  index             index of UE or eNB depending on the eNB_flag
			* \param[in]  srb2add_list      SRB configuration list to be created.
			* \param[in]  drb2add_list      DRB configuration list to be created.
			* \param[in]  drb2release_list  DRB configuration list to be released.
			* \param[in]  security_mode     Security algorithm to apply for integrity/ciphering
			* \param[in]  kRRCenc           RRC encryption key
			* \param[in]  kRRCint           RRC integrity key
			* \param[in]  kUPenc            User-Plane encryption key
			* \return     A status about the processing, OK or error code.
			*/

			memcpy(&rrcPdcpConfigAsn1Msg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

			rrc_pdcp_config_asn1_req(&rrcPdcpConfigAsn1Msg.ctxt_pP, &rrcPdcpConfigAsn1Msg.srb2add_list_pP,
					&rrcPdcpConfigAsn1Msg.drb2add_list_pP, &rrcPdcpConfigAsn1Msg.drb2release_list_pP,
					rrcPdcpConfigAsn1Msg.security_modeP, &rrcPdcpConfigAsn1Msg.kRRCenc_pP, &rrcPdcpConfigAsn1Msg.kRRCint_pP,
					&rrcPdcpConfigAsn1Msg.kUPenc_pP
#ifdef Rel10
  , &rrcPdcpConfigAsn1Msg.pmch_InfoList_r9_pP;
#endif
			);
		}

		break;


		case RRC_PDCP_CONFIG_REQ:
		{
			/*! \fn void rrc_pdcp_config_req(const protocol_ctxt_t* const ,uint32_t,rb_id_t,uint8_t)
			* \brief This functions initializes relevant PDCP entity
			* \param[in] ctxt_pP        Running context.
			* \param[in] actionP flag for action: add, remove , modify
			* \param[in] rb_idP Radio Bearer ID of relevant PDCP entity
			* \param[in] security_modeP Radio Bearer ID of relevant PDCP entity
			* \return none
			* \note None
			* @ingroup _pdcp
			*/

			memcpy(&rrcPdcpConfigReqMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
			rrc_pdcp_config_req (&rrcPdcpConfigReqMsg.ctxt_pP, rrcPdcpConfigReqMsg.srb_flagP, rrcPdcpConfigReqMsg.actionP, rrcPdcpConfigReqMsg.rb_idP, rrcPdcpConfigReqMsg.security_modeP);

		}

		break;

		case PDCP_MODULE_CLEANUP:
		{
			 pdcp_module_cleanup ();
		}

		break;

		case PDCP_MODULE_INIT_REQ:
		{
			pdcp_module_init ();
		}

		break;

		case RLC_STATUS_FOR_PDCP:
		{
			result_get = true;
			memcpy(&rlcStatusPdcpMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
			return_result_RLC = rlcStatusPdcpMsg.statVal;
		}

		break;

		case PDCP_REG_REQ:
		{
			memcpy(&pdcpRegReqMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);
			int i;
			for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
			{
				if (sockFd == activeRequests[i].sockFD)
				{
					strncpy(activeRequests[i].entity, pdcpRegReqMsg.entity, sizeof(activeRequests[i].entity));
					break;
				}
			}

		}

		break;

		case MAC_ENB_GET_RRC_STATUS_RSP:
		{
			memcpy(&macENBStatusRsp,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

			mac_status_get = true;

		}

		break;

		case PDCP_CONFIG_SET_SECURITY_REQ:
		{
			memcpy(&pdcpConfigSetSecurityReqMsg,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

			pdcp_config_set_security (&pdcpConfigSetSecurityReqMsg.ctxt_pP, &pdcpConfigSetSecurityReqMsg.pdcp_pP,
					pdcpConfigSetSecurityReqMsg.rb_idP, pdcpConfigSetSecurityReqMsg.lc_idP, pdcpConfigSetSecurityReqMsg.security_modeP,
					&pdcpConfigSetSecurityReqMsg.kRRCenc, &pdcpConfigSetSecurityReqMsg.kRRCint, &pdcpConfigSetSecurityReqMsg.kUPenc);

		}

		break;

		case PDCP_HASH_COLLEC:
		{
			PDCP_HASH_COLLEC_T pdcpHashCollecMsg;
			memcpy(&pdcpHashCollecMsg.pdcp_coll_p,pdcp_coll_p, sizeof(hash_table_t*));

			MsgInsertFunc (PDCP_HASH_COLLEC, sizeof(PDCP_HASH_COLLEC_T), &pdcpHashCollecMsg, &temppdcpSendBuffer);
			int i;
			for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
			{
				if (activeRequests[i].sockFD > 0)
				{
					MsgSend (activeRequests[i].sockFD);
					break;
				}
			}
		}

		break;

		case PDCP_GET_SEQ_NUMBER_TEST:
		{
			PDCP_GET_SEQ_NUMBER_TEST_T pdcpGetSeqNo;
			memcpy(&pdcpGetSeqNo,temppdcpReceiveBuffer, ExtRecMsg.msgSize);

			pdcpGetSeqNo.return_result = pdcp_get_sequence_number_of_pdu_with_long_sn(&pdcpGetSeqNo.pdu_buffer);

			MsgInsertFunc (PDCP_GET_SEQ_NUMBER_TEST, ExtRecMsg.msgSize, &pdcpGetSeqNo, &temppdcpSendBuffer);

			int i;
			for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
			{
				if (activeRequests[i].sockFD > 0)
				{
					MsgSend (activeRequests[i].sockFD);
					break;
				}
			}

		}

		break;

		 default:
		 {
			 printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
			 printf("Unknown msg received from OAI\n\n\n\n");
			 printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		 }
		 break;
	}
}


rlc_op_status_t get_result(int sockFD)
{
	return_result_RLC = -1;
	int loop = 0;
	result_get = false;
/*				  //TODO, need to inspect. Set a appropriate function call here
 * while(loop)
	{
		if (FD_ISSET(sockFD,&readFds))
		{
			MsgReceive (sockFD);

			if (result_get == true && return_result_RLC > -1)
			{
				loop = 1;
				result_get = false;
				return return_result_RLC;
			}
		}
	}*/

	return -1;
}


rlc_op_status_t rlc_status_resut(const protocol_ctxt_t* const ctxt_pP,
        const srb_flag_t   srb_flagP,
        const MBMS_flag_t  MBMS_flagP,
        const rb_id_t      rb_idP,
        const mui_t        muiP,
        confirm_t    confirmP,
        sdu_size_t   sdu_sizeP,
        mem_block_t *sdu_pP)
{
	memcpy (&rlcDataReqMsg.ctxt_pP, ctxt_pP, sizeof(protocol_ctxt_t));
//	rlcDataReqMsg.ctxt_pP = *ctxt_pP;
	rlcDataReqMsg.srb_flagP = srb_flagP;
	rlcDataReqMsg.MBMS_flagP = MBMS_flagP;
	rlcDataReqMsg.rb_idP = rb_idP;
	rlcDataReqMsg.muiP = muiP;
	rlcDataReqMsg.confirmP = confirmP;
	rlcDataReqMsg.sdu_sizeP = sdu_sizeP;
	memcpy (&rlcDataReqMsg.sdu_pP, sdu_pP, sizeof(mem_block_t));

	int i;
	char *module_name1 = "RLC";
	char *module_name2 = "OAI";
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
//		if (strcmp(module_name1, activeRequests[i].entity) || strcmp(module_name2, activeRequests[i].entity))
		{
			  MsgInsertFunc(RLC_DATA_REQ, sizeof(RLC_DATA_REQ_T), &rlcDataReqMsg, &temppdcpSendBuffer);
			  MsgSend (activeRequests[i].sockFD);
/*			  printf("Waiting for status reply from RLC........\n\n");
			  //TODO, need to inspect. Set a appropriate function call here
			  rlc_op_status_t rlc_status= get_result(activeRequests[i].sockFD);
			  printf("Got the status reply from RLC. Continuing the program\n\n");
			  return rlc_status;*/
		}
	}
	return 0;
}


void pdcp_rrc_data_ind_send (  const protocol_ctxt_t* const ctxt_pP,
		  const rb_id_t                srb_idP,
		  const sdu_size_t             sdu_sizeP,
		  uint8_t              * const buffer_pP)
{
	PDCP_RRC_DATA_IND_RSP_T pdcpRRCDataIndRspMsg;

	memcpy (&pdcpRRCDataIndRspMsg.ctxt_pP, ctxt_pP, sizeof (protocol_ctxt_t));
	pdcpRRCDataIndRspMsg.srb_idP = srb_idP;
	pdcpRRCDataIndRspMsg.sdu_sizeP = sdu_sizeP;
	memcpy (&pdcpRRCDataIndRspMsg.buffer_pP, buffer_pP, sdu_sizeP);

	MsgInsertFunc (PDCP_RRC_DATA_IND_RSP, sizeof(PDCP_RRC_DATA_IND_RSP_T), &pdcpRRCDataIndRspMsg, &temppdcpSendBuffer);
	int i;
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		if (activeRequests[i].sockFD > 0)
		{
			MsgSend (activeRequests[i].sockFD);
			break;
		}
	}

}

int get_mac_status(int sockFD)
{
	int loop = 0;
	mac_status_get = false;
/*				  //TODO, need to inspect. Set a appropriate function call here
 * while(loop)
	{
		if (FD_ISSET(sockFD,&readFds))
		{
			MsgReceive (sockFD);

			if (mac_status_get == true)
			{
				loop = 1;
				mac_status_get = false;
				return macENBStatusRsp.mac_status;
			}
		}
	}*/

	return -1;
}


int mac_eNB_get_rrc_status_send(const module_id_t   module_idP, const rnti_t  rntiP)
{
	MAC_ENB_GET_RRC_STATUS_REQ_T macEnbGetRRCStatusMsg;

	macEnbGetRRCStatusMsg.module_idP = module_idP;
	macEnbGetRRCStatusMsg.rntiP = rntiP;

	MsgInsertFunc (MAC_ENB_GET_RRC_STATUS_REQ, sizeof(MAC_ENB_GET_RRC_STATUS_REQ_T), &macEnbGetRRCStatusMsg, &temppdcpSendBuffer);
	int i;
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		if (activeRequests[i].sockFD > 0)
		{
			MsgSend (activeRequests[i].sockFD);
			return get_mac_status(activeRequests[i].sockFD);
		}
	}

	return 0;
}

void execute_msgHandler (UINT32 messageId, INT32 sockFd)
{
	MsgHandler(messageId, sockFd);
}

void execute_MsgInsertFunc (UINT32 MsgId, UINT32 MsgSize, VOID* MsgData)
{
	temppdcpSendBuffer = pdcpSendBuffer;
    memset(pdcpSendBuffer,0, BUFFER_SIZE);
	MsgInsertFunc (MsgId, MsgSize, MsgData, &temppdcpSendBuffer);
}

void execute_MsgSend(int sendFD)
{
	MsgSend(sendFD);
}
