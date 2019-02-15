/*
 * pdcp_main.c
 *
 *  Created on: Sep 21, 2017
 *      Author: idefix
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

#include "downlink_pdcp_meas.h"
#include "uplink_pdcp_meas.h"

//#include "collec_dec_pdcp.h"
#include "socket_msg.h"

//conn_info connInfo[MAX_NO_CONN_TO_PDCP];

//******************************************************************************
// Global Declarations
//******************************************************************************

INT32 gConnectSockFd;
INT32 gListenSockFd;
INT32 gNewConnectedListenSockFd;
INT32 sendFD;
fd_set fdgroup;
fd_set readFds;
// This defines the measurement time interval in sec and microsecs.
int measurem_intvall_s  = 0;
int measurem_intvall_us = 2;



// Socket and socket function related variables
struct sockaddr_in socketAddrPDCP, socketCldMan;
struct timeval timeout;
//    struct timespec timeout;

INT32 fdmax;
INT32 reuseaddr = TRUE;
INT32 portno_PDCP;


// Message send , receive and handling related variables

INT8 *module;
BOOL loadMeasure = FALSE;
INT32 retValue;
UINT32 sizeOfMsg;
UINT32 responseBufferSize;


//******************************************************************************
// PDCP self test
//******************************************************************************

//#define SELF_TEST_ENABLE 1



//******************************************************************************
// PDCP variables
//******************************************************************************
/*
 * These are the PDCP entities that will be utilised
 * throughout the test
 *
 * For pdcp_data_req() and pdcp_data_ind() these
 * are passed and used
 */
pdcp_t pdcp_array[MAX_NO_CONN_TO_PDCP];
int db_index, buffer_index;

/*
 * TX list is the one we use to receive PDUs created by pdcp_data_req() and
 * RX list is the one we use to pass these PDUs to pdcp_data_ind(). In test_pdcp_data_req()
 * method every PDU created by pdcp_data_req() are first validated and then added to RX
 * list after it's removed from TX list
 */
list_t test_pdu_tx_list;
list_t test_pdu_rx_list;

extern _tSchedBuffer activeRequests[MAX_NO_CONN_TO_PDCP];
void 	init_connection ()
{
	int i,n;
	for (i=0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		for (n=0; n<MAX_BUFFER_REC_WINDOW; n++)
		{
			activeRequests[i].sockBufferDatabase[n].isBufferUsed = false;
		}
	}
}


/*void conn_info_array_init ()
{
	int i = 0;
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		connInfo[i].socID = -99;
		connInfo[i].bearerID = -99;
		connInfo[i].pdcpInsID = -99;
	}
}*/


//Initialize PDCP data structure
BOOL init_pdcp_entity(pdcp_t *pdcp_entity)
{
  if (pdcp_entity == NULL)
    return FALSE;

  /*
   * Initialize sequence number state variables of relevant PDCP entity
   */
  pdcp_entity->next_pdcp_tx_sn = 0;
  pdcp_entity->next_pdcp_rx_sn = 0;
  pdcp_entity->tx_hfn = 0;
  pdcp_entity->rx_hfn = 0;
//  pdcp_entity->kUPenc[0] = 4;
  /* SN of the last PDCP SDU delivered to upper layers */
  pdcp_entity->last_submitted_pdcp_rx_sn = 4095;
  pdcp_entity->seq_num_size = 12;

/*  printf("PDCP entity is initialized: Next TX: %d, Next Rx: %d, TX HFN: %d, RX HFN: %d, " \
      "Last Submitted RX: %d, Sequence Number Size: %d\n", pdcp_entity->next_pdcp_tx_sn, \
      pdcp_entity->next_pdcp_rx_sn, pdcp_entity->tx_hfn, pdcp_entity->rx_hfn, \
      pdcp_entity->last_submitted_pdcp_rx_sn, pdcp_entity->seq_num_size);*/

  return TRUE;
}


//Function to set the sockets to nonBlocking

