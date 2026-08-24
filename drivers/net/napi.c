#include <drivers/net_napi.h>

void napi_init(struct napi *napi, napi_poll_fn poll, uint32_t weight, void *private_data)
{
    napi->poll = poll;
    napi->weight = weight;
    napi->private_data = private_data;
    napi->scheduled = 0;
}

void napi_schedule(struct napi *napi)
{
    if (napi != 0)
        __atomic_store_n(&napi->scheduled, 1, __ATOMIC_RELEASE);
}

unsigned napi_poll(struct napi *napi)
{
    if (napi == 0 || napi->poll == 0 ||
        __atomic_load_n(&napi->scheduled, __ATOMIC_ACQUIRE) == 0)
        return 0;
    return napi->poll(napi, napi->weight);
}

void napi_complete(struct napi *napi)
{
    if (napi != 0)
        __atomic_store_n(&napi->scheduled, 0, __ATOMIC_RELEASE);
}