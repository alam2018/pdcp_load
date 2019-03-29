/*
 * pdcp_defense.c
 *
 *  Created on: Jan 8, 2019
 *      Author: user
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

#include "socket_msg.h"
#include "pdcp_defense.h"


extern _tSchedBuffer activeRequests[MAX_NO_CONN_TO_PDCP];
extern double total_mips_req, total_bw_req;
extern double current_downlink_mips, current_uplink_mips, current_downlink_bw, current_uplink_bw;

//Function forward deceleration
void activate_skipping ();

int cpu_avail, down_bw_avail, up_bw_avail;
double mips_per_usr[MAX_NO_CONN_TO_PDCP], down_bw_per_usr[MAX_NO_CONN_TO_PDCP], up_bw_per_usr[MAX_NO_CONN_TO_PDCP];


//This function calculates the priority of a bearer based on E2E time delay budget according to 5QI table and elapsed time at pdcp
double get_time_prio(int index)
{
	int noBuffer;
	struct timespec curr_time;
	clock_gettime(CLOCK_MONOTONIC, &curr_time);
	double temp_elapse_time = 0, elapse_time = 0;
	for (noBuffer = 0; noBuffer <MAX_BUFFER_REC_WINDOW; noBuffer++)
	{
		if (activeRequests[index].sockBufferDatabase[noBuffer].isBufferUsed == true)
		{
			elapse_time = (double)(curr_time.tv_sec - activeRequests[index].sockBufferDatabase[noBuffer].bufferRecTime.tv_sec)*SEC_TO_MILI +
						(double)(curr_time.tv_nsec - activeRequests[index].sockBufferDatabase[noBuffer].bufferRecTime.tv_nsec) * NANO_TO_MILI;

			if (temp_elapse_time < elapse_time)
			{
				temp_elapse_time = elapse_time;
			}
		}
	}

	int i;
	for (i = 0; i<INDEX; i++)
	{
		if (qci_5g_value[i] == activeRequests[index].qci)
		{

			double time_prio = (temp_elapse_time) / (double) qci_delay[i];
			return time_prio;
		}
	}

	return -1.0;
}

//This function scales the priority of the bearer according to 5QI table priority
double get_qci_prio (int qci)
{
	int i, temp_prio = 0;
	for (i = 0; i<INDEX; i++)
	{
		if (qci_5g_value[i] == qci)
		{
			temp_prio = qci_prio[i];
			double qci_prio = (double) ((MAX_QCI_PRO - temp_prio) / MAX_QCI_PRO);
			return qci_prio;
		}
	}

	return -1.0;
}

double get_unprocessed_bytes_prio (int index)
{
	int noBuffer;
	long int unP_byte = 0;
	for (noBuffer = 0; noBuffer <MAX_BUFFER_REC_WINDOW; noBuffer++)
	{
		if (activeRequests[index].sockBufferDatabase[noBuffer].isBufferUsed)
		{
			unP_byte += (int) (((PDCP_DATA_REQ_FUNC_T*)activeRequests[index].sockBufferDatabase[noBuffer].pData)->sdu_buffer_size);
		}
	}

	double prio_calc = (double) (unP_byte / MAX_BUFFER_PER_BEARER);
	return prio_calc;
}

double get_uti_prio (int index)
{
	double utilization_prio = 0;
	int cpu_capacity = MAX_CPU_CAPACITY;
	utilization_prio = (double) (activeRequests[index].total_bytes_rec / cpu_capacity);

	double dummy_multiConnectivity_multicast_prio = 1;  //TODO when multi-connectivity is added to the traffic generator for the evaluation, we need proper prio calculation here

	return (utilization_prio * dummy_multiConnectivity_multicast_prio);
}


int defense ()
{
	//First we need to prioritize all the bearers
	int noConnect;
	for (noConnect = 0; noConnect < MAX_NO_CONN_TO_PDCP; noConnect++)
	{
		double timing_prio = get_time_prio(noConnect);
		double qci_prio = get_qci_prio (activeRequests[noConnect].qci);
		double unscheduling_prio = get_unprocessed_bytes_prio (noConnect);
		double utilization_prio = get_uti_prio (noConnect);

		activeRequests[noConnect].prio = timing_prio * qci_prio * unscheduling_prio * utilization_prio;
	}


	activate_skipping ();

	return 0;
}

void activate_skipping ()
{
	int pdcp_index [MAX_NO_CONN_TO_PDCP];
	int i, j;
	for (i = 0; i < MAX_NO_CONN_TO_PDCP; i++)
	{
		if (activeRequests[i].isThisInstanceActive == true)
		{
			for (j = 1; j < MAX_NO_CONN_TO_PDCP; j++)
			{
				if (activeRequests[j].isThisInstanceActive == true)
				{
					if (activeRequests[i].prio > activeRequests[j].prio)
					{
						pdcp_index[i] = j;
					}
				}
			}
		}
	}

	for (i = 0; i < MAX_NO_CONN_TO_PDCP; i++)
	{
		if (activeRequests[i].isThisInstanceActive == true)
		{
			if ((total_mips_req > cpu_avail) || (current_downlink_bw > down_bw_avail) || (current_uplink_bw > current_uplink_bw))
			{
				activeRequests[pdcp_index[i]].defense_approve = false;
				total_mips_req = total_mips_req - mips_per_usr[pdcp_index[i]];
	//			total_bw_req = total_bw_req - bw_per_usr[i];
				current_downlink_bw = current_downlink_bw - down_bw_per_usr[pdcp_index[i]];
				current_uplink_bw = current_uplink_bw - down_bw_per_usr[pdcp_index[i]];
			}
			break;
		}
	}
}