static VOID sockSetNonBlocking(INT32 sockFD)
{
	INT32 sockNonBlockopts;

	sockNonBlockopts = fcntl(sockFD,F_GETFL);

	if (0 > sockNonBlockopts)
		{
			perror("fcntl(F_GETFL)");
			printf("PDCP module start failed!!! \n");
			exit(EXIT_FAILURE);
		}

	sockNonBlockopts = (sockNonBlockopts | O_NONBLOCK);
	if (0 > fcntl(sockFD,F_SETFL,sockNonBlockopts))
		{
			perror("fcntl(F_SETFL)");
			printf("PDCP module start failed!!! \n");
			exit(EXIT_FAILURE);
		}

	int one = 4;
	unsigned int len = sizeof(one);
	int resOpt = getsockopt(sockFD, SOL_TCP, TCP_NODELAY, (void*)&one, &len);

	one =1;

	resOpt = setsockopt(sockFD, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
	resOpt = getsockopt(sockFD, SOL_TCP, TCP_CORK, (void*)&one, &len);
	resOpt = getsockopt(sockFD, SOL_TCP, TCP_NODELAY, (void*)&one, &len);
	return;
}


//Set specific core to run PDCP
void SetCore(int  Core2Pin)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(Core2Pin, &set);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &set) < 0)
		perror("Error in sched_setaffinity()");
}

//Terminate the program
void sigint_handler(int sig)
{
    char  c;

    signal(sig, SIG_IGN);
    printf("OUCH, did you hit Ctrl-C?\n"
           "Do you really want to quit? [y/n] ");
    c = getchar();
    if (c == 'y' || c == 'Y')
         exit(0);
    else
         signal(SIGINT, sigint_handler);
    getchar(); // Get new line character
}

//Initializing all the parameters for PDCP connection
void set_txt_inp (int countLine, char *val)
{
	INT32 portno_CldMng;

	switch (countLine)
	{
	case 1:
		printf("PDCP connection parameter reading started\n\n");
		break;

	case 2:
		socketCldMan.sin_addr.s_addr = inet_addr(val);
		printf ("PDCP IP: %s", val);
		break;

	case 3:
		portno_CldMng = atoi(val);
		socketCldMan.sin_port = htons(portno_CldMng);
		printf ("PDCP Port: %s", val);
		break;

	 default:
	 {
		 printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		 printf("Unknown msg received from PDCP\n\n\n\n");
		 printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	 }
	 break;
	}
}

