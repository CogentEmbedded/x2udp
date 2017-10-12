/*******************************************************************************
 * can2udp.c
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

/*
 * Includes
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

#include <libdaemon/daemon.h>
#include <libconfig.h>

#include "can2udp.h"
#include <linux/can.h>
#include <linux/can/raw.h>

/*
 * Settings
 */

#define CAN2UDP_DEFAULT_CONFIG_FILENAME "/etc/can2udp"

/*
 * Type declarations
 */

typedef
struct channel channel_t;

struct channel
{
    /* .. SocketCAN interface name */
    const char *interface_name;

    /* .. device index for UDP */
    int udp_interface_index;

    /* .. socket for CAN */
    int raw_socket;

    /* .. array of filtered IDs */
    canid_t *filters;

    /* .. length of filters */
    size_t filters_length;

    /*.. interface supports CAN FD */
    int can_fd_enabled;

    /* .. pointer to the next element in the list */
    channel_t *next;
};

typedef
struct deamon_config
{
    /* .. Single linked list of channels */
    channel_t *channels;

    /* .. UDP port number we transmit to */
    int port;

    /* .. network interface to bind UDP socket to */
    const char *interface;

    /* .. broadcast UDP socket */
    int socket_broadcast;

    /* .. broadcast IP address */
    struct sockaddr_in baddr;
} daemon_config_t;

/*******************************************************************************
 * Parsing of config file.
 * See default cofnfig for file format.
 ******************************************************************************/

int
parse_config(daemon_config_t *config, const char *config_file_name)
{
    config_t cf;
    int i, j;

    /* .. set default values for parameters */
    config->channels = NULL;
    config->port = CAN2UDP_DEFAULT_PORT;
    config->interface = NULL;

    config_init(&cf);

    /* set default name if none is provided */
    if (!config_file_name)
        config_file_name = CAN2UDP_DEFAULT_CONFIG_FILENAME;

    /* .. try reading config file */
    if (config_read_file(&cf, config_file_name) != CONFIG_TRUE)
    {
        daemon_log(LOG_ERR, "Error parsing config file '%s' %s:%d - %s\n",
                   config_file_name,
                   config_error_file(&cf),
                   config_error_line(&cf),
                   config_error_text(&cf));

        /* .. release and exit  */
        config_destroy(&cf);
        return -1;
    }

    config_lookup_int(&cf, "port", &config->port);

    config_lookup_string(&cf, "interface", &config->interface);
    if (config->interface)
        config->interface = strdup(config->interface);

    /* .. try getting channel configurations */
    const config_setting_t *channels  = config_lookup(&cf, "interfaces");
    if (channels)
    {
        int count = config_setting_length(channels);
        channel_t *chc = NULL;

        for (i = 0; i < count; i++)
        {
            if (chc)
            {
                /* .. allocate memory for new element and jump to it */
                chc->next = malloc(sizeof(*chc));
                chc = chc->next;
            }
            else
            {
                /* .. allocate memory for the first element and store it */
                chc = malloc(sizeof(*chc));
                config->channels = chc;
            }

            if (!chc)
            {
                daemon_log(LOG_ERR, "Out of memory");

                config_destroy(&cf);
                return -1;
            }

            /* .. parse config for the channel */
            config_setting_t *channel = config_setting_get_elem (channels, i);
            if (channel)
            {
                /* .. set default values */
                chc->next = NULL;
                chc->interface_name = "vcan0";
                chc->udp_interface_index = i;
                chc->raw_socket = 0;
                chc->filters = NULL;
                chc->filters_length = 0;
                chc->can_fd_enabled = 1;

                /* .. try reading channel settings
                 *    We copy strings here because they get destroyed together with cf,
                 *    and we don't really hold the reference to cf any longer.
                 */
                config_setting_lookup_string(channel, "name", &chc->interface_name);
                chc->interface_name = strdup(chc->interface_name);
                config_setting_lookup_int(channel, "interface_index", &chc->udp_interface_index);
                config_lookup_bool(&cf, "can_fd", &chc->can_fd_enabled);

                /* .. try parsing message filter */
                config_setting_t *filter = config_setting_get_member(channel, "filter");
                if (filter)
                {
                    /* .. ignore invalid lengths */
                    ssize_t len = config_setting_length(filter);
                    if (len >= 0)
                    {
                        /* .. allocate memory */
                        chc->filters = malloc(sizeof(*chc->filters) * len);
                        chc->filters_length = len;

                        if (!chc->filters)
                        {
                            daemon_log(LOG_ERR, "Out of memory");

                            config_destroy(&cf);
                            return -1;
                        }

                        /* .. read all elements of the filter */
                        for (j = 0; j < len; j ++)
                        {
                            config_setting_t *element =  config_setting_get_elem(filter, j);
                            if (element)
                                chc->filters[j] = config_setting_get_int (element);
                        }
                    }
                }
            }
        }
    }

    /* .. release */
    config_destroy(&cf);

    return 0;
}

