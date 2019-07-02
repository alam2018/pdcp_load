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
FILE *pdcp_freq_rep;
FILE *def_rep;
FILE *usr_prio;
FILE *defense_time;
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
	//This generates report about whole PDCP process time in term of MIPS
/*	fprintf (mips_summ, "Reference Time; Total receive cycle; Number of packet; Total processed bytes; "
			"Total required MIPS per second;\n");*/

	//This generates report about only PDCP pkt process time in term of millisecond
	fprintf (mips_summ, "Reference Time; Total receive cycle; Number of packet; Total processed bytes; "
				"Total required Time for pkt process (ms);\n");
#endif

#ifdef freq_report
	pdcp_freq_rep = fopen ("pdcp_freq_rep.csv","w+");
	setbuf(pdcp_freq_rep, NULL);
	if (pdcp_freq_rep == NULL)
	{
		printf ("File not created okay, errno = %d\n", errno);
	}
	//This generates report about whole PDCP process time in term of MIPS
/*	fprintf (mips_summ, "Reference Time; Total receive cycle; Number of packet; Total processed bytes; "
			"Total required MIPS per second;\n");*/

	//This generates report about only PDCP pkt process time in term of millisecond
	fprintf (pdcp_freq_rep, "Reference Time; Total receive cycle; Number of packet; Total processed bytes; "
				"Total required Time for pkt process (us);\n");
#endif

#ifdef def_report
	def_rep = fopen ("pdcp_defense_report.csv","w+");
	setbuf(def_rep, NULL);
	if (def_rep == NULL)
	{
		printf ("File not created okay, errno = %d\n", errno);
	}

	fprintf (def_rep, "CPU request ; CPU available ; CPU allocated by defense ; BW request ;"
			"BW available ; Total BW allocated by defense \n");
#endif

#ifdef usr_prio_report
	usr_prio = fopen ("user_priority_report.csv","w+");
	setbuf(usr_prio, NULL);
	if (usr_prio == NULL)
	{
		printf ("File not created okay, errno = %d\n", errno);
	}

	fprintf (usr_prio, "Bearer ID; QCI priority; Utilization priority; Unscheduling priority; Total priority \n");
#endif

#ifdef defense_time_report
	defense_time = fopen ("defense_time_report.csv","w+");
	setbuf(defense_time, NULL);
	if (defense_time == NULL)
	{
	printf ("File not created okay, errno = %d\n", errno);
	}

	fprintf (defense_time, "Number of Bearer processing request; Total CPU Request (DMIPS); Available CPU (DMIPS); "
			"Allocated CPU (DMIPS); Number of Dropped Bearer;  Defense Time (ns)\n");
#endif

}


void init_report_write()
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
	pdcpTime_per_pkt += pdcp_time;
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

	init_report_write();
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
		//This generates report about whole PDCP process time in term of MIPS
/*	fprintf (mips_summ,"%ld; %d; %lld; %lld; %f;\n", refernce_time.tv_sec, rec_cycle -1,
			noPkt - (long long int) totalPktNo, procBytes - totalPktSize, total_mips_pro_sec);*/

		//This generates report about only PDCP pkt process time in term of millisecond
		fprintf (mips_summ,"%ld; %d; %lld; %lld; %f;\n", refernce_time.tv_sec, rec_cycle -1,
					noPkt - (long long int) totalPktNo, procBytes - totalPktSize, pdcpTime_per_pkt/1000);
		pdcpTime_per_pkt = 0;
#endif

#ifdef freq_report
		//This generates report about only PDCP pkt process time in term of millisecond
		fprintf (pdcp_freq_rep,"%ld; %d; %lld; %lld; %f;\n", refernce_time.tv_sec, rec_cycle -1,
					noPkt, procBytes, pdcpTime_per_pkt/1000);
		pdcpTime_per_pkt = 0;
#endif
		rec_cycle = 1;
//		noPkt = (long long int) totalPktNo;
//		procBytes = totalPktSize;
		noPkt = 0;
		procBytes = 0;
		tempVal.tv_sec = refernce_time.tv_sec;
		total_mips_pro_sec = 0;
//		total_mips_pro_sec += total_mips;
		total_mips_pro_sec = 0;

	} else
		perror("ERROR report write for MIPS per second");


#ifdef mips_calc_report
	fprintf (pdcp_report_write,"%f; %ld; %f;\n", total_mips, refernce_time.tv_sec, total_mips_pro_sec);
#endif

	init_report_write();
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

double cpu_req, cpu_alloc, cpu_available, down_bw_req, down_bw_alloc, down_bw_available;

void set_cpu_data (double req, double alloc, double avail)
{
	cpu_req = req;
	cpu_alloc = alloc;
	cpu_available = avail;
}

void set_down_bw_data (double req, double alloc, double avail)
{
	down_bw_req = req;
	down_bw_alloc = alloc;
	down_bw_available = avail;
}

void defense_report_write ()
{
	fprintf (def_rep,"%f ; %f; %f; %f; %f; %f\n", cpu_req, cpu_available, cpu_alloc,
			down_bw_req, down_bw_available, down_bw_alloc);
}

double usr_qci_prio = 0, utilization_prio = 0, unschedule_prio = 0, total_prio = 0;
int bearer_db = 0;

void set_usr_prio_data (int bearer_index, double qci, double utilization, double unschedule, double total)
{
	bearer_db = bearer_index;
	usr_qci_prio = qci;
	utilization_prio = utilization;
	unschedule_prio = unschedule;
	total_prio = total;

	if (usr_qci_prio == 0)
		printf ("check");
}

void user_prio_write ()
{
	if (usr_qci_prio == 0)
		printf ("check");
	fprintf (usr_prio,"%d ; %f; %f; %f; %f\n", bearer_db, usr_qci_prio, utilization_prio, unschedule_prio, total_prio);
}

int drb_req = 0, drb_dropped = 0;

void defense_timming_report_write (double def_timing)
{
	fprintf (defense_time,"%d ; %f; %f; %f; %d; %f\n", drb_req, cpu_req, cpu_available, cpu_alloc, drb_dropped, def_timing);
	drb_req = 0;
	drb_dropped = 0;
}
