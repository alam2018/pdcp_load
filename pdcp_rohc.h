/*
 * pdcp_rohc.h
 *
 *  Created on: Feb 2, 2018
 *      Author: idefix
 */

#ifndef PDCP_ROHC_H_
#define PDCP_ROHC_H_

#include "socket_msg.h"

int rohc_main();
struct rohc_comp *compressor[MAX_NO_CONN_TO_PDCP];  /* the ROHC compressor */
struct rohc_comp * create_compressor();

int rohc_decompmain();
struct rohc_decomp *decompressor[MAX_NO_CONN_TO_PDCP];  /* the ROHC compressor */
struct rohc_decomp * create_decompressor();

#endif /* PDCP_ROHC_H_ */
