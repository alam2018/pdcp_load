/*
 * pdcp_rohc_decomp.c
 *
 *  Created on: Aug 27, 2018
 *      Author: user
 */

/*
 * pdcp_rohc.c
 *
 *  Created on: Jan 30, 2018
 *      Author: idefix
 */


/* system includes */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

#include "socket_msg.h"
#include "pdcp_rohc.h"
#include "pdcp_support.h"

//#include "config.h" /* for HAVE_*_H definitions */

#if HAVE_WINSOCK2_H == 1
#  include <winsock2.h> /* for htons() on Windows */
#endif
#if HAVE_ARPA_INET_H == 1
#  include <arpa/inet.h> /* for htons() on Linux */
#endif

/* includes required to use the compression part of the ROHC library */
#include <rohc/rohc.h>
#include <rohc/rohc_comp.h>

/** The size (in bytes) of the buffers used in the program */
#define BUFFER_SIZE_ROHC 2048

/** The payload for the fake IP packet */
#define FAKE_PAYLOAD "hello, ROHC world!"


#ifdef ROHC_COMPRESSION
//extern PDCP_DATA_REQ_FUNC_T 			pdcpDataReqFuncMsg;
extern PDCP_DATA_IND_T 				pdcpDataIndMsg;


struct rohc_decomp * create_decompressor(void)
	__attribute__((warn_unused_result));

//static void create_packet(struct rohc_buf *const packet);
static bool rohc_decompression(struct rohc_decomp *const decompressor, const struct rohc_buf comp_packet)
	__attribute__((warn_unused_result, nonnull(1)));

/* user-defined function callbacks for the ROHC library */
static void print_rohc_decomptraces(void *const priv_ctxt,
                              const rohc_trace_level_t level,
                              const rohc_trace_entity_t entity,
                              const int profile,
                              const char *const format,
                              ...)
	__attribute__((format(printf, 5, 6), nonnull(5)));
static int gen_random_num(const struct rohc_comp *const comp,
                          void *const user_context)
	__attribute__((warn_unused_result));
static bool rtp_detect(const unsigned char *const ip,
                       const unsigned char *const udp,
                       const unsigned char *payload,
                       const unsigned int payload_size,
                       void *const rtp_private)
	__attribute__((warn_unused_result));



static void print_rohc_decomptraces(void *const priv_ctxt,
                              const rohc_trace_level_t level,
                              const rohc_trace_entity_t entity,
                              const int profile,
                              const char *const format,
                              ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}


/**
 * @brief Dump the given network packet on standard output
 *
 * @param packet  The packet to dump
 */
static void dump_packet(const struct rohc_buf packet)
{
	size_t i;

	for(i = 0; i < packet.len; i++)
	{
		printf("0x%02x ", rohc_buf_byte_at(packet, i));
		if(i != 0 && ((i + 1) % 8) == 0)
		{
			printf("\n");
		}
	}
	if(i != 0 && ((i + 1) % 8) != 0) /* be sure to go to the line */
	{
		printf("\n");
	}
}



/**
 * @brief The main entry point for the RTP detection program
 *
 * @param argc  The number of arguments given to the program
 * @param argv  The table of arguments given to the program
 * @return      0 in case of success, 1 otherwise
 */

//struct rohc_comp *compressor[10];  /* the ROHC compressor */
uint8_t ip_buffer[ROHC_BUFFER_SIZE];

