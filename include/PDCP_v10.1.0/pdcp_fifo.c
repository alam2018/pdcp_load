/*******************************************************************************
    OpenAirInterface
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is
   included in this distribution in the file called "COPYING". If not,
   see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@lists.eurecom.fr

  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

 *******************************************************************************/

/*! \file pdcp_fifo.c
 * \brief pdcp interface with linux IP interface, have a look at http://man7.org/linux/man-pages/man7/netlink.7.html for netlink
 * \author  Lionel GAUTHIER and Navid Nikaein
 * \date 2009
 * \version 0.5
 * \warning This component can be runned only in user-space
 * @ingroup pdcp
 */

#define PDCP_FIFO_C
#define PDCP_DEBUG 1

//ALAM
#include "collec_dec_pdcp.h"
#include "OCG_vars.h"
#include "phy_vars.h"
#include "list.h"

#ifndef OAI_EMU
extern int otg_enabled;
#endif

#include "pdcp.h"
#include "pdcp_primitives.h"

#ifdef USER_MODE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define rtf_put write
#define rtf_get read
#else
//#include <rtai_fifos.h>
#endif //USER_MODE

//#include "../MAC/extern.h"
//#include "RRC/L2_INTERFACE/openair_rrc_L2_interface.h"
//#include "NETWORK_DRIVER/LITE/constant.h"
//#include "SIMULATION/ETH_TRANSPORT/extern.h"
#include "OCG.h"
//#include "UTIL/OCG/OCG_extern.h"
#include "log.h"
#include "otg_tx.h"
//#include "UTIL/FIFO/pad_list.h"
#include "../platform_constants.h"
#include "msc.h"

#include "../assertions.h"

#ifdef PDCP_USE_NETLINK
#include <sys/socket.h>
#include <linux/netlink.h>

extern char nl_rx_buf[NL_MAX_PAYLOAD];
extern struct sockaddr_nl nas_src_addr, nas_dest_addr;
extern struct nlmsghdr *nas_nlh_tx;
extern struct nlmsghdr *nas_nlh_rx;
extern struct iovec nas_iov_tx;
extern struct iovec nas_iov_rx;
extern int nas_sock_fd;
extern struct msghdr nas_msg_tx;
extern struct msghdr nas_msg_rx;

unsigned char pdcp_read_state_g = 0;
#endif

//extern Packet_OTG_List_t *otg_pdcp_buffer;

#if defined(LINK_ENB_PDCP_TO_GTPV1U)
#  include "gtpv1u_eNB_task.h"
#endif

pdcp_data_req_header_t pdcp_read_header_g;

