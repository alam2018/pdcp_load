/*
 * collec_dec_pdcp.h
 *
 *  Created on: Oct 4, 2017
 *      Author: idefix
 */

#ifndef INCLUDE_PDCP_V10_1_0_COLLEC_DEC_PDCP_H_
#define INCLUDE_PDCP_V10_1_0_COLLEC_DEC_PDCP_H_


//OPENAIR2/LAYER2/MAC/defs.h
/*!\brief DTCH DRB1  logical channel */
#define DTCH 3 // LCID



//OPENAIR2/LAYER2/RLC/rlc.h
#define  RLC_MUI_UNDEFINED     (mui_t)0
typedef enum rlc_confirm_e {
  RLC_SDU_CONFIRM_NO    = 0,
  RLC_SDU_CONFIRM_YES   = 1,
} rlc_confirm_t;


///openair1/PHY/TOOLS/time_meas.h
typedef struct {

  long long in;
  long long diff;
  long long diff_now;
  long long p_time; /*!< \brief absolute process duration */
  long long diff_square; /*!< \brief process duration square */
  long long max;
  int trials;
} time_stats_t;



#endif /* INCLUDE_PDCP_V10_1_0_COLLEC_DEC_PDCP_H_ */
