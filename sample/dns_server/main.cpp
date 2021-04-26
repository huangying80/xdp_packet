#include <stdio.h>
#include "process.h"
#include "xdp_runtime.h"

int main(int argc, char *argv[])
{
    const char *eth = argv[1];
    const char *ip = argv[2];
    uint16_t    port = atoi(argv[3]);
    const char *prog = argv[4];
    int         ret;
    struct xdp_runtime runtime;

    ret = xdp_runtime_init(&runtime, eth, prog, NULL);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_init failed with %s\n", eth);
        goto out;
    }
    ret = xdp_runtime_udp_packet(53);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_udp_packet failed with %s\n", eth);
        goto out;
    }

    ret = xdp_runtime_ipv4_packet(ip, 32, XDP_UPDATE_IP_DST);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_ipv4_packet failed ip %s\n", ip);
        goto out;
    }
    ret = xdp_runtime_setup_queue(&runtime, 1, 1024);
    if (ret < 0) {
        fprintf(stderr, "xdp_runtime_setup_queue failed with %s\n", eth);
        goto out;
    }
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
