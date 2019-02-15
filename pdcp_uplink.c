/*
 * pdcp_uplink.c
 *
 *  Created on: Aug 31, 2018
 *      Author: user
 */

#define PDCP_C
#ifndef USER_MODE
//#include <rtai_fifos.h>
#endif
#include "assertions.h"
#include "hashtable.h"
#include "pdcp.h"
#include "pdcp_util.h"
#include "pdcp_sequence_manager.h"
//#include "LAYER2/RLC/rlc.h"
//#include "LAYER2/MAC/extern.h"
//#include "RRC/L2_INTERFACE/openair_rrc_L2_interface.h"
#include "pdcp_primitives.h"
//#include "OCG.h"
//#include "OCG_extern.h"
//#include "otg_rx.h"
#include "log.h"
#include <inttypes.h>
#include "platform_constants.h"
#include "vcd_signal_dumper.h"
#include "msc.h"

#include "rlc_alam.h"
#include "mac_defs.h"

#if defined(ENABLE_SECURITY)
# include "osa_defs.h"
#endif

#if defined(ENABLE_ITTI)
# include "intertask_interface.h"
#endif

#if defined(LINK_ENB_PDCP_TO_GTPV1U)
#  include "gtpv1u_eNB_task.h"
#  include "gtpv1u.h"
#endif

#ifndef OAI_EMU
extern int otg_enabled;
#endif

//Alam
#include "socket_msg.h"
#include "defs.h"
#include "vars.h"
#include "list.h"
#include "SRB-ToAddMod.h"
#include "SRB-ToAddModList.h"
#include "DRB-ToAddModList.h"
#include "pdcp_support.h"

//Define Max IP packet size. If Ethernet jumbo frame defined max packet size would be 9000 else 15000
#define ETHERNET_JUMBO_FRAME
//#undef ETHERNET_JUMBO_FRAME

#ifdef ETHERNET_JUMBO_FRAME
#define MAX_IP_PACKET_SIZE  9000
#endif


//// global var for openair performance profiler
//int opp_enabled = 0;
//int otg_enabled = 0;

//extern conn_info connInfo[MAX_NO_CONN_TO_PDCP];

//-----------------------------------------------------------------------------
/*
 * If PDCP_UNIT_TEST is set here then data flow between PDCP and RLC is broken
 * and PDCP has no longer anything to do with RLC. In this case, after it's handed
 * an SDU it appends PDCP header and returns (by filling in incoming pointer parameters)
 * this mem_block_t to be dissected for testing purposes. For further details see test
 * code at targets/TEST/PDCP/test_pdcp.c:test_pdcp_data_req()
 */

extern mem_block_t       *pdcp_pdu_p;
extern uint16_t           pdcp_pdu_size;
extern pdcp_t pdcp_array[2];

mem_block_t       *sdu_buffer_pP      = NULL;
BOOL pdcp_p_init = TRUE;

boolean_t
pdcp_data_ind_uplink(
	int pdcpIndex,
	const protocol_ctxt_t* const ctxt_pP,
	const srb_flag_t   srb_flagP,
	const MBMS_flag_t  MBMS_flagP,
	const rb_id_t      rb_idP,
	const sdu_size_t   sdu_buffer_sizeP,
	unsigned char * const sdu_buffer
)
{

  pdcp_t      *pdcp_p          = NULL;
  list_t      *sdu_list_p      = NULL;
  mem_block_t *new_sdu_p       = NULL;
  uint8_t      pdcp_header_len = 0;
  uint8_t      pdcp_tailer_len = 0;
  pdcp_sn_t    sequence_number = 0;
  volatile sdu_size_t   payload_offset  = 0;
  rb_id_t      rb_id            = rb_idP;
  boolean_t    packet_forwarded = FALSE;
  hash_key_t      key             = HASHTABLE_NOT_A_KEY_VALUE;
  hashtable_rc_t  h_rc;

  pdcp_p = &pdcp_array[pdcpIndex];
  pdcp_p->rlc_mode = RLC_MODE_AM;


  if (MBMS_flagP) {
    AssertError (rb_idP < NB_RB_MBMS_MAX, return FALSE,
                 "RB id is too high (%u/%d) %u rnti %x!\n",
                 rb_idP,
                 NB_RB_MBMS_MAX,
                 ctxt_pP->module_id,
                 ctxt_pP->rnti);

    if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
      LOG_D(PDCP, "e-MBMS Data indication notification for PDCP entity from eNB %u to UE %x "
            "and radio bearer ID %d rlc sdu size %d ctxt_pP->enb_flag %d\n",
            ctxt_pP->module_id,
            ctxt_pP->rnti,
            rb_idP,
            sdu_buffer_sizeP,
            ctxt_pP->enb_flag);

    } else {
      LOG_D(PDCP, "Data indication notification for PDCP entity from UE %x to eNB %u "
            "and radio bearer ID %d rlc sdu size %d ctxt_pP->enb_flag %d\n",
            ctxt_pP->rnti,
            ctxt_pP->module_id ,
            rb_idP,
            sdu_buffer_sizeP,
            ctxt_pP->enb_flag);
    }

  } else {
    rb_id = rb_idP % maxDRB;
    AssertError (rb_id < maxDRB, return FALSE, "RB id is too high (%u/%d) %u UE %x!\n",
                 rb_id,
                 maxDRB,
                 ctxt_pP->module_id,
                 ctxt_pP->rnti);
    AssertError (rb_id > 0, return FALSE, "RB id is too low (%u/%d) %u UE %x!\n",
                 rb_id,
                 maxDRB,
                 ctxt_pP->module_id,
                 ctxt_pP->rnti);
  }

  sdu_list_p = &pdcp_sdu_list;
  // PDCP transparent mode for MBMS traffic