//-----------------------------------------------------------------------------
int pdcp_fifo_flush_sdus(const protocol_ctxt_t* const  ctxt_pP)
{
  //-----------------------------------------------------------------------------

  mem_block_t     *sdu_p            = list_get_head (&pdcp_sdu_list);
  int              bytes_wrote      = 0;
  int              pdcp_nb_sdu_sent = 0;
  uint8_t          cont             = 1;
#if defined(LINK_ENB_PDCP_TO_GTPV1U)
  //MessageDef      *message_p        = NULL;
#endif

#if defined(PDCP_USE_NETLINK) && defined(LINUX)
  int ret = 0;
#endif

  while (sdu_p && cont) {

#if ! defined(OAI_EMU)
    ((pdcp_data_ind_header_t *)(sdu_p->data))->inst = 0;
#endif

#if defined(LINK_ENB_PDCP_TO_GTPV1U)

    if (ctxt_pP->enb_flag) {
      AssertFatal(0, "Now execution should not go here");
      LOG_D(PDCP,"Sending to GTPV1U %d bytes\n", ((pdcp_data_ind_header_t *)(sdu_p->data))->data_size);
      gtpv1u_new_data_req(
        ctxt_pP->module_id, //gtpv1u_data_t *gtpv1u_data_p,
        ctxt_pP->rnti,//rb_id/maxDRB, TO DO UE ID
        ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id + 4,
        &(((uint8_t *) sdu_p->data)[sizeof (pdcp_data_ind_header_t)]),
        ((pdcp_data_ind_header_t *)(sdu_p->data))->data_size);

      list_remove_head (&pdcp_sdu_list);
      free_mem_block (sdu_p);
      cont = 1;
      pdcp_nb_sdu_sent += 1;
      sdu_p = list_get_head (&pdcp_sdu_list);
      LOG_D(OTG,"After  GTPV1U\n");
      continue; // loop again
    }

#endif /* defined(ENABLE_USE_MME) */
#ifdef PDCP_DEBUG
    LOG_D(PDCP, "PDCP->IP TTI %d INST %d: Preparing %d Bytes of data from rab %d to Nas_mesh\n",
          ctxt_pP->frame, ((pdcp_data_ind_header_t *)(sdu_p->data))->inst,
          ((pdcp_data_ind_header_t *)(sdu_p->data))->data_size, ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id);
#endif //PDCP_DEBUG
    cont = 0;

    if (!pdcp_output_sdu_bytes_to_write) {
      if (!pdcp_output_header_bytes_to_write) {
        pdcp_output_header_bytes_to_write = sizeof (pdcp_data_ind_header_t);
      }

#ifdef PDCP_USE_RT_FIFO
      bytes_wrote = rtf_put (PDCP2PDCP_USE_RT_FIFO,
                             &(((uint8_t *) sdu->data)[sizeof (pdcp_data_ind_header_t) - pdcp_output_header_bytes_to_write]),
                             pdcp_output_header_bytes_to_write);

#else
#ifdef PDCP_USE_NETLINK
#ifdef LINUX
      memcpy(NLMSG_DATA(nas_nlh_tx), &(((uint8_t *) sdu_p->data)[sizeof (pdcp_data_ind_header_t) - pdcp_output_header_bytes_to_write]),
             pdcp_output_header_bytes_to_write);
      nas_nlh_tx->nlmsg_len = pdcp_output_header_bytes_to_write;
#endif //LINUX
#endif //PDCP_USE_NETLINK

      bytes_wrote = pdcp_output_header_bytes_to_write;
#endif //PDCP_USE_RT_FIFO

#ifdef PDCP_DEBUG
      LOG_D(PDCP, "Frame %d Sent %d Bytes of header to Nas_mesh\n",
            ctxt_pP->frame,
            bytes_wrote);
#endif //PDCP_DEBUG

      if (bytes_wrote > 0) {
        pdcp_output_header_bytes_to_write = pdcp_output_header_bytes_to_write - bytes_wrote;

        if (!pdcp_output_header_bytes_to_write) { // continue with sdu
          pdcp_output_sdu_bytes_to_write = ((pdcp_data_ind_header_t *) sdu_p->data)->data_size;

#ifdef PDCP_USE_RT_FIFO
          bytes_wrote = rtf_put (PDCP2PDCP_USE_RT_FIFO, &(sdu->data[sizeof (pdcp_data_ind_header_t)]), pdcp_output_sdu_bytes_to_write);
#else

#ifdef PDCP_USE_NETLINK
#ifdef LINUX
          memcpy(NLMSG_DATA(nas_nlh_tx)+sizeof(pdcp_data_ind_header_t), &(sdu_p->data[sizeof (pdcp_data_ind_header_t)]), pdcp_output_sdu_bytes_to_write);
          nas_nlh_tx->nlmsg_len += pdcp_output_sdu_bytes_to_write;
          ret = sendmsg(nas_sock_fd,&nas_msg_tx,0);

          if (ret<0) {
            LOG_D(PDCP, "[PDCP_FIFOS] sendmsg returns %d (errno: %d)\n", ret, errno);
      	    MSC_LOG_TX_MESSAGE_FAILED(
      	      (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
      	      (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_IP_ENB:MSC_IP_UE,
      	      NULL,
      	      0,
      	      MSC_AS_TIME_FMT" DATA-IND RNTI %"PRIx16" rb %u size %u",
      	      MSC_AS_TIME_ARGS(ctxt_pP),
      	      ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id,
      	      ((pdcp_data_ind_header_t *)(sdu_p->data))->data_size);
           mac_xface->macphy_exit("sendmsg failed for nas_sock_fd\n");
            break;
          } else {
        	  MSC_LOG_TX_MESSAGE(
        	    (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
        	    (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_IP_ENB:MSC_IP_UE,
        	    NULL,
        	    0,
        	    MSC_AS_TIME_FMT" DATA-IND RNTI %"PRIx16" rb %u size %u",
        	    MSC_AS_TIME_ARGS(ctxt_pP),
        	    ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id,
        	    ((pdcp_data_ind_header_t *)(sdu_p->data))->data_size);
          }

#endif // LINUX
#endif //PDCP_USE_NETLINK
          bytes_wrote= pdcp_output_sdu_bytes_to_write;
#endif // PDCP_USE_RT_FIFO

#ifdef PDCP_DEBUG
          LOG_D(PDCP, "PDCP->IP Frame %d INST %d: Sent %d Bytes of data from rab %d to higher layers\n",
                ctxt_pP->frame,
                ((pdcp_data_ind_header_t *)(sdu_p->data))->inst,
                bytes_wrote,
                ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id);
#endif //PDCP_DEBUG

          if (bytes_wrote > 0) {
            pdcp_output_sdu_bytes_to_write -= bytes_wrote;

            if (!pdcp_output_sdu_bytes_to_write) { // OK finish with this SDU
              // LOG_D(PDCP, "rb sent a sdu qos_sap %d\n", sapiP);
              LOG_D(PDCP,
                    "[FRAME %05d][xxx][PDCP][MOD xx/xx][RB %u][--- PDCP_DATA_IND / %d Bytes --->][IP][INSTANCE %u][RB %u]\n",
                    ctxt_pP->frame,
                    ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id,
                    ((pdcp_data_ind_header_t *)(sdu_p->data))->data_size,
                    ((pdcp_data_ind_header_t *)(sdu_p->data))->inst,
                    ((pdcp_data_ind_header_t *)(sdu_p->data))->rb_id);

              list_remove_head (&pdcp_sdu_list);
              free_mem_block (sdu_p);
              cont = 1;
              pdcp_nb_sdu_sent += 1;
              sdu_p = list_get_head (&pdcp_sdu_list);
            }
          } else {
            LOG_W(PDCP, "RADIO->IP SEND SDU CONGESTION!\n");
          }
        } else {
          LOG_W(PDCP, "RADIO->IP SEND SDU CONGESTION!\n");
        }
      }
    } else {
      // continue writing sdu
#ifdef PDCP_USE_RT_FIFO
      bytes_wrote = rtf_put (PDCP2PDCP_USE_RT_FIFO,
                             (uint8_t *) (&(sdu_p->data[sizeof (pdcp_data_ind_header_t) + ((pdcp_data_ind_header_t *) sdu_p->data)->data_size - pdcp_output_sdu_bytes_to_write])),
                             pdcp_output_sdu_bytes_to_write);
#else  // PDCP_USE_RT_FIFO
      bytes_wrote = pdcp_output_sdu_bytes_to_write;
#endif  // PDCP_USE_RT_FIFO

      if (bytes_wrote > 0) {
        pdcp_output_sdu_bytes_to_write -= bytes_wrote;

        if (!pdcp_output_sdu_bytes_to_write) {     // OK finish with this SDU
          //PRINT_RB_SEND_OUTPUT_SDU ("[PDCP] RADIO->IP SEND SDU\n");
          list_remove_head (&pdcp_sdu_list);
          free_mem_block (sdu_p);
          cont = 1;
          pdcp_nb_sdu_sent += 1;
          sdu_p = list_get_head (&pdcp_sdu_list);
          // LOG_D(PDCP, "rb sent a sdu from rab\n");
        }
      }
    }
  }

#ifdef PDCP_USE_RT_FIFO

  if ((pdcp_nb_sdu_sent)) {
    if ((pdcp_2_nas_irq > 0)) {
#ifdef PDCP_DEBUG
      LOG_D(PDCP, "Frame %d : Trigger NAS RX interrupt\n",
            ctxt_pP->frame);
#endif //PDCP_DEBUG
      rt_pend_linux_srq (pdcp_2_nas_irq);

    } else {
      LOG_E(PDCP, "Frame %d: ERROR IF IP STACK WANTED : NOTIF PACKET(S) pdcp_2_nas_irq not initialized : %d\n",
            ctxt_pP->frame,
            pdcp_2_nas_irq);
    }
  }

#endif  //PDCP_USE_RT_FIFO

  return pdcp_nb_sdu_sent;
}

//-----------------------------------------------------------------------------
int pdcp_fifo_read_input_sdus (const protocol_ctxt_t* const  ctxt_pP)
{
#ifdef PDCP_USE_NETLINK
  protocol_ctxt_t                ctxt_cpy = *ctxt_pP;
  protocol_ctxt_t                ctxt;
  hash_key_t                     key       = HASHTABLE_NOT_A_KEY_VALUE;
  hashtable_rc_t                 h_rc;
  struct pdcp_netlink_element_s* data_p    = NULL;
  module_id_t                    ue_id     = 0;
  pdcp_t*                        pdcp_p    = NULL;
# if defined(PDCP_USE_NETLINK_QUEUES)
  rb_id_t                        rab_id    = 0;

  pdcp_transmission_mode_t       pdcp_mode = PDCP_TRANSMISSION_MODE_UNKNOWN;


  while (pdcp_netlink_dequeue_element(ctxt_pP, &data_p) != 0) {
    DevAssert(data_p != NULL);
    rab_id = data_p->pdcp_read_header.rb_id % maxDRB;
    // ctxt_pP->rnti is NOT_A_RNTI
    ctxt_cpy.rnti = pdcp_module_id_to_rnti[ctxt_cpy.module_id][data_p->pdcp_read_header.inst];
    key = PDCP_COLL_KEY_VALUE(ctxt_pP->module_id, ctxt_cpy.rnti, ctxt_pP->enb_flag, rab_id, SRB_FLAG_NO);
    h_rc = hashtable_get(pdcp_coll_p, key, (void**)&pdcp_p);

    if (h_rc != HASH_TABLE_OK) {
      LOG_W(PDCP, PROTOCOL_CTXT_FMT" Dropped IP PACKET cause no PDCP instanciated\n",
            PROTOCOL_CTXT_ARGS(ctxt_pP));
      free(data_p->data);
      free(data_p);
      data_p = NULL;
      continue;
    }

    CHECK_CTXT_ARGS(&ctxt_cpy);

    AssertFatal (rab_id    < maxDRB,                       "RB id is too high (%u/%d)!\n", rab_id, maxDRB);

    if (rab_id != 0) {
        LOG_D(PDCP, "[FRAME %05d][%s][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ "
            "/ %d Bytes --->][PDCP][MOD %u][RB %u]\n",
              ctxt_cpy.frame,
              (ctxt_cpy.enb_flag) ? "eNB" : "UE",
            data_p->pdcp_read_header.inst,
            data_p->pdcp_read_header.rb_id,
            data_p->pdcp_read_header.data_size,
            ctxt_cpy.module_id,
              rab_id);
#ifdef  OAI_NW_DRIVER_TYPE_ETHERNET

      if ((data_p->pdcp_read_header.traffic_type == TRAFFIC_IPV6_TYPE_MULTICAST) /*TRAFFIC_IPV6_TYPE_MULTICAST */ ||
          (data_p->pdcp_read_header.traffic_type == TRAFFIC_IPV4_TYPE_MULTICAST) /*TRAFFIC_IPV4_TYPE_MULTICAST */ ||
          (data_p->pdcp_read_header.traffic_type == TRAFFIC_IPV4_TYPE_BROADCAST) /*TRAFFIC_IPV4_TYPE_BROADCAST */ ) {
#if defined (Rel10)
          PDCP_TRANSMISSION_MODE_TRANSPARENT;
#else
          pdcp_mode= PDCP_TRANSMISSION_MODE_DATA;
#endif
      } else if ((data_p->pdcp_read_header.traffic_type == TRAFFIC_IPV6_TYPE_UNICAST) /* TRAFFIC_IPV6_TYPE_UNICAST */ ||
                 (data_p->pdcp_read_header.traffic_type == TRAFFIC_IPV4_TYPE_UNICAST) /*TRAFFIC_IPV4_TYPE_UNICAST*/ ) {
          pdcp_mode=  PDCP_TRANSMISSION_MODE_DATA;
        } else {
          pdcp_mode= PDCP_TRANSMISSION_MODE_DATA;
          LOG_W(PDCP,"unknown IP traffic type \n");
        }

#else // OAI_NW_DRIVER_TYPE_ETHERNET NASMESH driver does not curreenlty support multicast traffic
        pdcp_mode = PDCP_TRANSMISSION_MODE_DATA;
#endif
      pdcp_data_req(&ctxt_cpy,
                      SRB_FLAG_NO,
                      rab_id % maxDRB,
                      RLC_MUI_UNDEFINED,
                      RLC_SDU_CONFIRM_NO,
                    data_p->pdcp_read_header.data_size,
                    data_p->data,
                      pdcp_mode);
    } else if (ctxt_cpy.enb_flag) {
      /* rb_id = 0, thus interpreated as broadcast and transported as
       * multiple unicast is a broadcast packet, we have to send this
       * packet on all default RABS of all connected UEs
       */
      LOG_D(PDCP, "eNB Try Forcing send on DEFAULT_RAB_ID first_ue_local %u nb_ue_local %u\n", oai_emulation.info.first_ue_local, oai_emulation.info.nb_ue_local);

      for (ue_id = 0; ue_id < NB_UE_INST; ue_id++) {
        if (pdcp_module_id_to_rnti[ctxt_cpy.module_id][ue_id] != NOT_A_RNTI) {
          LOG_D(PDCP, "eNB Try Forcing send on DEFAULT_RAB_ID UE %d\n", ue_id);
          ctxt.module_id     = ctxt_cpy.module_id;
          ctxt.rnti          = ctxt_cpy.pdcp_module_id_to_rnti[ctxt_cpy.module_id][ue_id];
          ctxt.frame         = ctxt_cpy.frame;
          ctxt.enb_flag      = ctxt_cpy.enb_flag;

          pdcp_data_req(
            &ctxt,
            SRB_FLAG_NO,
            DEFAULT_RAB_ID,
            RLC_MUI_UNDEFINED,
            RLC_SDU_CONFIRM_NO,
            data_p->pdcp_read_header.data_size,
            data_p->data,
            PDCP_TRANSMISSION_MODE_DATA);
        }
      }
    } else {
      LOG_D(PDCP, "Forcing send on DEFAULT_RAB_ID\n");
      pdcp_data_req(
        &ctxt_cpy,
        SRB_FLAG_NO,
        DEFAULT_RAB_ID,
        RLC_MUI_UNDEFINED,
        RLC_SDU_CONFIRM_NO,
        data_p->pdcp_read_header.data_size,
        data_p->data,
        PDCP_TRANSMISSION_MODE_DATA);
    }

    free(data_p->data);
    free(data_p);
    data_p = NULL;
  }

  return 0;
# else /* PDCP_USE_NETLINK_QUEUES*/
  int              len = 1;
  rb_id_t          rab_id  = 0;

  while (len > 0) {
    len = recvmsg(nas_sock_fd, &nas_msg_rx, 0);

    if (len<=0) {
      // nothing in pdcp NAS socket
      //LOG_D(PDCP, "[PDCP][NETLINK] Nothing in socket, length %d \n", len);
    } else {
      for (nas_nlh_rx = (struct nlmsghdr *) nl_rx_buf;
           NLMSG_OK (nas_nlh_rx, len);
           nas_nlh_rx = NLMSG_NEXT (nas_nlh_rx, len)) {

        if (nas_nlh_rx->nlmsg_type == NLMSG_DONE) {
          LOG_D(PDCP, "[PDCP][NETLINK] RX NLMSG_DONE\n");
          //return;
        }

        if (nas_nlh_rx->nlmsg_type == NLMSG_ERROR) {
          LOG_D(PDCP, "[PDCP][NETLINK] RX NLMSG_ERROR\n");
        }

        if (pdcp_read_state_g == 0) {
          if (nas_nlh_rx->nlmsg_len == sizeof (pdcp_data_req_header_t) + sizeof(struct nlmsghdr)) {
            pdcp_read_state_g = 1;  //get
            memcpy((void *)&pdcp_read_header_g, (void *)NLMSG_DATA(nas_nlh_rx), sizeof(pdcp_data_req_header_t));
            LOG_D(PDCP, "[PDCP][NETLINK] RX pdcp_data_req_header_t inst %u, rb_id %u data_size %d\n",
                  pdcp_read_header_g.inst, pdcp_read_header_g.rb_id, pdcp_read_header_g.data_size);
          } else {
            LOG_E(PDCP, "[PDCP][NETLINK] WRONG size %d should be sizeof (pdcp_data_req_header_t) + sizeof(struct nlmsghdr)\n",
                  nas_nlh_rx->nlmsg_len);
          }
        } else {
          pdcp_read_state_g = 0;
          // print_active_requests()
#ifdef PDCP_DEBUG
          LOG_D(PDCP, "[PDCP][NETLINK] Something in socket, length %d \n",
                nas_nlh_rx->nlmsg_len - sizeof(struct nlmsghdr));
#endif

#ifdef OAI_EMU

          // overwrite function input parameters, because only one netlink socket for all instances
          if (pdcp_read_header_g.inst < oai_emulation.info.nb_enb_local) {
            ctxt.frame         = ctxt_cpy.frame;
            ctxt.enb_flag      = ENB_FLAG_YES;
            ctxt.module_id     = pdcp_read_header_g.inst  +  oai_emulation.info.first_enb_local;
            ctxt.rnti          = oai_emulation.info.eNB_ue_module_id_to_rnti[ctxt.module_id ][pdcp_read_header_g.rb_id / maxDRB + oai_emulation.info.first_ue_local];
            rab_id    = pdcp_read_header_g.rb_id % maxDRB;
          } else {
            ctxt.frame         = ctxt_cpy.frame;
            ctxt.enb_flag      = ENB_FLAG_NO;
            ctxt.module_id     = pdcp_read_header_g.inst - oai_emulation.info.nb_enb_local + oai_emulation.info.first_ue_local;
            ctxt.rnti          = pdcp_UE_UE_module_id_to_rnti[ctxt.module_id];
            rab_id    = pdcp_read_header_g.rb_id;
          }

          CHECK_CTXT_ARGS(&ctxt);
          AssertFatal (rab_id    < maxDRB,                       "RB id is too high (%u/%d)!\n", rab_id, maxDRB);
          /*LGpdcp_read_header.inst = (pdcp_read_header_g.inst >= oai_emulation.info.nb_enb_local) ? \
                  pdcp_read_header_g.inst - oai_emulation.info.nb_enb_local+ NB_eNB_INST + oai_emulation.info.first_ue_local :
                  pdcp_read_header_g.inst +  oai_emulation.info.first_enb_local;*/
#else // OAI_EMU
          pdcp_read_header_g.inst = 0;
#warning "TO DO CORRCT VALUES FOR ue mod id, enb mod id"
          ctxt.frame         = ctxt_cpy.frame;
          ctxt.enb_flag      = ctxt_cpy.enb_flag;

          if (ctxt_cpy.enb_flag) {
            ctxt.module_id = 0;
            rab_id      = pdcp_read_header_g.rb_id % maxDRB;
            ctxt.rnti          = pdcp_eNB_UE_instance_to_rnti[pdcp_eNB_UE_instance_to_rnti_index];
          } else {
            ctxt.module_id = 0;
            rab_id      = pdcp_read_header_g.rb_id % maxDRB;
            ctxt.rnti          = pdcp_UE_UE_module_id_to_rnti[ctxt.module_id];
          }


#endif

          if (ctxt.enb_flag) {
            if (rab_id != 0) {
              rab_id = rab_id % maxDRB;
              key = PDCP_COLL_KEY_VALUE(ctxt.module_id, ctxt.rnti, ctxt.enb_flag, rab_id, SRB_FLAG_NO);
              h_rc = hashtable_get(pdcp_coll_p, key, (void**)&pdcp_p);

              if (h_rc == HASH_TABLE_OK) {
#ifdef PDCP_DEBUG
                LOG_D(PDCP, "[FRAME %5u][eNB][NETLINK][IP->PDCP] INST %d: Received socket with length %d (nlmsg_len = %d) on Rab %d \n",
                      ctxt.frame,
                      pdcp_read_header_g.inst,
                      len,
                      nas_nlh_rx->nlmsg_len-sizeof(struct nlmsghdr),
                      pdcp_read_header_g.rb_id);
#endif

          	    MSC_LOG_RX_MESSAGE(
          	      (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
                  (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_IP_ENB:MSC_IP_UE,
          	      NULL,
          	      0,
          	      MSC_AS_TIME_FMT" DATA-REQ inst %u rb %u rab %u size %u",
          	      MSC_AS_TIME_ARGS(ctxt_pP),
                  pdcp_read_header_g.inst,
          	      pdcp_read_header_g.rb_id,
          	      rab_id,
          	      pdcp_read_header_g.data_size);
                LOG_D(PDCP, "[FRAME %5u][eNB][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ / %d Bytes --->][PDCP][MOD %u]UE %u][RB %u]\n",
                      ctxt_cpy.frame,
                      pdcp_read_header_g.inst,
                      pdcp_read_header_g.rb_id,
                      pdcp_read_header_g.data_size,
                      ctxt.module_id,
                      ctxt.rnti,
                      rab_id);

                pdcp_data_req(&ctxt,
                              SRB_FLAG_NO,
                              rab_id,
                              RLC_MUI_UNDEFINED,
                              RLC_SDU_CONFIRM_NO,
                              pdcp_read_header_g.data_size,
                              (unsigned char *)NLMSG_DATA(nas_nlh_rx),
                              PDCP_TRANSMISSION_MODE_DATA);
              } else {
                LOG_D(PDCP, "[FRAME %5u][eNB][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ / %d Bytes ---X][PDCP][MOD %u][UE %u][RB %u] NON INSTANCIATED INSTANCE, DROPPED\n",
                      ctxt.frame,
                      pdcp_read_header_g.inst,
                      pdcp_read_header_g.rb_id,
                      pdcp_read_header_g.data_size,
                      ctxt.module_id,
                      ctxt.rnti,
                      rab_id);
              }
            } else  { // rb_id =0, thus interpreated as broadcast and transported as multiple unicast
              // is a broadcast packet, we have to send this packet on all default RABS of all connected UEs
#warning CODE TO BE REVIEWED, ONLY WORK FOR SIMPLE TOPOLOGY CASES
              for (ue_id = 0; ue_id < NB_UE_INST; ue_id++) {
                if (oai_emulation.info.eNB_ue_module_id_to_rnti[ctxt_cpy.module_id][ue_id] != NOT_A_RNTI) {
                  ctxt.rnti = oai_emulation.info.eNB_ue_module_id_to_rnti[ctxt_cpy.module_id][ue_id];
                  LOG_D(PDCP, "[FRAME %5u][eNB][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ / %d Bytes --->][PDCP][MOD %u][UE %u][RB DEFAULT_RAB_ID %u]\n",
                        ctxt.frame,
                        pdcp_read_header_g.inst,
                        pdcp_read_header_g.rb_id,
                        pdcp_read_header_g.data_size,
                        ctxt.module_id,
                        ctxt.rnti,
                        DEFAULT_RAB_ID);
                  pdcp_data_req (
                    &ctxt,
                    SRB_FLAG_NO,
                    DEFAULT_RAB_ID,
                    RLC_MUI_UNDEFINED,
                    RLC_SDU_CONFIRM_NO,
                    pdcp_read_header_g.data_size,
                    (unsigned char *)NLMSG_DATA(nas_nlh_rx),
                    PDCP_TRANSMISSION_MODE_DATA);
                }
              }
            }
          } else { // enb_flag
            if (rab_id != 0) {
              rab_id = rab_id % maxDRB;
              key = PDCP_COLL_KEY_VALUE(ctxt.module_id, ctxt.rnti, ctxt.enb_flag, rab_id, SRB_FLAG_NO);
              h_rc = hashtable_get(pdcp_coll_p, key, (void**)&pdcp_p);

              if (h_rc == HASH_TABLE_OK) {
#ifdef PDCP_DEBUG
                LOG_D(PDCP, "[FRAME %5u][UE][NETLINK][IP->PDCP] INST %d: Received socket with length %d (nlmsg_len = %d) on Rab %d \n",
                      ctxt.frame,
                      pdcp_read_header_g.inst,
                      len,
                      nas_nlh_rx->nlmsg_len-sizeof(struct nlmsghdr),
                      pdcp_read_header_g.rb_id);

                LOG_D(PDCP, "[FRAME %5u][UE][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ / %d Bytes --->][PDCP][MOD %u][UE %u][RB %u]\n",
                      ctxt.frame,
                      pdcp_read_header_g.inst,
                      pdcp_read_header_g.rb_id,
                      pdcp_read_header_g.data_size,
                      ctxt.module_id,
                      ctxt.rnti,
                      rab_id);
#endif
          	    MSC_LOG_RX_MESSAGE(
          	      (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
                  (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_IP_ENB:MSC_IP_UE,
          	      NULL,
          	      0,
          	      MSC_AS_TIME_FMT" DATA-REQ inst %u rb %u rab %u size %u",
          	      MSC_AS_TIME_ARGS(ctxt_pP),
                  pdcp_read_header_g.inst,
          	      pdcp_read_header_g.rb_id,
          	      rab_id,
          	      pdcp_read_header_g.data_size);

                pdcp_data_req(
                  &ctxt,
                  SRB_FLAG_NO,
                  rab_id,
                  RLC_MUI_UNDEFINED,
                  RLC_SDU_CONFIRM_NO,
                  pdcp_read_header_g.data_size,
                  (unsigned char *)NLMSG_DATA(nas_nlh_rx),
                  PDCP_TRANSMISSION_MODE_DATA);
              } else {
            	MSC_LOG_RX_DISCARDED_MESSAGE(
                  (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
                  (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_IP_ENB:MSC_IP_UE,
                  NULL,
                  0,
                  MSC_AS_TIME_FMT" DATA-REQ inst %u rb %u rab %u size %u",
                  MSC_AS_TIME_ARGS(ctxt_pP),
                  pdcp_read_header_g.inst,
                  pdcp_read_header_g.rb_id,
                  rab_id,
                  pdcp_read_header_g.data_size);
                LOG_D(PDCP,
                      "[FRAME %5u][UE][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ / %d Bytes ---X][PDCP][MOD %u][UE %u][RB %u] NON INSTANCIATED INSTANCE key 0x%"PRIx64", DROPPED\n",
                      ctxt.frame,
                      pdcp_read_header_g.inst,
                      pdcp_read_header_g.rb_id,
                      pdcp_read_header_g.data_size,
                      ctxt.module_id,
                      ctxt.rnti,
                      rab_id,
                      key);
              }
            }  else {
              LOG_D(PDCP, "Forcing send on DEFAULT_RAB_ID\n");
              LOG_D(PDCP, "[FRAME %5u][eNB][IP][INSTANCE %u][RB %u][--- PDCP_DATA_REQ / %d Bytes --->][PDCP][MOD %u][UE %u][RB DEFAULT_RAB_ID %u]\n",
                    ctxt.frame,
                    pdcp_read_header_g.inst,
                    pdcp_read_header_g.rb_id,
                    pdcp_read_header_g.data_size,
                    ctxt.module_id,
                    ctxt.rnti,
                    DEFAULT_RAB_ID);
          	  MSC_LOG_RX_MESSAGE(
                (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_PDCP_ENB:MSC_PDCP_UE,
                (ctxt_pP->enb_flag == ENB_FLAG_YES) ? MSC_IP_ENB:MSC_IP_UE,
                NULL,0,
                MSC_AS_TIME_FMT" DATA-REQ inst %u rb %u default rab %u size %u",
                MSC_AS_TIME_ARGS(ctxt_pP),
                pdcp_read_header_g.inst,
                pdcp_read_header_g.rb_id,
                DEFAULT_RAB_ID,
                pdcp_read_header_g.data_size);

              pdcp_data_req (
                &ctxt,
                SRB_FLAG_NO,
                DEFAULT_RAB_ID,
                RLC_MUI_UNDEFINED,
                RLC_SDU_CONFIRM_NO,
                pdcp_read_header_g.data_size,
                (unsigned char *)NLMSG_DATA(nas_nlh_rx),
                PDCP_TRANSMISSION_MODE_DATA);
            }
          }

        }
      }
    }
  }

  return len;
# endif
#else // neither PDCP_USE_NETLINK nor PDCP_USE_RT_FIFO
  return 0;
#endif // PDCP_USE_NETLINK
}


void pdcp_fifo_read_input_sdus_from_otg (const protocol_ctxt_t* const  ctxt_pP) {

  unsigned char       *otg_pkt=NULL;
  module_id_t          dst_id; // dst for otg
  rb_id_t              rb_id;
  unsigned int         pkt_size=0;
#if defined(USER_MODE) && defined(OAI_EMU)
  module_id_t          src_id;
  static unsigned int  pkt_cnt_enb=0, pkt_cnt_ue=0;

  Packet_otg_elt_t    *otg_pkt_info=NULL;
  int                  result;
  uint8_t              pdcp_mode, is_ue=0;
#endif
  protocol_ctxt_t      ctxt;
  // we need to add conditions to avoid transmitting data when the UE is not RRC connected.
#if defined(USER_MODE) && defined(OAI_EMU)

  if (oai_emulation.info.otg_enabled ==1 ) {
    // module_id is source id
    while ((otg_pkt_info = pkt_list_remove_head(&(otg_pdcp_buffer[ctxt_pP->instance]))) != NULL) {
      LOG_I(OTG,"Mod_id %d Frame %d Got a packet (%p), HEAD of otg_pdcp_buffer[%d] is %p and Nb elements is %d\n",
            ctxt_pP->module_id,
            ctxt_pP->frame,
            otg_pkt_info,
            ctxt_pP->instance,
            pkt_list_get_head(&(otg_pdcp_buffer[ctxt_pP->instance])),
            otg_pdcp_buffer[ctxt_pP->instance].nb_elements);
      //otg_pkt_info = pkt_list_remove_head(&(otg_pdcp_buffer[module_id]));
      dst_id    = (otg_pkt_info->otg_pkt).dst_id; // type is module_id_t
      src_id    = (otg_pkt_info->otg_pkt).module_id; // type is module_id_t
      rb_id     = (otg_pkt_info->otg_pkt).rb_id;
      is_ue     = (otg_pkt_info->otg_pkt).is_ue;
      pdcp_mode = (otg_pkt_info->otg_pkt).mode;
      //    LOG_I(PDCP,"pdcp_fifo, pdcp mode is= %d\n",pdcp_mode);

      // generate traffic if the ue is rrc reconfigured state
      // if (mac_get_rrc_status(module_id, ctxt_pP->enb_flag, dst_id ) > 2 /*RRC_CONNECTED*/) { // not needed: this test is already done in update_otg_enb
      otg_pkt = (unsigned char*) (otg_pkt_info->otg_pkt).sdu_buffer;
      pkt_size = (otg_pkt_info->otg_pkt).sdu_buffer_size;

      if (otg_pkt != NULL) {
        if (is_ue == 0 ) {
          PROTOCOL_CTXT_SET_BY_MODULE_ID(
            &ctxt,
            src_id,
            ENB_FLAG_YES,
            oai_emulation.info.eNB_ue_module_id_to_rnti[ctxt.module_id][dst_id],
            ctxt_pP->frame,
            ctxt_pP->subframe,
	    src_id);

          LOG_D(OTG,"[eNB %d] Frame %d sending packet %d from module %d on rab id %d (src %d, dst %d/%x) pkt size %d for pdcp mode %d\n",
                ctxt.module_id,
                ctxt.frame,
                pkt_cnt_enb++,
                src_id,
                rb_id,
                src_id,
                dst_id,
		oai_emulation.info.eNB_ue_module_id_to_rnti[ctxt.module_id][dst_id],
                pkt_size,
                pdcp_mode);
          result = pdcp_data_req(&ctxt,
                                 SRB_FLAG_NO,
                                 rb_id,
                                 RLC_MUI_UNDEFINED,
                                 RLC_SDU_CONFIRM_NO,
                                 pkt_size,
                                 otg_pkt,
                                 pdcp_mode);
          if (result != TRUE) {
            LOG_W(OTG,"PDCP data request failed!\n");
          }
        } else {
          //rb_id= eNB_index * MAX_NUM_RB + DTCH;


          LOG_D(OTG,"[UE %d] Frame %d: sending packet %d from module %d on rab id %d (src %d/%x, dst %d) pkt size %d\n",
                ctxt_pP->module_id,
                ctxt_pP->frame,
                pkt_cnt_ue++,
                ctxt_pP->module_id,
                rb_id,
                src_id,
		pdcp_UE_UE_module_id_to_rnti[ctxt_pP->module_id], // [src_id]
                dst_id,
                pkt_size);
          PROTOCOL_CTXT_SET_BY_MODULE_ID(
            &ctxt,
            ctxt_pP->module_id, //src_id,
            ENB_FLAG_NO,
            pdcp_UE_UE_module_id_to_rnti[ctxt_pP->module_id],// [src_id]
            ctxt_pP->frame,
            ctxt_pP->subframe,
	    dst_id);

          result = pdcp_data_req( &ctxt,
                                  SRB_FLAG_NO,
                                  rb_id,
                                  RLC_MUI_UNDEFINED,
                                  RLC_SDU_CONFIRM_NO,
                                  pkt_size,
                                  otg_pkt,
                                  PDCP_TRANSMISSION_MODE_DATA);
          if (result != TRUE) {
            LOG_W(OTG,"PDCP data request failed!\n");
          }
        }

        free(otg_pkt);
        otg_pkt = NULL;
      }

      // } //else LOG_D(OTG,"ctxt_pP->frame %d enb %d-> ue %d link not yet established state %d  \n", ctxt_pP->frame, eNB_index,dst_id - NB_eNB_INST, mac_get_rrc_status(module_id, ctxt_pP->enb_flag, dst_id - NB_eNB_INST));

    }
  }

#else

  if ((otg_enabled==1) && (ctxt_pP->enb_flag == ENB_FLAG_YES)) { // generate DL traffic
    unsigned int ctime=0;
    ctime = ctxt_pP->frame * 100;

    /*if  ((mac_get_rrc_status(eNB_index, ctxt_pP->enb_flag, 0 ) > 2) &&
    (mac_get_rrc_status(eNB_index, ctxt_pP->enb_flag, 1 ) > 2)) { */
    PROTOCOL_CTXT_SET_BY_MODULE_ID(
      &ctxt,
      ctxt_pP->module_id,
      ctxt_pP->enb_flag,
      NOT_A_RNTI,
      ctxt_pP->frame,
      ctxt_pP->subframe,
      ctxt_pP->module_id);

    for (dst_id = 0; dst_id<NUMBER_OF_UE_MAX; dst_id++) {
      ctxt.rnti = oai_emulation.info.eNB_ue_module_id_to_rnti[ctxt.module_id][dst_id];

      if (ctxt.rnti != NOT_A_RNTI) {
//        if (mac_eNB_get_rrc_status(ctxt.module_id, ctxt.rnti ) > 2 /*RRC_SI_RECEIVED*/) {
    	  if (mac_eNB_get_rrc_status_send (ctxt.module_id, ctxt.rnti ) > 2 /*RRC_SI_RECEIVED*/) {
        unsigned int temp = 0;
          otg_pkt=packet_gen(
                    ENB_MODULE_ID_TO_INSTANCE(ctxt.module_id),
                    UE_MODULE_ID_TO_INSTANCE(dst_id),
                    0,
                    ctime,
                    &temp);
        pkt_size = temp;

        if (otg_pkt != NULL) {
          rb_id = dst_id * maxDRB + DTCH;
          pdcp_data_req(&ctxt,
                        SRB_FLAG_NO,
                        rb_id,
                        RLC_MUI_UNDEFINED,
                        RLC_SDU_CONFIRM_NO,
                        pkt_size,
                        otg_pkt,
                        PDCP_TRANSMISSION_MODE_DATA);
            LOG_I(OTG,
                  "send packet from module %d on rab id %d (src %d, dst %d) pkt size %d\n",
                  ctxt_pP->module_id, rb_id, ctxt_pP->module_id, dst_id, pkt_size);
            free(otg_pkt);
          }
        }
      }
    }
  }

#endif
}