/*
 * SocketCAN channel handling
 */
int channel_init(channel_t *chc, fd_set *fds)
{
    struct ifreq ifr;
    struct sockaddr_can addr;
    size_t j;
    int use_canfd = 1;

    /* .. create the socket */
    if ((chc->raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        daemon_log(LOG_WARNING, "CAN socket error: %m");
        goto error;
    }

    /* .. obtain CAN channel index */
    strcpy(ifr.ifr_name, chc->interface_name);
    if (ioctl(chc->raw_socket, SIOCGIFINDEX, &ifr) < 0)
    {
        daemon_log(LOG_WARNING, "CAN socket name set failed for '%s': %m", chc->interface_name);
        goto error;
    }

    /*.. set non-blocking */
    if (fcntl(chc->raw_socket, F_SETFL, O_NONBLOCK)< 0)
    {
        daemon_log(LOG_WARNING, "Error setting nonblock for CAN socket '%s'. Ignoring: %m", chc->interface_name);
    }

    if (chc->filters)
    {
        size_t len = chc->filters_length;
        if (len > CAN_RAW_FILTER_MAX)
        {
            daemon_log(LOG_WARNING, "Limiting the number of filters to %d for CAN socket '%s'. Ignoring: %m", CAN_RAW_FILTER_MAX, chc->interface_name);
            len = CAN_RAW_FILTER_MAX;
        }

        struct can_filter *rfilter = malloc(sizeof(struct can_filter) * len);

        /*.. filter messages only in case they are needed */
        for (j = 0; j < len; j++)
        {
            rfilter[j].can_mask = chc->filters[j] > CAN_SFF_MASK ? CAN_EFF_MASK : CAN_EFF_MASK;
            rfilter[j].can_id = chc->filters[j];
        }

        if (setsockopt(chc->raw_socket, SOL_CAN_RAW, CAN_RAW_FILTER, rfilter, sizeof(struct can_filter) * len) < 0)
        {
            daemon_log(LOG_WARNING, "Error setting filters for CAN socket '%s'. Ignoring: %m", chc->interface_name);
        }

        free(rfilter);
    }

    /* .. connect the socket to the channel */
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(chc->raw_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        daemon_log(LOG_WARNING, "CAN socket bind failed for '%s': %m", chc->interface_name);

        goto error;
    }

    /*.. try to enable CAN FD. Ignore errors. */
    if (setsockopt(chc->raw_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                   &use_canfd, sizeof(use_canfd)) < 0)
    {
        daemon_log(LOG_WARNING, "Error enabling CAN FD frames for CAN socket '%s'. Ignoring: %m", chc->interface_name);
        chc->can_fd_enabled = 0;
    }

    /* .. add fd to the list for select() call */
    FD_SET(chc->raw_socket, fds);

    return 0;

error:
    if (chc->raw_socket)
        close(chc->raw_socket);
    chc->raw_socket = 0;

    return -1;
}

int channel_send_frame(daemon_config_t *config, channel_t *chc, struct canfd_frame *frame, unsigned long timestamp)
{
    /* .. init default data */
    can2udp_packet_t packet = {
        .version = CAN2UDP_PACKET_VERSION,
        .flags = 0,
        .interface_id = (uint16_t)chc->udp_interface_index,
        .timestamp = timestamp
    };

    /* copy CAN packet */
    if (frame)
        memcpy(&packet.raw_frame, frame, sizeof(*frame));

    /* .. send the packet out */
    int err;
    if ((err = sendto(config->socket_broadcast,
                      &packet, sizeof(packet),
                      0,
                      (struct sockaddr *)&config->baddr, sizeof(config->baddr))
         != sizeof(packet)))
        daemon_log(LOG_WARNING, "Error sending data to UDP socket. Data loss occured. %d, %m", err);

    return 0;
}

unsigned long tiemval_to_ns(struct timeval tv)
{
    return ((tv.tv_sec * 1000000ul + tv.tv_usec) * 1000ul);
}

static unsigned long pkt_count = 0;

int channel_process(daemon_config_t *config, channel_t *chc)
{
    /*.. FIXME the size of the array can be used for tuning performance */
    struct canfd_frame frame;
    int ret = 0;

    /* .. try reading new BCM message */
    ssize_t nbytes = read(chc->raw_socket, &frame, chc->can_fd_enabled ? CANFD_MTU : CAN_MTU);
    if ((nbytes == 0) || (nbytes == -EINTR))
        return -EINTR;
    else if ( nbytes != CAN_MTU && nbytes != CANFD_MTU )
    {
        daemon_log(LOG_WARNING, "Error reading data from RAW socket for '%s'. Unexpected size %zd.", chc->interface_name, nbytes);
        return -EINVAL;
    }

    struct timeval tv;
    if (ioctl(chc->raw_socket, SIOCGSTAMP, &tv) < 0)
    {
        daemon_log(LOG_DEBUG, "Error reading timestamp from RAW socket for '%s'.", chc->interface_name);
        memset(&tv, 0, sizeof(tv));
    }

    /* .. process all received messages */
    ret = channel_send_frame(config, chc, &frame, tiemval_to_ns(tv));

    daemon_log(LOG_DEBUG, "processed %lu packets", ++pkt_count);

    return ret;
}

int channel_is_ready(channel_t *chc, fd_set *fds)
{
    return FD_ISSET(chc->raw_socket, fds);
}

int channel_close(channel_t *chc, fd_set *fds)
{
    /* .. add fd to the list for select() call */
    FD_CLR(chc->raw_socket, fds);

    /* close and destroy CAN Socket */
    if (close(chc->raw_socket) < 0)
    {
        daemon_log(LOG_ERR, "Error closing socket for '%s'. %m.", chc->interface_name);
        return -1;
    }
    chc->raw_socket = 0;

    /* .. free strings */
    free((void *)chc->interface_name);
    chc->interface_name = NULL;

    return 0;
}

/*
 * Socket
 */

int socket_init(daemon_config_t *config)
{
    const int yes = 1;

    if((config->socket_broadcast = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        daemon_log(LOG_ERR, "Error creating UDP socket %m.");
        return -1;
    }

    /* .. make the socket broadcast */
    if (setsockopt(config->socket_broadcast, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0)
    {
        daemon_log(LOG_ERR, "Error setting UDP socket broadcast. %m");
        return -1;
    }

    /* .. bind the socket to an interface if required */
    if (config->interface)
        if (setsockopt(config->socket_broadcast, SOL_SOCKET, SO_BINDTODEVICE, config->interface, strlen(config->interface)) < 0)
        {
            daemon_log(LOG_WARNING, "Cannot bind UDP socket to '%s'. Packets will be sent on all interfaces. %m", config->interface);
        }

    /* .. initialize broadcast address */
    bzero(&config->baddr, sizeof(config->baddr));
    config->baddr.sin_family = AF_INET;
    config->baddr.sin_port = htons(config->port);
    config->baddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    return 0;
}

int socket_close(daemon_config_t *config)
{
    /* close and destroy the socket */
    if (close(config->socket_broadcast) < 0)
    {
        daemon_log(LOG_ERR, "Error closing UDP socket fd. %m.");
        return -1;
    }
    config->socket_broadcast = 0;

    return 0;
}

/*
 * System integration functions.
 */

int system_init(daemon_config_t *config, fd_set *fds, const char* config_file_name)
{
    if (parse_config(config, config_file_name) < 0)
        return -1;

    /* .. init all SocketCAN channels */
    int good_channels = 0;
    channel_t *chc = config->channels;

    while (chc)
    {
        /* .. try to initialize channel */
        if (channel_init(chc, fds) == 0)
            good_channels++;

        /* .. go to the next item in the list */
        chc = chc->next;
    }

    daemon_log(LOG_INFO, "Initialized %d good channels.", good_channels);

    if (!good_channels)
    {
        daemon_log(LOG_ERR, "No channels to work with. Exit.");
        return -ENODATA;
    }

    /* .. init UDP socket for broadcasting */
    socket_init(config);

    return 0;
}

int system_check_channels_and_process(daemon_config_t *config, fd_set *fds)
{
    /* .. init all channels */
    int procesed_channels = 0;
    channel_t *chc = config->channels;

    /* .. loop through all elements */
    while (chc)
    {
        /* .. try to initialize channel */
        if (channel_is_ready(chc, fds))
        {
            int err;
            if ((err = channel_process(config, chc)) != 0)
                daemon_log(LOG_WARNING, "Error processing channel '%s'. Error %d", chc->interface_name, err);

            procesed_channels ++;
        }

        /* .. go to the next item in the list */
        chc = chc->next;
    }

    return 0;
}

void system_close(daemon_config_t *config, fd_set *fds)
{
    /* .. close the socket first */
    socket_close(config);

    /* .. loop through all channels */
    channel_t *chc = config->channels;
    while (chc)
    {
        /* .. try to close the channel */
        if (channel_close(chc, fds) != 0)
            daemon_log(LOG_WARNING, "Error closing channel '%s'", chc->interface_name);

        /* .. free this element and go to the next in list */
        channel_t *next = chc->next;
        free(chc);
        chc = next;
    }

    /* .. free strings */
    if (config->interface)
    {
        free((void *)config->interface);
        config->interface = NULL;
    }
}

/*
 * Main daemon routines
 */

#define run_or_return(fun, retval) \
    do { int rv = fun; \
    if (rv < 0) \
{ daemon_log(LOG_ERR, #fun " failed (%d): %s", rv, strerror(errno)); \
    return retval; } \
    } while (0)

#define run_or_retval(fun, retval) \
    do { int rv = fun; \
    if (rv < 0) \
{ daemon_log(LOG_ERR, #fun " failed (%d): %s", rv, strerror(errno)); \
    if (run_daemon && retval != 0) daemon_retval_send(retval); \
    goto finish; } \
    } while (0)

int main(int argc, char **argv)
{
    pid_t pid;
    int run_daemon = 1;
    int verbosity = 0;
    fd_set fds;
    int quit = 0;
    daemon_config_t config;
    const char* config_file_name = CAN2UDP_DEFAULT_CONFIG_FILENAME;

    /*.. use damon name from command line for both syslog and PID file */
    daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);

    int c;
    while ((c = getopt (argc, argv, "kDtvc:")) != -1)
        switch (c)
        {
        case 'k':
            run_or_return(daemon_pid_file_kill_wait(SIGTERM, 5), 4);
            return 0;

        case 'D':
            run_daemon = 1;
            daemon_log_use = DAEMON_LOG_AUTO;
            break;

        case 't':
            run_daemon = 0;
            daemon_log_use = DAEMON_LOG_STDERR;
            break;

        case 'v':
            verbosity = 1;
            daemon_set_verbosity(verbosity ? 7 : 3);
            break;

        case 'c':
            config_file_name = optarg;
            break;

        case '?':
            if (optopt == 'c')
                daemon_log(LOG_ERR, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                daemon_log(LOG_ERR, "Unsupported comand line option provided: %c", optopt);
            return 1;

        default:
            abort();
        }

    /*.. fix signals for the daemon */
    run_or_return(daemon_reset_sigs(-1), 2);
    run_or_return(daemon_unblock_sigs(-1), 3);

    if ((pid = daemon_pid_file_is_running()) >= 0)
    {
        daemon_log(LOG_ERR, "Already running, pid=%u", pid);
        return 1;
    }

    run_or_return(daemon_retval_init(), 1);

    /*.. fork the daemon */
    if (run_daemon && (pid = daemon_fork()) < 0)
    {

        /*.. error -> exit */
        daemon_retval_done();
        return 1;

    }
    else if (run_daemon && pid)
    {
        /*.. parent */
        int ret;

        /*.. 20 seconds timeout */
        if ((ret = daemon_retval_wait(20)) < 0)
        {
            daemon_log(LOG_ERR, "Could not recieve return value from daemon process: %s", strerror(errno));
            return 6;
        }

        if (ret != 0)
            daemon_log(LOG_ERR, "Daemon failed with return value %i", ret);

        return ret;
    }
    else
    {
        /* daemon */
        daemon_log(LOG_INFO, "Starting %s ver: " VERSION "...", daemon_log_ident);

        /* FIXME this doesn't work .... */
        run_or_retval(daemon_close_all(-1), 7);

        /*.. housekeeping */
        if (run_daemon)
            run_or_retval(daemon_pid_file_create(), 8);
        run_or_retval(daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, SIGHUP, 0), 9);

        /*.. init subsystems*/
        FD_ZERO(&fds);
        run_or_retval(system_init(&config, &fds, config_file_name), 10);

        /* add dameon signal fd to select() list */
        FD_SET(daemon_signal_fd(), &fds);

        /* Send our status to parent process */
        if (run_daemon)
            daemon_retval_send(0);
        daemon_log(LOG_INFO, "Started and working...");

        while (!quit)
        {
            fd_set fds2 = fds;

            /* Wait for an incoming signal */
            int ret = select(FD_SETSIZE, &fds2, 0, 0, 0);

            if (ret == -EINTR)
                continue;
            else if (ret < 0)
            {
                daemon_log(LOG_ERR, "select(): %s", strerror(errno));
                break;
            }

            /*.. handle daemon signals */
            if (FD_ISSET(daemon_signal_fd(), &fds2)) {
                int sig;
                run_or_retval((sig = daemon_signal_next()),  0);

                switch (sig) {
                case SIGINT:
                case SIGQUIT:
                case SIGTERM:
                    daemon_log(LOG_WARNING, "Got SIGINT, SIGQUIT or SIGTERM.");
                    quit = 1;
                    goto close_and_finish;

                case SIGHUP:
                    daemon_log(LOG_INFO, "Got HUP");
                    /* FIXME implement re-reading of the config */
                    break;
                default:
                    /*.. ignore other signals */;
                }
            }

            /*.. check all channels for readiness */
            run_or_retval(system_check_channels_and_process(&config, &fds2), 0);
        }

close_and_finish:
        system_close(&config, &fds);

finish:
        daemon_log(LOG_INFO, "Terminating.");
        if (run_daemon)
        {
            daemon_retval_send(255);
            daemon_signal_done();
            daemon_pid_file_remove();
        }

        return 0;
    }
}
