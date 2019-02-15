/*
 * pdcp_support.h
 *
 *  Created on: Oct 8, 2017
 *      Author: idefix
 */

#ifndef PDCP_SUPPORT_H_
#define PDCP_SUPPORT_H_

#define MODULE_ID 101010
#define TOTAL_PDCP_INSTANCE      2
#define SEC_TO_NANO_SECONDS  1000000000
#define NANO_TO_MICRO  0.001



//This macro enable all the measurements and initialize prepare reports
#define create_report
#undef create_report

//This macro writes detailed time measurement of different portion (ROHC, PDU, ENC) of PDCP.
//Be careful about def or undef this macro. Either define this or "mips_calc_report". not at the same time both.
#define detailed_timing_report
#undef detailed_timing_report

//This macro writes detailed time measurement of different portion (ROHC, PDU, ENC) of PDCP
#define mips_calc_report
#undef mips_calc_report

//This macro calculates total MIPS requirement by PDCP every second
#define mips_sum_report
#undef mips_sum_report

#define create_uplink_report
#undef create_uplink_report

#ifdef create_report
#define REPORT_NAME "pdcp_downlink_report_socket"
#else
#define REPORT_NAME "pdcp_uplink_without_rohc"
#endif

int total_connection;
long long int total_processed_bytes;
void execute_msgHandler (UINT32 messageId, INT32 sockFd);
void prepare_file_for_report ();


double rohc_time_downlink, sequencing_time_downlink, encrypt_time;
struct timespec rohc_start, rohc_end, seq_downlink_start, seq_downlink_end, encrypt_start, encrypt_end, packet_rec;

double rohc_time_uplink, sequencing_time_uplink, decrypt_time;
struct timespec rohc_uplink_start, rohc_uplink_end, seq_uplink_start, seq_uplink_end, decrypt_start, decrypt_end;

struct timespec refernce_time;


boolean_t pdcp_init_seq_numbers(pdcp_t* pdcp_entity);

void pdcp_layer_init(void);

rlc_op_status_t rlc_status_return (rlc_op_status_t val);

rlc_op_status_t rlc_status_resut(const protocol_ctxt_t* const ctxt_pP,
        const srb_flag_t   srb_flagP,
        const MBMS_flag_t  MBMS_flagP,
        const rb_id_t      rb_idP,
        const mui_t        muiP,
        confirm_t    confirmP,
        sdu_size_t   sdu_sizeP,
        mem_block_t *sdu_pP);


void pdcp_rrc_data_ind_send (
  const protocol_ctxt_t* const ctxt_pP,
  const rb_id_t                srb_idP,
  const sdu_size_t             sdu_sizeP,
  uint8_t              * const buffer_pP
);

int mac_eNB_get_rrc_status_send(
  const module_id_t   module_idP,
  const rnti_t  rntiP
);

/*typedef struct conn_info {
	int socID;					//Socket FD ID
	int bearerID;				//Bearer ID
	int pdcpInsID;				//Incase of multiple bearer handling, PDCP internal database ID
	int dataDirection;			//If 0 then dataflow uplink direction elseIF 1 downlink direction
} conn_info;*/

//Report writing deceleration
void update_rohc_db (double rohc_time, int dbIndex);
void update_pdu_db (double pdu_time, int dbIndex);
void update_enc_db (double enc_time, int dbIndex);
void pdcp_time_per_packet (double pdcp_time, int dbIndex);
void total_pdcp_time (double total_pdcp_time);
void create_pdcp_report ();
void update_connect_status (int fd);
void process_start_time_record (struct timespec start_time);
void mips_report (int totalPktNo, long long int totalPktSize);





#endif /* PDCP_SUPPORT_H_ */
