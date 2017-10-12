/*******************************************************************************
 * iio2udp.c
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

/*
 * Includes
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <math.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libdaemon/daemon.h>
#include <libconfig.h>

#include "iio2udp.h"
#include "iio.h"

/*
 * Settings
 */

#define IIO2UDP_DEFAULT_CONFIG_FILENAME "/etc/iio2udp"

/*
 * Type declarations
 */

typedef
struct channel channel_t;

struct channel
{
    /* .. scale, default 1.0 */
    double scale;

    /* .. internal scale of the device (the value must be divided by) */
    double iio_scale;

    /* .. offset, default 0.0 */
    double offset;

    /* .. sample time for the channel in ms */
    int sample_time;

    /* .. iio device name */
    const char *device_name;

    /* .. iio channel name */
    const char *channel_name;

    /* .. iio device reference */
    struct iio_device *rx;

    /* .. iio channel reference */
    struct iio_channel *ch;

    /* .. device index for UDP */
    int udp_device_index;

    /* .. device index for UDP */
    int udp_channel_index;

    /* .. use long packet format for UDP */
    int use_long_format;

    /* .. file descriptor of the timer */
    int timerfd;

    /* .. pointer to the next element in the list */
    channel_t *next;
};

typedef
struct deamon_config
{
    /* .. Single linked list of channel configs */
    channel_t *channels;

    /* .. context for interfacing libiio funcitons */
    struct iio_context *context;

    /* .. UDP port number we transmit to */
    int port;

    /* .. network interface to bind UDP socket to */
    const char *interface;

    /* .. broadcast UDP socket */
    int socket_broadcast;

    /* .. broadcast IP address */
    struct sockaddr_in baddr;
} daemon_config_t;

/*
 * Parsing of config file.
 * See default cofnfig for file format.
 */

int
parse_config(daemon_config_t *config, const char *config_file_name)
{
    config_t cf;
    int i;

    /* .. set default values for parameters */
    config->channels = NULL;
    config->port = IIO2UDP_DEFAULT_PORT;
    config->context = NULL;
    config->interface = NULL;

    config_init(&cf);

    /* set default name if none is provided */
    if (!config_file_name)
        config_file_name = IIO2UDP_DEFAULT_CONFIG_FILENAME;

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
    const config_setting_t *channels  = config_lookup(&cf, "channels");
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
                chc->scale = 1.0;
                chc->offset = 0.0;
                chc->iio_scale = 1.0;
                chc->next = NULL;
                chc->rx = NULL;
                chc->ch = NULL;
                chc->device_name = "";
                chc->channel_name = "";
                chc->sample_time = 100;
                chc->use_long_format = 0;
                chc->udp_device_index = 0;
                chc->udp_channel_index = i;
                chc->timerfd = 0;

                /* .. try reading channel settings
                 *    We copy strings here because they get destroyed together with cf,
                 *    and we don't really hold the reference to cf any longer.
                 */
                config_setting_lookup_float (channel, "scale", &chc->scale);
                config_setting_lookup_float (channel, "offset", &chc->offset);
                config_setting_lookup_int(channel, "sample_time", &chc->sample_time);
                config_setting_lookup_string(channel, "device", &chc->device_name);
                chc->device_name = strdup(chc->device_name);
                config_setting_lookup_string(channel, "channel", &chc->channel_name);
                chc->channel_name = strdup(chc->channel_name);
                config_setting_lookup_bool(channel, "long_format", &chc->use_long_format);
                config_setting_lookup_int(channel, "device_index", &chc->udp_device_index);
                config_setting_lookup_int(channel, "channel_index", &chc->udp_channel_index);
            }
        }
    }

    /* .. release */
    config_destroy(&cf);

    return 0;
}

/*
 * Signal channel handling
 */

int channel_init(struct iio_context *context, channel_t *chc, fd_set *fds)
{
    /* .. locate the device */
    chc->rx = iio_context_find_device(context, chc->device_name);
    if (!chc->rx)
    {
        char err_str[1024];
        iio_strerror(errno, err_str, sizeof(err_str));
        daemon_log(LOG_WARNING, "Cannot open iio device '%s'. Error '%s'", chc->device_name, err_str);

        goto error;
    }

    /* .. locate the channel */
    chc->ch = iio_device_find_channel(chc->rx, chc->channel_name, false);
    if (!chc->ch)
    {
        char err_str[1024];
        iio_strerror(errno, err_str, sizeof(err_str));
        daemon_log(LOG_WARNING, "Cannot open iio channel '%s/%s'. Error '%s'", chc->device_name, chc->channel_name, err_str);

        goto error;
    }

    //		int i;
    //		for (i=0; i < iio_channel_get_attrs_count(chc->ch); i++)
    //		{
    //			daemon_log(LOG_INFO, "Attr: %s", iio_channel_get_attr(chc->ch, i));
    //		}

    /* .. try to read hardware scale. Use 1.0 if failed */
    iio_channel_attr_read_double(chc->ch, "scale", &chc->iio_scale);

    /* .. create fd timer using sample_time */
    if ((chc->timerfd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK)) < 0)
    {
        daemon_log(LOG_ERR, "Cannot create the timer for '%s/%s'. %m", chc->device_name, chc->channel_name);
        goto error;
    }

    /* .. set and start the timer */
    struct itimerspec interval =
    {	{ floor(chc->sample_time / 1000.0), fmod(chc->sample_time * 1e6, 1e9) },
    { floor(chc->sample_time / 1000.0), fmod(chc->sample_time * 1e6, 1e9) } };

    if (timerfd_settime(chc->timerfd, 0, &interval, NULL) < 0)
    {
        daemon_log(LOG_ERR, "Error calling timerfd_settime for '%s/%s'. %m.", chc->device_name, chc->channel_name);
        goto error;
    }

    /* .. add fd to the list for select() call */
    FD_SET(chc->timerfd, fds);

    return 0;

