# x2udp
CAN and IIO network streaming utilities

 - can2udp - Streams CAN and CAN FD packets in UDP
 - iio2udp  - Streams data from IIO in UDP

## How to build
 1. Prepare dependencies

  The project was tested with
   - libdaemon version 0.14
   - libconfig version  1.5
   - libiio version 0.7
   - Linux Kernel 4.0

 Any recent and fresh version of dependencies are expected to work fine.

 1. Download sources
> git clone https://github.com/CogentEmbedded/x2udp.git

 1. Configure
> cd x2udp
> mkdir build
> cd build
> cmake ..

 1. Compile & Install
> make
> make install

## Limitations of the Current Version

  - Only system V configuration is supported, systemd support to be implemented
  - CAN RTR and ERROR frames are not handled

## UDP packet format
Packet format definitions can be found in header files under the include directory.
 1. can2udp version 2
```c
#define CAN2UDP_DEFAULT_PORT 4858
#define CAN2UDP_PACKET_VERSION 2
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
```

 2. iio2udp version 1
```c
#define IIO2UDP_DEFAULT_PORT 4857
#define IIO2UDP_PACKET_VERSION 1
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
```