int rohc_decompmain(struct rohc_decomp *const decompressor)
{

	/* the buffer that will contain the IPv4 packet to compress */
	memset (&ip_buffer, 0, ROHC_BUFFER_SIZE);
	struct rohc_buf ip_packet = rohc_buf_init_empty(ip_buffer, ROHC_BUFFER_SIZE);
#ifndef create_report
//! [define random callback 1]
	unsigned int seed;

	/* initialize the random generator */
	seed = time(NULL);
	srand(seed);
#endif

	ip_packet.len = pdcpDataIndMsg.rohc_packet.len;
	ip_packet.time.sec = pdcpDataIndMsg.rohc_packet.time.sec;
	ip_packet.time.nsec = pdcpDataIndMsg.rohc_packet.time.nsec;
	ip_packet.max_len = pdcpDataIndMsg.rohc_packet.max_len;
	ip_packet.offset = pdcpDataIndMsg.rohc_packet.offset;

	memcpy (ip_packet.data, &pdcpDataIndMsg.rohc_packet.ipData, ip_packet.len);

	/* compress the RTP packet with a user-defined callback to detect the
	 * RTP packets among the UDP packets */

	if(!rohc_decompression(decompressor, ip_packet))
	{
		fprintf(stderr, "compression with detection by UDP ports failed\n");
		goto release_compressor;
	}

	/* release the ROHC compressor when you do not need it anymore */
//	printf("\n\ndestroy the ROHC decompressor\n");
//	rohc_comp_free(compressor);

	return 0;

release_compressor:
	rohc_comp_free(compressor);
error:
	fprintf(stderr, "an error occurred during program execution, "
	        "abort program\n");
	return 1;
}

/**
 * @brief Create and configure one ROHC compressor
 *
 * @return  The created and configured ROHC compressor
 */

struct rohc_decomp * create_decompressor()
{
	struct rohc_decomp *decompressor;

	/* create the ROHC compressor */
//	decompressor = rohc_decomp_new2(ROHC_LARGE_CID, 4, ROHC_U_MODE);
	decompressor = rohc_decomp_new2(ROHC_SMALL_CID, 4, ROHC_U_MODE);
	if(decompressor == NULL)
	{
		fprintf(stderr, "failed create the ROHC compressor\n");
		goto error;
	}

#ifndef create_uplink_report
//	 set the callback for traces on compressor
//! [set compression traces callback]
/*	if(!rohc_decomp_set_traces_cb2(decompressor, print_rohc_decomptraces, NULL))
	{
		fprintf(stderr, "failed to set the callback for traces on "
		        "compressor\n");
		goto release_compressor;
	}*/
//! [set compression traces callback]
#endif

	/* Enable the decompression profiles you need */
	printf("\nenable several ROHC decompression profiles\n");
//! [enable ROHC decompression profile]
/*	if(!rohc_decomp_enable_profile(decompressor, ROHC_PROFILE_UNCOMPRESSED))
	{
		fprintf(stderr, "failed to enable the Uncompressed profile\n");

	}
	if(!rohc_decomp_enable_profile(decompressor, ROHC_PROFILE_IP))
	{
		fprintf(stderr, "failed to enable the IP-only profile\n");

	}*/
//! [enable ROHC decompression profile]
//! [enable ROHC decompression profiles]
	if(!rohc_decomp_enable_profiles(decompressor, ROHC_PROFILE_UDP,
            ROHC_PROFILE_UDPLITE, ROHC_PROFILE_IP, ROHC_PROFILE_RTP, -1))
	{
		fprintf(stderr, "failed to enable the IP/UDP and IP/UDP-Lite "
		        "profiles\n");

	}
//! [enable ROHC decompression profiles]
	return decompressor;

release_compressor:
	rohc_comp_free(decompressor);
error:
	return NULL;
}

/**
 * @brief Compress one IP/UDP/RTP packet
 *
 * @param compressor     The ROHC compressor
 * @param uncomp_packet  The IP/UDP/RTP packet to compress
 * @return               true if the compression is successful,
 *                       false if the compression failed
 */