//    LOG_D(PDCP, " [TM] Asking for a new mem_block of size %d\n",sdu_buffer_sizeP);
    sdu_buffer_pP = get_free_mem_block(sdu_buffer_sizeP);

    if (sdu_buffer_pP != NULL) {
      memcpy(&sdu_buffer_pP->data[0], sdu_buffer, sdu_buffer_sizeP);
    }

  if (sdu_buffer_sizeP == 0) {
    LOG_W(PDCP, "SDU buffer size is zero! Ignoring this chunk!\n");
    return FALSE;
  }

  /*
   * Parse the PDU placed at the beginning of SDU to check
   * if incoming SN is in line with RX window
   */


  if (MBMS_flagP == 0 ) {
    if (srb_flagP) { //SRB1/2
      pdcp_header_len = PDCP_CONTROL_PLANE_DATA_PDU_SN_SIZE;
      pdcp_tailer_len = PDCP_CONTROL_PLANE_DATA_PDU_MAC_I_SIZE;
      sequence_number =   pdcp_get_sequence_number_of_pdu_with_SRB_sn((unsigned char*)sdu_buffer_pP->data);
    } else { // DRB

#ifdef create_uplink_report
			clock_gettime(CLOCK_MONOTONIC, &seq_uplink_start);
#endif
      pdcp_tailer_len = 0;

      if (pdcp_p->seq_num_size == PDCP_SN_7BIT) {
        pdcp_header_len = PDCP_USER_PLANE_DATA_PDU_SHORT_SN_HEADER_SIZE;
        sequence_number =     pdcp_get_sequence_number_of_pdu_with_short_sn((unsigned char*)sdu_buffer_pP->data);
      } else if (pdcp_p->seq_num_size == PDCP_SN_12BIT) {
        pdcp_header_len = PDCP_USER_PLANE_DATA_PDU_LONG_SN_HEADER_SIZE;
        sequence_number =     pdcp_get_sequence_number_of_pdu_with_long_sn((unsigned char*)sdu_buffer_pP->data);
      } else {
        //sequence_number = 4095;
        LOG_E(PDCP,
              PROTOCOL_PDCP_CTXT_FMT"wrong sequence number  (%d) for this pdcp entity \n",
              PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
              pdcp_p->seq_num_size);
      }

      //uint8_t dc = pdcp_get_dc_filed((unsigned char*)sdu_buffer_pP->data);
    }

    /*
     * Check if incoming SDU is long enough to carry a PDU header
     */
    if (sdu_buffer_sizeP < pdcp_header_len + pdcp_tailer_len ) {
      LOG_W(PDCP,
            PROTOCOL_PDCP_CTXT_FMT"Incoming (from RLC) SDU is short of size (size:%d)! Ignoring...\n",
            PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
            sdu_buffer_sizeP);
      free_mem_block(sdu_buffer_pP);

      return FALSE;
    }

    if (pdcp_is_rx_seq_number_valid(sequence_number, pdcp_p, srb_flagP) == TRUE) {

#ifdef create_uplink_report
			clock_gettime(CLOCK_MONOTONIC, &seq_uplink_end);
			sequencing_time_uplink = (double)(seq_uplink_end.tv_sec - seq_uplink_start.tv_sec)*SEC_TO_NANO_SECONDS +
					(double)(seq_uplink_end.tv_nsec - seq_uplink_start.tv_nsec);
#endif
      printf ("\nSequence number %d verified\n", sequence_number);
    } else {
      LOG_W(PDCP,
            PROTOCOL_PDCP_CTXT_FMT"Incoming PDU has an unexpected sequence number (%d), RX window synchronisation have probably been lost!\n",
            PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
            sequence_number);
    }

    // SRB1/2: control-plane data
    if (srb_flagP) {
#if defined(ENABLE_SECURITY)

      if (pdcp_p->security_activated == 1) {
        if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
          start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
        } else {
          start_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
        }

        pdcp_validate_security(ctxt_pP,
                               pdcp_p,
                               srb_flagP,
                               rb_idP,
                               pdcp_header_len,
                               sequence_number,
                               sdu_buffer_pP->data,
                               sdu_buffer_sizeP - pdcp_tailer_len);

        if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
          stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
        } else {
          stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
        }
      }

