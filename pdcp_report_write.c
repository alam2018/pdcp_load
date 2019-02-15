/*
 * pdcp_report_write.c
 *
 *  Created on: Sep 26, 2018
 *      Author: user
 */

#include "string.h"
#include "unistd.h"
#include "unistd.h"
#include "strings.h"
#include "errno.h"


#include <stdint.h>
#include "stdlib.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include "socket_msg.h"
#include "pdcp_support.h"

#define MIPS_PER_CPU 	9485.8662

double pdcpTime_per_process, pdcpTime_per_packet[MAX_NO_CONN_TO_PDCP];

int rec_cycle;
long long int noPkt, procBytes;

typedef struct reportBuffer {
	double rohc_time;
	double pdu_time;
	double enc_time;
}_treportBuffer;

_treportBuffer pdcp_report [MAX_NO_CONN_TO_PDCP];

#define SEC_TO_NANO_SECONDS  1000000000


double mips_from_us (double time_in_us)
{
	double time_in_s = time_in_us / SEC_TO_NANO_SECONDS;
	double total_mips = time_in_s * MIPS_PER_CPU;

	return total_mips;
}


FILE *pdcp_report_write;
FILE *conect_status;
FILE *mips_summ;
void report () {
//	pdcp_report_write= (FILE **) malloc(96 * sizeof(FILE*));
	pdcp_report_write = fopen ("feeder_report.csv","w+");
	if (pdcp_report_write == NULL)
	{
		printf ("File not created okay, errno = %d\n", errno);
	}
	int i;
#ifdef detailed_timing_report
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		fprintf (pdcp_report_write,"PDCP instance; ROHC time(us); PDCP SDU creation(us); decryption time(us); "
			"Per packet Process Time (us);");
	}
	fprintf (pdcp_report_write, "Total PDCP Process time (us);\n");
#endif

#ifdef mips_calc_report
	fprintf (pdcp_report_write,"Total number of rec pkt; Total bytes processed by PDCP; ");
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		fprintf (pdcp_report_write,"PDCP instance; Per packet Process Capacity (mips);");
	}
	fprintf (pdcp_report_write, "Total needed PDCP Process Capacity (mips); Reference time (s); "
			"MIPS per second\n");
#endif

#ifdef mips_sum_report
	mips_summ = fopen ("mips_summ_report.csv","w+");
	setbuf(mips_summ, NULL);
	if (mips_summ == NULL)
	{
		printf ("File not created okay, errno = %d\n", errno);
	}
	fprintf (mips_summ, "Reference Time; Total receive cycle; Number of packet; Total processed bytes; "
			"Total required MIPS per second;\n");
#endif
}

void init_report()
{
	int i;
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		pdcp_report[i].enc_time = 0;
		pdcp_report[i].pdu_time = 0;
		pdcp_report[i].rohc_time = 0;
		pdcpTime_per_packet[i] = 0;
	}
	pdcpTime_per_process = 0;
}


void update_rohc_db (double rohc_time, int dbIndex)
{
	pdcp_report[dbIndex].rohc_time = rohc_time;
}

void update_pdu_db (double pdu_time, int dbIndex)
{
	pdcp_report[dbIndex].pdu_time = pdu_time;
}

void update_enc_db (double enc_time, int dbIndex)
{
	pdcp_report[dbIndex].enc_time = enc_time;
}

void pdcp_time_per_packet (double pdcp_time, int dbIndex)
{
	pdcpTime_per_packet[dbIndex] = pdcp_time;
}

void total_pdcp_time (double total_pdcp_time)
{
	pdcpTime_per_process = total_pdcp_time;
}


void create_pdcp_report ()
{
	int i;
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
/*		fprintf (pdcp_report_write,"PDCP instance; %d; ROHC time(us); %f; PDCP SDU creation(us); %f; decryption time(us); "
				"%f; Per packet Process Time (us); %f;", i, pdcp_report[i].rohc_time/1000,
				pdcp_report[i].pdu_time/1000, pdcp_report[i].enc_time/1000, pdcpTime_per_packet[i]/1000);*/
		fprintf (pdcp_report_write,"%d; %f; %f; %f; %f;", i, pdcp_report[i].rohc_time/1000,
				pdcp_report[i].pdu_time/1000, pdcp_report[i].enc_time/1000, pdcpTime_per_packet[i]/1000);
	}
	fprintf (pdcp_report_write,"%f\n", pdcpTime_per_process/1000);

	init_report();
}

double total_mips_pro_sec = 0;
struct timespec tempVal;
bool reset_tempVal = true;
void mips_report (int totalPktNo, long long int totalPktSize)
{
	rec_cycle++;
	noPkt += (long long int) totalPktNo;
	procBytes += totalPktSize;
	double per_pkt_mips;
	int i;
#ifdef mips_calc_report
	fprintf (pdcp_report_write,"%d; %lld; ", totalPktNo, totalPktSize);
	for (i = 0; i<MAX_NO_CONN_TO_PDCP; i++)
	{
		per_pkt_mips = mips_from_us (pdcpTime_per_packet[i]);
		fprintf (pdcp_report_write, "%d; %f; ", i, per_pkt_mips);
	}
#endif
	double total_mips = mips_from_us(pdcpTime_per_process);

	if (reset_tempVal == true)
	{
		reset_tempVal = false;
		tempVal.tv_sec = 0;
	}

	if (tempVal.tv_sec == 0)
	{
		tempVal.tv_sec = refernce_time.tv_sec;
		total_mips_pro_sec += total_mips;
	} else if (tempVal.tv_sec == refernce_time.tv_sec)
	{
		total_mips_pro_sec += total_mips;
	} else if (tempVal.tv_sec < refernce_time.tv_sec)
	{

#ifdef mips_sum_report
	fprintf (mips_summ,"%ld; %d; %lld; %lld; %f;\n", refernce_time.tv_sec, rec_cycle -1,
			noPkt - (long long int) totalPktNo, procBytes - totalPktSize, total_mips_pro_sec);
#endif
		rec_cycle = 1;
		noPkt = (long long int) totalPktNo;
		procBytes = totalPktSize;
		tempVal.tv_sec = refernce_time.tv_sec;
		total_mips_pro_sec = 0;
		total_mips_pro_sec += total_mips;

	} else
		perror("ERROR report write for MIPS per second");


#ifdef mips_calc_report
	fprintf (pdcp_report_write,"%f; %ld; %f;\n", total_mips, refernce_time.tv_sec, total_mips_pro_sec);
#endif

	init_report();
}

int totalConnection = 0;
void update_connect_status (int fd)
{
	totalConnection++;

	conect_status = fopen ("connection_status.txt","a");
	if (conect_status == NULL)
	{
		printf ("File not created okay, errno = %d\n", errno);
	}

	fprintf (conect_status,"New connection on FD %d; Total number of feeder connection %d\n", fd, totalConnection);
	fclose(conect_status);
}

void process_start_time_record (struct timespec start_time)
{
	refernce_time.tv_sec = start_time.tv_sec;
	refernce_time.tv_nsec = start_time.tv_nsec;
}
