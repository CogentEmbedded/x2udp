/*******************************************************************************
 * iio2udp.h
 *
 * Daemon for converting IIO data to UDP packets.
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

#ifndef __IIO_2_UDP_H
#define __IIO_2_UDP_H

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdint.h>

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

#define IIO2UDP_DEFAULT_PORT 4857
#define IIO2UDP_PACKET_VERSION 1

/*******************************************************************************
 * Type declarations
 ******************************************************************************/

/* .. short data packet containing just data */
typedef
struct iio2udp_packet_short
{
    /* .. version of the data packet structure */
    uint8_t version;

    /* .. miscellaneous flags */
    uint8_t flags;

    /* .. quality of the data */
    uint16_t OPCQuality;

    /* .. id of the iio device on the host */
    uint16_t device_id;

    /* .. unique id of the channel within the device */
    uint16_t channel_id;

    union
    {
        /* .. value of the channel in uint64_t */
        uint64_t value_u64;

        /* .. value of the channel in double */
        double value_dbl;

    };

} __attribute__ ((packed)) iio2udp_packet_short_t;

COMPILE_TIME_ASSERT( sizeof(iio2udp_packet_short_t) == 16 )

/* .. long data packet containing debug information in addition to data  */
typedef
struct iio2udp_packet_long
{
    /* .. short packet comes first */
    iio2udp_packet_short_t data;

    /* .. short name of the iio device */
    uint8_t device_name[64 + 1];

    /* .. short name of the channel */
    uint8_t	channel_name[64 + 1];

    /* .. value of the channel in characters */
    uint8_t value_string[64 + 1];
} __attribute__ ((packed)) iio2udp_packet_long_t;

COMPILE_TIME_ASSERT( sizeof(iio2udp_packet_long_t) == sizeof(iio2udp_packet_short_t) + 64 + 1 + 64 + 1 + 64 + 1 )


#endif    /*  __IIO_2_UDP_H */