#endif
      //rrc_lite_data_ind(module_id, //Modified MW - L2 Interface
  	MSC_LOG_TX_MESSAGE(
  	    (ctxt_pP->enb_flag == ENB_FLAG_NO)? MSC_PDCP_UE:MSC_PDCP_ENB,
        (ctxt_pP->enb_flag == ENB_FLAG_NO)? MSC_RRC_UE:MSC_RRC_ENB,
        NULL,0,
        PROTOCOL_PDCP_CTXT_FMT" DATA-IND len %u",
        PROTOCOL_PDCP_CTXT_ARGS(ctxt_pP, pdcp_p),
        sdu_buffer_sizeP - pdcp_header_len - pdcp_tailer_len);
/*      pdcp_rrc_data_ind(ctxt_pP,
                        rb_id,
                        sdu_buffer_sizeP - pdcp_header_len - pdcp_tailer_len,
                        (uint8_t*)&sdu_buffer_pP->data[pdcp_header_len]);*/
    pdcp_rrc_data_ind_send(ctxt_pP,
                      rb_id,
                      sdu_buffer_sizeP - pdcp_header_len - pdcp_tailer_len,
                      (uint8_t*)&sdu_buffer_pP->data[pdcp_header_len]);
      free_mem_block(sdu_buffer_pP);

      // free_mem_block(new_sdu);
      if (ctxt_pP->enb_flag) {
        stop_meas(&eNB_pdcp_stats[ctxt_pP->module_id].data_ind);
      } else {
        stop_meas(&UE_pdcp_stats[ctxt_pP->module_id].data_ind);
      }

      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDCP_DATA_IND,VCD_FUNCTION_OUT);
      return TRUE;
    }

    /*
     * DRBs
     */
    payload_offset=pdcp_header_len;// PDCP_USER_PLANE_DATA_PDU_LONG_SN_HEADER_SIZE;
#if defined(ENABLE_SECURITY)

#ifdef create_uplink_report
			clock_gettime(CLOCK_MONOTONIC, &decrypt_start);
#endif
    if (pdcp_p->security_activated > 0) {
      if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
        start_meas(&eNB_pdcp_stats[ctxt_pP->module_id].validate_security);
      } else {
        start_meas(&UE_pdcp_stats[ctxt_pP->module_id].validate_security);
      }

      pdcp_validate_security(
        ctxt_pP,
        pdcp_p,
        srb_flagP,
        rb_idP,
        pdcp_header_len,
        sequence_number,
        sdu_buffer_pP->data,
        sdu_buffer_sizeP - pdcp_tailer_len);

//		uint8_t  buffer[4000];
//		memcpy(buffer, &sdu_buffer_pP->data, sdu_buffer_sizeP);

#ifdef create_uplink_report
			clock_gettime(CLOCK_MONOTONIC, &decrypt_end);
			decrypt_time = (double)(decrypt_end.tv_sec - decrypt_start.tv_sec)*SEC_TO_NANO_SECONDS +
					(double)(decrypt_end.tv_nsec - decrypt_start.tv_nsec);
#endif
    }

#endif
  } else {
    payload_offset=0;
  }

  // XXX Decompression would be done at this point

  /*
   * After checking incoming sequence number PDCP header
   * has to be stripped off so here we copy SDU buffer starting
   * from its second byte (skipping 0th and 1st octets, i.e.
   * PDCP header)
   */
