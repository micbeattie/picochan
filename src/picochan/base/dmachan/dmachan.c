#include "picochan/dmachan.h"

dmachan_1way_config_t dmachan_1way_config_claim(uint32_t addr, dma_channel_config ctrl) {
        pch_dmaid_t dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        return dmachan_1way_config_make(dmaid, addr, ctrl);
}

dmachan_config_t dmachan_config_claim(uint32_t txaddr, dma_channel_config txctrl, uint32_t rxaddr, dma_channel_config rxctrl) {
        return ((dmachan_config_t){
                .tx = dmachan_1way_config_claim(txaddr, txctrl),
                .rx = dmachan_1way_config_claim(rxaddr, rxctrl)
        });
}
