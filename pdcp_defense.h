/*
 * pdcp_defense.h
 *
 *  Created on: Feb 11, 2019
 *      Author: user
 */

#ifndef PDCP_DEFENSE_H_
#define PDCP_DEFENSE_H_

#define APPROXIMATE_REMAINING_TIME 50

#define INDEX	22
#define MAX_QCI_PRO 90  //Obtained from 5qi table

const int qci_5g_value [INDEX] = {1, 2, 3, 4, 65, 66, 67, 75, 5, 6, 7, 8, 9, 69, 70, 79, 80, 81, 82, 83, 84, 85};
const int qci_delay [INDEX] = {100, 150, 50, 300, 75, 100, 100, 50, 100, 300, 100, 300, 60, 200, 50, 10, 5, 10, 20, 10, 10};
const int qci_prio [INDEX] = {20, 40, 30, 50, 7, 20, 15, 25, 10, 60, 70, 80, 90, 5, 55, 65, 68, 11, 12, 13, 19, 22};


typedef struct qci_database {
	int qci_value;
	int qci_delay;
	int qci_prio;
}_tqci_property;


//Forward function deceleration
int defense ();


#endif /* PDCP_DEFENSE_H_ */