#if defined(LINK_ENB_PDCP_TO_GTPV1U)

  if ((TRUE == ctxt_pP->enb_flag) && (FALSE == srb_flagP)) {
    MSC_LOG_TX_MESSAGE(
    		MSC_PDCP_ENB,
    		MSC_GTPU_ENB,
    		NULL,0,
    		"0 GTPV1U_ENB_TUNNEL_DATA_REQ  ue %x rab %u len %u",
    		ctxt_pP->rnti,
    		rb_id + 4,
    		sdu_buffer_sizeP - payload_offset);
    //LOG_T(PDCP,"Sending to GTPV1U %d bytes\n", sdu_buffer_sizeP - payload_offset);
    gtpu_buffer_p = itti_malloc(TASK_PDCP_ENB, TASK_GTPV1_U,
                                sdu_buffer_sizeP - payload_offset + GTPU_HEADER_OVERHEAD_MAX);
    AssertFatal(gtpu_buffer_p != NULL, "OUT OF MEMORY");
    memcpy(&gtpu_buffer_p[GTPU_HEADER_OVERHEAD_MAX], &sdu_buffer_pP->data[payload_offset], sdu_buffer_sizeP - payload_offset);
    message_p = itti_alloc_new_message(TASK_PDCP_ENB, GTPV1U_ENB_TUNNEL_DATA_REQ);
    AssertFatal(message_p != NULL, "OUT OF MEMORY");
    GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).buffer       = gtpu_buffer_p;
    GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).length       = sdu_buffer_sizeP - payload_offset;
    GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).offset       = GTPU_HEADER_OVERHEAD_MAX;
    GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).rnti         = ctxt_pP->rnti;
    GTPV1U_ENB_TUNNEL_DATA_REQ(message_p).rab_id       = rb_id + 4;
    itti_send_msg_to_task(TASK_GTPV1_U, INSTANCE_DEFAULT, message_p);
    packet_forwarded = TRUE;
  }

#else
  packet_forwarded = FALSE;
#endif

  if (FALSE == packet_forwarded) {
    new_sdu_p = get_free_mem_block(sdu_buffer_sizeP - payload_offset + sizeof (pdcp_data_ind_header_t));

    if (new_sdu_p) {
      if (pdcp_p->rlc_mode == RLC_MODE_AM ) {
        pdcp_p->last_submitted_pdcp_rx_sn = sequence_number;
      }

      /*
       * Prepend PDCP indication header which is going to be removed at pdcp_fifo_flush_sdus()
       */
      memset(new_sdu_p->data, 0, sizeof (pdcp_data_ind_header_t));
      ((pdcp_data_ind_header_t *) new_sdu_p->data)->data_size = sdu_buffer_sizeP - payload_offset;

      // Here there is no virtualization possible
      // set ((pdcp_data_ind_header_t *) new_sdu_p->data)->inst for IP layer here
      if (ctxt_pP->enb_flag == ENB_FLAG_NO) {
        ((pdcp_data_ind_header_t *) new_sdu_p->data)->rb_id = rb_id;
#if defined(OAI_EMU)
        ((pdcp_data_ind_header_t*) new_sdu_p->data)->inst  = ctxt_pP->module_id + oai_emulation.info.nb_enb_local - oai_emulation.info.first_ue_local;
#endif
      } else {
        ((pdcp_data_ind_header_t*) new_sdu_p->data)->rb_id = rb_id + (ctxt_pP->module_id * maxDRB);
#if defined(OAI_EMU)
        ((pdcp_data_ind_header_t*) new_sdu_p->data)->inst  = ctxt_pP->module_id - oai_emulation.info.first_enb_local;
#endif
      }

      memcpy(&new_sdu_p->data[sizeof (pdcp_data_ind_header_t)], \
             &sdu_buffer_pP->data[payload_offset], \
             sdu_buffer_sizeP - payload_offset);
//      list_add_tail_eurecom (new_sdu_p, sdu_list_p);

      /* Print octets of incoming data in hexadecimal form */
/*      LOG_D(PDCP, "Following content has been received from RLC (%d,%d)(PDCP header has already been removed):\n",
            sdu_buffer_sizeP  - payload_offset + sizeof(pdcp_data_ind_header_t),
            sdu_buffer_sizeP  - payload_offset);*/
      //util_print_hex_octets(PDCP, &new_sdu_p->data[sizeof (pdcp_data_ind_header_t)], sdu_buffer_sizeP - payload_offset);
      //util_flush_hex_octets(PDCP, &new_sdu_p->data[sizeof (pdcp_data_ind_header_t)], sdu_buffer_sizeP - payload_offset);

      /*
       * Update PDCP statistics
       * XXX Following two actions are identical, is there a merge error?
       */

      /*if (ctxt_pP->enb_flag == 1) {
          Pdcp_stats_rx[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH]++;
          Pdcp_stats_rx_bytes[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH] += sdu_buffer_sizeP;
        } else {
          Pdcp_stats_rx[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH]++;
          Pdcp_stats_rx_bytes[module_id][(rb_idP & RAB_OFFSET2) >> RAB_SHIFT2][(rb_idP & RAB_OFFSET) - DTCH] += sdu_buffer_sizeP;
        }*/
    }
    free_mem_block (new_sdu_p);
  }

  free_mem_block(sdu_buffer_pP);

  return TRUE;

}