static bool rohc_decompression(struct rohc_decomp *const decompressor, const struct rohc_buf comp_packet)
{
//	uint8_t rohc_buffer[SDU_BUFFER_SIZE];
//	struct rohc_buf rohc_packet = rohc_buf_init_empty(rohc_buffer, SDU_BUFFER_SIZE);
	rohc_status_t status;

	/* the buffer that will contain the resulting IP packet */
		unsigned char ip_buffer[ROHC_BUFFER_SIZE];
		struct rohc_buf ip_packet = rohc_buf_init_empty(ip_buffer, ROHC_BUFFER_SIZE);
		/* we do not want to handle feedback in this simple example */
//		struct rohc_buf *rcvd_feedback = NULL;
//		struct rohc_buf *feedback_send = NULL;

		/* structures to handle ROHC feedback */
		unsigned char rcvd_feedback_buffer_d[ROHC_BUFFER_SIZE];	// the buffer that will contain the ROHC feedback packet received
		struct rohc_buf rcvd_feedback = rohc_buf_init_empty(rcvd_feedback_buffer_d, ROHC_BUFFER_SIZE);

		unsigned char feedback_send_buffer_d[ROHC_BUFFER_SIZE];	// the buffer that will contain the ROHC feedback packet to be sent
		struct rohc_buf feedback_send = rohc_buf_init_empty(feedback_send_buffer_d, ROHC_BUFFER_SIZE);

		rohc_buf_reset (&rcvd_feedback);
		rohc_buf_reset (&feedback_send);
		rohc_buf_reset (&ip_packet);


	/* then, compress the fake IP/UDP/RTP packet with the RTP profile */
#ifndef create_report
	printf("\ndecompress the IP/UDP/RTP packet\n");
#endif

#ifdef create_uplink_report
			clock_gettime(CLOCK_MONOTONIC, &rohc_uplink_start);
#endif
	status = rohc_decompress3(decompressor, comp_packet, &ip_packet, &rcvd_feedback, &feedback_send);

#ifdef create_uplink_report
			clock_gettime(CLOCK_MONOTONIC, &rohc_uplink_end);
			rohc_time_uplink = (double)(rohc_uplink_end.tv_sec - rohc_uplink_start.tv_sec)*SEC_TO_NANO_SECONDS
					+ (double)(rohc_uplink_end.tv_nsec - rohc_uplink_start.tv_nsec);
#endif

	//! [decompress ROHC packet #1]
		printf("\n");
	//! [decompress ROHC packet #2]
		if(status == ROHC_STATUS_OK)
		{
			/* decompression is successful */
			if(!rohc_buf_is_empty(ip_packet))
			{
				/* ip_packet.len bytes of decompressed IP data available in
				 * ip_packet: dump the IP packet on the standard output */
				memset (pdcpDataIndMsg.rohc_packet.ipData, 0, ip_packet.len);
				memcpy (pdcpDataIndMsg.rohc_packet.ipData, ip_packet.data, ip_packet.len);
				pdcpDataIndMsg.rohc_packet.len = ip_packet.len;
//				printf("IP packet resulting from the ROHC decompression:\n");
//				dump_packet(ip_packet);
			}
			else
			{
				/* no IP packet was decompressed because of ROHC segmentation or
				 * feedback-only packet:
				 *  - the ROHC packet was a non-final segment, so at least another
				 *    ROHC segment is required to be able to decompress the full
				 *    ROHC packet
				 *  - the ROHC packet was a feedback-only packet, it contained only
				 *    feedback information, so there was nothing to decompress */
				printf("no IP packet decompressed");
			}
		}
		else
		{
			/* failure: decompressor failed to decompress the ROHC packet */
			fprintf(stderr, "decompression of fake ROHC packet failed\n");
		}
	//! [decompress ROHC packet #3]

	return true;

error:
	return false;
}

//! [define random callback 2]
/**
 * @brief Generate a random number
 *
 * @param comp          The ROHC compressor
 * @param user_context  Should always be NULL
 * @return              A random number
 */
static int gen_random_num(const struct rohc_comp *const comp,
                          void *const user_context)
{
	return rand();
}

//! [define compression traces callback]
/**
 * @brief Callback to print traces of the ROHC library
 *
 * @param priv_ctxt  An optional private context, may be NULL
 * @param level      The priority level of the trace
 * @param entity     The entity that emitted the trace among:
 *                    \li ROHC_TRACE_COMP
 *                    \li ROHC_TRACE_DECOMP
 * @param profile    The ID of the ROHC compression/decompression profile
 *                   the trace is related to
 * @param format     The format string of the trace
 */
static void print_rohc_traces(void *const priv_ctxt,
                              const rohc_trace_level_t level,
                              const rohc_trace_entity_t entity,
                              const int profile,
                              const char *const format,
                              ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}
//! [define compression traces callback]
#endif

