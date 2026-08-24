#ifndef DRIVERS_NET_NAPI_H
#define DRIVERS_NET_NAPI_H

#include <stdint.h>

struct napi;
typedef unsigned (*napi_poll_fn)(struct napi *napi, unsigned budget);

struct napi {
    napi_poll_fn poll;
    volatile uint32_t scheduled;
    uint32_t weight;
    void *private_data;
};

void napi_init(struct napi *napi, napi_poll_fn poll, uint32_t weight, void *private_data);
void napi_schedule(struct napi *napi);
unsigned napi_poll(struct napi *napi);
void napi_complete(struct napi *napi);

#endif