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
extern PDCP_DATA_REQ_FUNC_T 			pdcpDataReqFuncMsg;


struct rohc_comp * create_compressor(void)
	__attribute__((warn_unused_result));

static void create_packet(struct rohc_buf *const packet);

static bool rohc_compression (struct rohc_comp *const compressor,
                                   const struct rohc_buf packet)
	__attribute__((warn_unused_result, nonnull(1)));

/* user-defined function callbacks for the ROHC library */
static void print_rohc_traces(void *const priv_ctxt,
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



/**
 * @brief The main entry point for the RTP detection program
 *
 * @param argc  The number of arguments given to the program
 * @param argv  The table of arguments given to the program
 * @return      0 in case of success, 1 otherwise
 */

//struct rohc_comp *compressor[10];  /* the ROHC compressor */


int rohc_main(struct rohc_comp *const compressor)
{
	uint8_t ip_buffer[ROHC_BUFFER_SIZE];
//	struct rohc_comp *compressor;  /* the ROHC compressor */

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

/*	 create a ROHC compressor with small CIDs and the largest MAX_CID
	 * possible for small CIDs
	printf("\ncreate the ROHC compressor\n");
//	compressor = create_compressor();
	if(compressor == NULL)
	{
		fprintf(stderr, "failed create the ROHC compressor\n");
		goto error;
	}*/

	ip_packet.len = pdcpDataReqFuncMsg.rohc_packet.len;
	ip_packet.time.sec = pdcpDataReqFuncMsg.rohc_packet.time.sec;
	ip_packet.time.nsec = pdcpDataReqFuncMsg.rohc_packet.time.nsec;
	ip_packet.max_len = pdcpDataReqFuncMsg.rohc_packet.max_len;
	ip_packet.offset = pdcpDataReqFuncMsg.rohc_packet.offset;

//	memcpy (ip_packet.data, &pdcpDataReqFuncMsg.rohc_packet.data, ip_packet.len);
	memcpy (ip_packet.data, &pdcpDataReqFuncMsg.rohc_packet.ipData, ip_packet.len);

	/* compress the RTP packet with a user-defined callback to detect the
	 * RTP packets among the UDP packets */
#ifdef create_report
			clock_gettime(CLOCK_MONOTONIC, &rohc_start);
#endif

	if(!rohc_compression(compressor, ip_packet))
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
struct rohc_comp * create_compressor()
{
	struct rohc_comp *compressor;

	/* create the ROHC compressor */
	compressor = rohc_comp_new2(ROHC_SMALL_CID, ROHC_SMALL_CID_MAX,
	                            gen_random_num, NULL);
	if(compressor == NULL)
	{
		fprintf(stderr, "failed create the ROHC compressor\n");
		goto error;
	}

#ifndef create_report
//	 set the callback for traces on compressor
//! [set compression traces callback]
/*	if(!rohc_comp_set_traces_cb2(compressor, print_rohc_traces, NULL))
	{
		fprintf(stderr, "failed to set the callback for traces on "
		        "compressor\n");
		goto release_compressor;
	}*/
//! [set compression traces callback]
#endif

	/* enable the RTP compression profile */
	printf("\nenable the ROHC UDP compression profile\n");
	if(!rohc_comp_enable_profiles(compressor, ROHC_PROFILE_UDP,
	                              ROHC_PROFILE_UDPLITE, ROHC_PROFILE_IP, ROHC_PROFILE_RTP, -1))
	{
		fprintf(stderr, "failed to enable the IP/UDP profile\n");
		goto release_compressor;
	}

	return compressor;

release_compressor:
	rohc_comp_free(compressor);
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
static bool rohc_compression(struct rohc_comp *const compressor,
                                   const struct rohc_buf uncomp_packet)
{
	uint8_t rohc_buffer[ROHC_BUFFER_SIZE];
	struct rohc_buf rohc_packet = rohc_buf_init_empty(rohc_buffer, ROHC_BUFFER_SIZE);
	rohc_status_t status;


	/* then, compress the fake IP/UDP/RTP packet with the RTP profile */
#ifndef create_report
	printf("\ncompress the fake IP/UDP/RTP packet\n");
#endif
	status = rohc_compress4(compressor, uncomp_packet, &rohc_packet);

#ifdef create_report
			clock_gettime(CLOCK_MONOTONIC, &rohc_end);
#endif

	if(status == ROHC_STATUS_SEGMENT)
	{
		fprintf(stderr, "unexpected ROHC segment\n");
		goto error;
	}
	else if(status == ROHC_STATUS_OK)
	{
/*		pdcpDataReqFuncMsg.rohc_packet.len = uncomp_packet.len;
		memset (pdcpDataReqFuncMsg.rohc_packet.data, 0, uncomp_packet.len);
		memcpy (pdcpDataReqFuncMsg.rohc_packet.data, rohc_packet.data, rohc_packet.len);
//		pdcpDataReqFuncMsg.rohc_packet.len = rohc_packet.len;
		pdcpDataReqFuncMsg.rohc_packet.offset = uncomp_packet.offset;*/

		pdcpDataReqFuncMsg.rohc_packet.len = rohc_packet.len;
//		memset (pdcpDataReqFuncMsg.rohc_packet.ipData, 0, ROHC_BUFFER_SIZE);
//		memcpy (pdcpDataReqFuncMsg.rohc_packet.ipData, rohc_packet.data, ROHC_BUFFER_SIZE);
		memset (pdcpDataReqFuncMsg.rohc_packet.ipData, 0, rohc_packet.len);
		memcpy (pdcpDataReqFuncMsg.rohc_packet.ipData, rohc_packet.data, rohc_packet.len);
//		pdcpDataReqFuncMsg.rohc_packet.len = rohc_packet.len;
		pdcpDataReqFuncMsg.rohc_packet.offset = rohc_packet.offset;
#ifndef create_report
		printf("\nIP/UDP/RTP packet successfully compressed\n");
#endif
	}
	else
	{
		/* compressor failed to compress the IP packet */
		fprintf(stderr, "compression of fake IP/UDP/RTP packet failed\n");
		goto error;
	}

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
