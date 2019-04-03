/*
 * pdcp_defense.h
 *
 *  Created on: Feb 11, 2019
 *      Author: user
 */

#ifndef PDCP_DEFENSE_H_
#define PDCP_DEFENSE_H_

#define MAX_CPU_CAPACITY											11383
#define OPERATING_FREQUENCY											2.1

//For these values look at Overview_of_MEF_22.2 and 3GPP TS 23.203 v16.0.0
#define CORE_TO_ENB_DELAY		20
#define MIDHAUL_DELAY			1
#define FRONTHAUL_DELAY			50

#define SEC_TO_MILI  1000
#define NANO_TO_MILI 0.000001
#define APPROXIMATE_REMAINING_TIME 50
#define MAX_BUFFER_PER_BEARER	7500

#define INDEX	22
#define MAX_QCI_PRO 90  //Obtained from 5qi table

const int qci_5g_value [INDEX] = {1, 2, 3, 4, 65, 66, 67, 75, 5, 6, 7, 8, 9, 69, 70, 79, 80, 81, 82, 83, 84, 85};
const int qci_delay [INDEX] = {100, 150, 50, 300, 75, 100, 100, 50, 100, 300, 100, 300, 60, 200, 50, 10, 5, 10, 20, 10, 10};
//The lowest qci_prio[] has the highest priority
const int qci_prio [INDEX] = {20, 40, 30, 50, 7, 20, 15, 25, 10, 60, 70, 80, 90, 5, 55, 65, 68, 11, 12, 13, 19, 22};


typedef struct qci_database {
	int qci_value;
	int qci_delay;
	int qci_prio;
}_tqci_property;

typedef struct pdcp_defense_database {
	int dbIndex;
	double prio;
}_tpdcp_defense_data;

extern int PDCP_MONITOR_WINDOW;
//Forward function deceleration
int defense ();


#endif /* PDCP_DEFENSE_H_ */
