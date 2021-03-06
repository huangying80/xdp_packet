#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include "xdp_runtime.h"
#include "xdp_log.h"

#include "process.h"

int main(int argc, char *argv[])
{
    char       *eth = NULL;
    char       *ip = NULL;
    char       *prog = NULL;
    uint16_t    port = 0;
    uint16_t    queue;
    int         ret;
    int         c = -1;
    int         option_index;
    int         numa_node = -1;
    in_addr_t   server_addr;
    struct xdp_runtime runtime;

    struct option long_options[] = {
        {"dev", required_argument, NULL, 'd'},
        {"ip", required_argument, NULL, 'i'},
        {"port", required_argument, NULL, 'p'},
        {"prog", required_argument, NULL, 'g'},
        {"queue", required_argument, NULL, 'Q'},
        {"numa_node", required_argument, NULL, 'n'},
        {NULL, 0, NULL, 0}
    };
    while (1) {
        c = getopt_long(argc, argv, "d:i:p:g:Q:n:", long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 'd':
                eth = strdup(optarg);               
                break;
            case 'i':
                ip = strdup(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'g':
                prog = strdup(optarg);
                break;
            case 'Q':
                queue = atoi(optarg);
                break;
            case 'n':
                numa_node = atoi(optarg);
                break;
            default:
                fprintf(stderr, "argument error !\n");
                return -1;
        }
    }
    if (!eth || !eth[0] || !ip || !ip[0] || !prog || !prog[0] || !port) {
        fprintf(stderr, "argument error !\n");
        return -1;
    }

    ret = xdp_runtime_init(&runtime, eth, prog, NULL);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_init failed with %s\n", eth);
        goto out;
    }
    ret = xdp_runtime_udp_packet(port);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_udp_packet failed with %s\n", eth);
        goto out;
    }
    
    if (numa_node >= 0) {
        xdp_runtime_setup_numa(&runtime, numa_node);
    }
    ret = xdp_runtime_setup_queue(&runtime, queue, 1024);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_setup_queue failed with %s\n", eth);
        goto out;
    }
    if (queue > 1) {
        ret = xdp_runtime_setup_rss_ipv4(&runtime, IPPROTO_UDP);
        if (ret < 0) {
            fprintf(stderr, "xdp_runtime_setup_rss_ipv4 failed with %s\n", eth);
            goto out;
        }
    }
    DnsProcess::setServerAddr(ip);
    ret = xdp_runtime_setup_workers(&runtime, DnsProcess::worker, 0);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_setup_workers failed with %s\n", eth);
        goto out;
    }

    ret = xdp_runtime_startup_workers(&runtime);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_startup_workers failed with %s\n", eth);
        goto out;
    }

out:
    xdp_runtime_release(&runtime);
    return 0;
}