error:
    /* .. memory does not need to be freed here. It is released together with the context */
    chc->rx = NULL;
    chc->ch = NULL;

    return -1;
}

int channel_process(daemon_config_t *config, channel_t *chc)
{
    int64_t dummy;
    if (read(chc->timerfd, &dummy, sizeof(dummy)) != sizeof(dummy))
        return -EINPROGRESS;

    /* .. fill in short packet */
    iio2udp_packet_short_t p_short = {
        .version = IIO2UDP_PACKET_VERSION,
        .device_id = htons(chc->udp_device_index),
        .channel_id = htons(chc->udp_channel_index),
    };

    /* .. try to read value */
    double value;
    if (iio_channel_attr_read_double(chc->ch, "raw", &value) == EXIT_SUCCESS)
    {
        /* .. condition the value */
        double conditioned = value / (chc->iio_scale != 0 ? chc->iio_scale : 1.0 ) * chc->scale + chc->offset;

        /*.. fill in data for the short packet */
        p_short.OPCQuality = htons(0xC0); /* .. this is good quality, no limits */
        p_short.value_dbl = conditioned;
    }
    else
    {
        /*.. fill in data for the short packet */
        p_short.OPCQuality = htons(0x00); /* .. this is bad quality */
    }

    /* .. fill in long packet */
    iio2udp_packet_long_t p_long = {
        .data = p_short,
    };

    void * packet = &p_short;
    size_t packet_length = sizeof(p_short);

    /* .. fill in long packet if required */
    if (chc->use_long_format)
    {
        /* .. copy first sizeof(p_long.xxx_name) bytes of strings */
        strncpy(p_long.device_name, chc->device_name, sizeof(p_long.device_name));
        strncpy(p_long.channel_name, chc->channel_name, sizeof(p_long.channel_name));

        /* .. and keep the pointer to the long packet */
        packet = &p_long;
        packet_length = sizeof(p_long);
    }

    /* .. send the packet out */
    int err;
    if ((err = sendto(config->socket_broadcast,
                      packet, packet_length,
                      0,
                      (struct sockaddr *)&config->baddr, sizeof(config->baddr))
         != packet_length))
        daemon_log(LOG_WARNING, "Error sending data to UDP socket. Data loss occured. %d, %m", err);

    return 0;
}

int channel_is_ready(channel_t *chc, fd_set *fds)
{
    return FD_ISSET(chc->timerfd, fds);
}

int channel_close(channel_t *chc, fd_set *fds)
{
    /* .. add fd to the list for select() call */
    FD_CLR(chc->timerfd, fds);

    /* close and destroy the timer */
    if (close(chc->timerfd) < 0)
    {
        daemon_log(LOG_ERR, "Error closing timer fd for '%s/%s'. %m.", chc->device_name, chc->channel_name);
        return -1;
    }
    chc->timerfd = 0;

    /* .. free strings */
    free((void *)chc->channel_name);
    chc->channel_name = NULL;
    free((void *)chc->device_name);
    chc->device_name = NULL;

    /* .. other memory does not need to be freed here. It is released together with the context */
    chc->rx = NULL;
    chc->ch = NULL;

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

    if (!(config->context = iio_create_default_context()))
    {
        char err_str[1024];
        iio_strerror(errno, err_str, sizeof(err_str));
        daemon_log(LOG_WARNING, "Cannot create default iio context. Error '%s'", err_str);

        return -ENODEV;
    }

    /* .. init all channels */
    int good_channels = 0;
    channel_t *chc = config->channels;

    while (chc)
    {
        /* .. try to initialize channel */
        if (channel_init(config->context, chc, fds)  == 0)
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
                daemon_log(LOG_WARNING, "Error processing channel '%s/%s'. Error %d", chc->device_name, chc->channel_name, err);

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
            daemon_log(LOG_WARNING, "Error closing channel '%s/%s'", chc->device_name, chc->channel_name);

        /* .. free this element and go to the next in list */
        channel_t *next = chc->next;
        free(chc);
        chc = next;
    }

    /* .. destroy and free default iio context */
    iio_context_destroy(config->context);
    config->context  = NULL;

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
    const char* config_file_name = IIO2UDP_DEFAULT_CONFIG_FILENAME;

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
