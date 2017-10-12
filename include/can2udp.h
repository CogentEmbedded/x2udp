/*******************************************************************************
 * can2udp.h
 *
 * Daemon for converting SocketCAN packets to UDP packets.
 *
 * Copyright (c) 2015-2017 Cogent Embedded Inc. ALL RIGHTS RESERVED.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************/

#ifndef __CAN_2_UDP_H
#define __CAN_2_UDP_H

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdint.h>
#include <linux/can.h>

/*******************************************************************************
 * Static Asserts
 ******************************************************************************/

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(!!(COND))*2-1];
#define COMPILE_TIME_ASSERT3(X,L) STATIC_ASSERT(X,static_assertion_at_line_##L)
#define COMPILE_TIME_ASSERT2(X,L) COMPILE_TIME_ASSERT3(X,L)
#define COMPILE_TIME_ASSERT(X)    COMPILE_TIME_ASSERT2(X,__LINE__)

/*******************************************************************************
 * Settings
 ******************************************************************************/

#define CAN2UDP_DEFAULT_PORT 4858
#define CAN2UDP_PACKET_VERSION 2
#define CAN2UDP_TIMEOUT (1 << 0)

/*******************************************************************************
 * Type declarations
 ******************************************************************************/

/* .. data packet containing full SocketCAN frame */
typedef
struct can2udp_packet_ver1
{
    /* .. version of the data packet structure */
    uint8_t version;

    /* .. miscellaneous flags */
    uint8_t flags;

    /* .. id of the can interface the host */
    uint16_t interface_id;

    /* .. SocketCAN frame */
    struct can_frame raw_frame;

} __attribute__ ((packed)) can2udp_packet_ver1_t;

COMPILE_TIME_ASSERT( sizeof(can2udp_packet_ver1_t) == 20 )

/* .. data packet containing full SocketCAN CAN FD frame */
typedef
struct can2udp_packet
{
    /* .. version of the data packet structure */
    uint8_t version;

    /* .. miscellaneous flags */
    uint8_t flags;

    /* .. id of the can interface the host */
    uint16_t interface_id;

    /* .. SocketCAN frame */
    struct canfd_frame raw_frame;

    /*.. timestamp in nanoseconds. Zero if not available. */
    uint64_t timestamp;

} __attribute__ ((packed)) can2udp_packet_t;

COMPILE_TIME_ASSERT( sizeof(can2udp_packet_t) == 84 )

#endif    /*  __CAN_2_UDP_H */