double downlink_mips, uplink_mips;
double downlink_bw, uplink_bw;
int main (INT32 argc, INT8 **argv )
{
	bool chkFirstConn = true;
	int firstSockFD = 0;

	int cpu_core = atoi(argv[2]);
	SetCore(cpu_core);

    // Initialize or filling up the socket structure of PDCP
	bzero((char *) &socketCldMan, sizeof(socketCldMan));
	socketCldMan.sin_family = AF_INET;

	FILE* lrmConfFile = 0;
	lrmConfFile = fopen("pdcp_sock_file.conf","r");

	int lineCount = 0;
	bool file_param=false;
	if (lrmConfFile == 0)
	{
		printf("Configuration file not found!!! \n");
		printf("PDCP module connection failed!!! \n\n");
		exit (EXIT_FAILURE);
	} else
	{
		printf("Configuration file found. Now analyzing the inputs \n");
		char line[80];
		while(fgets(line, sizeof(line),lrmConfFile)!=NULL)///something in the file
		{
			if (line[0] == 'S')
			{
				file_param = true;
			}

			if (file_param == true)
			{
				lineCount++;
				set_txt_inp (lineCount, &line[0]);
			}
		}
	}

	// ---------------------------------------------------------------------------
    // Create Socket for Cloud Manager communication
	// ---------------------------------------------------------------------------

    gConnectSockFd = socket(AF_INET, SOCK_STREAM, 0);

    if (gConnectSockFd < 0)
	{
	  perror("ERROR opening PDCP'socket");
	  printf("OAI module start failed!!! \n");
	  exit (EXIT_FAILURE);
	}

	fflush(stdout);

	// Feeder try to connect to PDCP
	int val = connect(gConnectSockFd,(struct sockaddr *) &socketCldMan,sizeof(socketCldMan));
	if ( val == SYSCALLFAIL)
	{
		perror("connect");
		printf("Check if Cloud manager is available for connection \n");
		printf("PDCP module start failed!!! \n");
		exit(EXIT_FAILURE);
	} else
		printf ("Successful connection from PDCP to cloud manager established\n");

	//Allow socket descriptor to be reusable and non blocking
	val = setsockopt(gConnectSockFd, SOL_SOCKET,  SO_REUSEADDR,	(char *)&reuseaddr, sizeof(reuseaddr));

	if (val < 0)
		{
		  perror("setsockopt() failed");
		  close(gConnectSockFd);
		  printf("OAI module start failed!!! \n");
		  exit(EXIT_FAILURE);
		}

	sockSetNonBlocking(gConnectSockFd);
	//--------------------------------------------------------------
	//create socket for accepting incoming communication
	//--------------------------------------------------------------
	//Create an AF_INET stream socket to receive incoming connections on

	gListenSockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (gListenSockFd < 0)
	   {
		 perror("ERROR opening PDCP socket for OAI");
		 printf("PDCP module start failed!!! \n");
		 exit (EXIT_FAILURE);
	   }

	//Allow socket descriptor to be reusable

	int num = setsockopt(gListenSockFd, SOL_SOCKET,  SO_REUSEADDR, (char *)&reuseaddr, sizeof(reuseaddr));

	if (num < 0)
		{
		  perror("setsockopt() failed");
		  close(gListenSockFd);
		  printf("PDCP module start failed!!! \n");
		  exit(EXIT_FAILURE);
		}

	// Set socket to be non-blocking.  All of the sockets for the incoming connections will also be non-blocking since
	// they will inherit that state from the listening socket.

	sockSetNonBlocking(gListenSockFd);

	// Bind the socket
	portno_PDCP = atoi(argv[1]);
	bzero((char *) &socketAddrPDCP, sizeof(socketAddrPDCP));
	socketAddrPDCP.sin_family = AF_INET;
	socketAddrPDCP.sin_addr.s_addr = INADDR_ANY;
	socketAddrPDCP.sin_port = htons(portno_PDCP);

	num = bind(gListenSockFd, (struct sockaddr *) &socketAddrPDCP, sizeof(socketAddrPDCP));
	if (num == SYSCALLFAIL )
	{
		perror("ERROR Binding PDCP'socket");
		close(gListenSockFd);
		printf("PDCP module start failed!!! \n");
		exit (EXIT_FAILURE);
	}

	num = listen(gListenSockFd,MAX_NO_CONN_TO_PDCP);
	if(num == SYSCALLFAIL)
	 {
		 perror("ERROR on listening socket");
		 close(gListenSockFd);
		 printf("PDCP module start failed!!! \n");
		 exit (EXIT_FAILURE);
	 }



	// Allocate memory to the send  buffer
	pdcpSendBuffer =  (UINT8*) (malloc(BUFFER_SIZE));
	if (NULL == pdcpSendBuffer)
		{
			printf("PDCP module start failed!!! \n");
			exit(EXIT_FAILURE);
		}

	// Allocate memory to the receive buffer
	pdcpReceiveBuffer = (UINT8*) (malloc(BUFFER_SIZE));
	if (NULL == pdcpReceiveBuffer)
		{
			printf("PDCP module start failed!!! \n");
			exit(EXIT_FAILURE);
		}

	//Initialize the master fd_set

	FD_ZERO(&fdgroup);
	FD_ZERO(&readFds);

	FD_SET(gListenSockFd,&fdgroup);
	FD_SET(gConnectSockFd,&fdgroup);

	fdmax = gListenSockFd;

	temppdcpSendBuffer = pdcpSendBuffer;

	// Set to zero.
	memset(pdcpSendBuffer,0,BUFFER_SIZE);


	printf("PDCP module started successfully! \n");
	fflush(stdout);  // Otherwise over ssh we do not see the SUCCESS message

	//Initialization
	  /*
	   * Initialize memory allocator, list_t for test PDUs, and log generator
	   */
#if defined create_report || defined create_uplink_report
//prepare_file_for_report ();
#endif

#ifdef BUFFER_READ_DELAY_REPORT
	buffer_read_delay_report ();
#endif

	  init_connection ();
//	  conn_info_array_init ();
	  pool_buffer_init();
	  list_init(&test_pdu_tx_list, NULL);
	  list_init(&test_pdu_rx_list, NULL);
	  logInit();
	  pdcp_layer_init ();

/*	  if (init_pdcp_entity(&pdcp_array[0]) == TRUE && init_pdcp_entity(&pdcp_array[1]) == TRUE)
	    printf(" PDCP entity initialization OK\n");
	  else {
		  printf("[TEST] Cannot initialize PDCP entities!\n");
	    return 1;
	  }*/
	  //Initialization of PDCP instances
	  int index;
	  BOOL init_status;
	  for (index = 0; index < MAX_NO_CONN_TO_PDCP; index++)
	  {
		  init_status =  init_pdcp_entity(&pdcp_array[index]);
//		  init_pdcp_entity(&pdcp_array[index];
		  if (init_status == TRUE)
			  continue;
		  else
		  {
			  printf ("PDCP instance initialization failed. Exiting program");
			  exit (0);
		  }

	  }

	  /* Initialize PDCP state variables */

	  for (index = 0; index < MAX_NO_CONN_TO_PDCP; ++index) {
	    if (pdcp_init_seq_numbers(&pdcp_array[index]) == FALSE) {
	    	printf("[TEST] Cannot initialize sequence numbers of PDCP entity %d!\n", index);
	      exit(1);
	    } else {
	    	printf("[TEST] Sequence number state of PDCP entity %d is initialized\n", index);
	    }
	  }

	  set_pdcp_configuration ();

	  signal(SIGINT, sigint_handler);

#if SELF_TEST_ENABLE
	  pdcp_array[0].rlc_mode = RLC_MODE_AM;
	  pdcp_array[1].rlc_mode = RLC_MODE_AM;
	  main_test();

	  /* Initialize PDCP state variables */
	  for (index = 0; index < 2; ++index) {
	    if (pdcp_init_seq_numbers(&pdcp_array[index]) == FALSE) {
	    	printf("[TEST] Cannot initialize sequence numbers of PDCP entity %d!\n", index);
	      exit(1);
	    } else {
	    	printf("[TEST] Sequence number state of PDCP entity %d is initialized\n", index);
	    }
	  }
#endif

	  report ();
	  struct timespec timePerPacket_start, timePerPacket_end, timePerProc_start, timePerProc_end;
	  double timePerPacket, timePerProc;
	  bool start_report = false;
		cld_reg (gConnectSockFd);
	while (TRUE)
	{
		int measurem_intvall_s  = 0;
		int measurem_intvall_us = 2;
		timeout.tv_sec  = measurem_intvall_s;
		timeout.tv_usec = measurem_intvall_us;
		readFds = fdgroup;
		INT32 n = select(fdmax+1,&readFds,NULL,NULL,&timeout);

		if(n < 0)
		  {
			  perror("select() failed");
			  exit(EXIT_FAILURE);
		  }

		// --------------------------------------------------------------
		// NEW TCP CONNECT REQUEST FROM UP, typically from FD=4
		// --------------------------------------------------------------

		 if (FD_ISSET(gListenSockFd,&readFds))
		 {
			 gNewConnectedListenSockFd = accept(gListenSockFd, NULL,NULL);
				 if(gNewConnectedListenSockFd < 0)
					 {
						 perror("ERROR accept");
						 exit(EXIT_FAILURE);
					 } else
					 {
						 if (chkFirstConn)
						 {
							 firstSockFD = gNewConnectedListenSockFd;
							 chkFirstConn = false;
						 }
					 }

					fdmax = gNewConnectedListenSockFd;
					update_connect_status (gNewConnectedListenSockFd);

					//Set the socket to nonblocking

					sockSetNonBlocking(gNewConnectedListenSockFd);

					FD_SET(gNewConnectedListenSockFd,&fdgroup);
		 }

#ifdef create_report
			clock_gettime(CLOCK_MONOTONIC, &timePerProc_start);
			process_start_time_record (timePerProc_start);
#endif

			/*----------------------------------------
			 * Msg recieve from Cloud Manager
			 */

		if (FD_ISSET(gConnectSockFd,&readFds))
		{
			cld_MsgReceive(gConnectSockFd);
		}
		 INT32 i_fd, noBuffer, noConect = 0;
		for (i_fd = firstSockFD; i_fd <= fdmax; i_fd++)
		{
			for (noBuffer = 0; noBuffer <MAX_BUFFER_REC_WINDOW; noBuffer++)
			{
				if (FD_ISSET(i_fd,&readFds) && i_fd != gConnectSockFd)
					{
						start_report = true;
						MsgReceive(i_fd, noBuffer);

					}
			}

		}
//		PDCP_DATA_REQ_FUNC_T 			*tempPDCPDownMsg;
//		void 			*tempPDCPDownMsg;
//		PDCP_DATA_IND_T 				*tempPDCPUpMsg;
		bool rohc_avail;
		int packet_size = 0;
		downlink_mips = 0;
		uplink_mips = 0;
		downlink_bw = 0;
		uplink_bw = 0;

		  for (noConect = 0; noConect < MAX_NO_CONN_TO_PDCP; noConect++)
		  {
			for (noBuffer = 0; noBuffer <MAX_BUFFER_REC_WINDOW; noBuffer++)
			{
				if (activeRequests[noConect].sockBufferDatabase[noBuffer].isBufferUsed == true)
				{
					if (activeRequests[noConect].msgID == PDCP_DATA_REQ_FUNC)
					{
						//Downlink CPU consumption calculation
#ifdef ROHC_COMPRESSION
						rohc_avail = true;
#else
						rohc_avail = false;
#endif
						packet_size = (int) (((PDCP_DATA_REQ_FUNC_T*)activeRequests[noConect].sockBufferDatabase[noBuffer].pData)->sdu_buffer_size);
						downlink_mips += (int) calc_downlink_mips (packet_size, rohc_avail);
						downlink_bw += calc_downlink_BW (packet_size, rohc_avail);
					} else if (activeRequests[noConect].msgID == PDCP_DATA_IND)
					{
						//Uplink CPU consumption calculation
#ifdef ROHC_COMPRESSION
						rohc_avail = true;
#else
						rohc_avail = false;
#endif
						packet_size = (int) (((PDCP_DATA_IND_T*)activeRequests[noConect].sockBufferDatabase[noBuffer].pData)->sdu_buffer_size);
						uplink_mips += calc_uplink_mips (packet_size, rohc_avail);
						uplink_bw += calc_uplink_bw (packet_size, rohc_avail);
					}
				}
			}
		  }

		int noRecPkt = 0;
		total_processed_bytes = 0;

		  for (noConect = 0; noConect < MAX_NO_CONN_TO_PDCP; noConect++)
		  {
#ifdef create_report
			clock_gettime(CLOCK_MONOTONIC, &timePerPacket_start);

#endif
				for (noBuffer = 0; noBuffer <MAX_BUFFER_REC_WINDOW; noBuffer++)
				{
					if (activeRequests[noConect].sockBufferDatabase[noBuffer].isBufferUsed == true)
					{
						noRecPkt++;
						db_index = activeRequests[noConect].dbIndex;
						buffer_index = noBuffer;
						execute_msgHandler (activeRequests[noConect].msgID, activeRequests[noConect].sockFD);
#ifdef create_report
			clock_gettime(CLOCK_MONOTONIC, &timePerPacket_end);
			timePerPacket = (double)(timePerPacket_end.tv_sec - timePerPacket_start.tv_sec)*SEC_TO_NANO_SECONDS +
					(double)(timePerPacket_end.tv_nsec - timePerPacket_start.tv_nsec);
			pdcp_time_per_packet (timePerPacket, db_index);
#endif

						db_index = -1;
						buffer_index = -1;
					}
				}
		  }
		  init_connection ();

#ifdef create_report
			clock_gettime(CLOCK_MONOTONIC, &timePerProc_end);
			timePerProc = (double)(timePerProc_end.tv_sec - timePerProc_start.tv_sec)*SEC_TO_NANO_SECONDS +
					(double)(timePerProc_end.tv_nsec - timePerProc_start.tv_nsec);
			total_pdcp_time (timePerProc);

			if (start_report == true)
			{
#ifdef detailed_timing_report
				create_pdcp_report ();
#endif

#if defined (mips_calc_report) || defined (mips_sum_report)
				mips_report (noRecPkt, total_processed_bytes);
#endif
				start_report = false;
			}
#endif
	}

}
