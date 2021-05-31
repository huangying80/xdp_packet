#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "xdp_runtime.h"
#include "xdp_log.h"

#include "synflood.h"

int main(int argc, char *argv[])
{
    char       *eth = NULL;
    char       *ip = NULL;
    char       *prog = NULL;
    uint16_t    port = 0;
    uint16_t    sender = 0;
    int         ret;
    int         c = -1;
    int         option_index;
    uint64_t    packetCount = 0;
    in_addr_t   server_addr;
    struct xdp_runtime runtime;

    struct option long_options[] = {
        {"dev", required_argument, NULL, 'd'},
        {"ip", required_argument, NULL, 'i'},
        {"port", required_argument, NULL, 'p'},
        {"prog", required_argument, NULL, 'g'},
        {"sender", required_argument, NULL, 's'},
        {"count", required_argument, NULL, 'n'},
        {NULL, 0, NULL, 0}
    };
    while (1) {
        c = getopt_long(argc, argv, "d:i:p:g:s:n:", long_options, &option_index);
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
            case 's':
                sender = atoi(optarg);
                break;
            case 'n':
                packetCount = atol(optarg);
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

    SynFlood::setDstAddr(ip, port);
    SynFlood::setPacketCount(packetCount);
    SynFlood::setSignal();

    ret = xdp_runtime_init(&runtime, eth, prog, NULL);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_init failed with %s\n", eth);
        goto out;
    }

    ret = xdp_runtime_setup_queue(&runtime, sender, 1024);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_setup_queue failed with %s\n", eth);
        goto out;
    }

    ret = xdp_runtime_setup_workers(&runtime, SynFlood::sender, 0);
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
