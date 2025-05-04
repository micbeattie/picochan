/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "cus_trace.h"

void __time_critical_func(cus_send_command_to_css)(pch_cu_t *cu) {
	int16_t tx_head = cu->tx_head;
        assert(tx_head >= 0);
	pch_unit_addr_t ua = (pch_unit_addr_t)tx_head;
        proto_packet_t p = cus_make_packet(cu, ua);
        memcpy(cu->tx_channel.cmdbuf, &p, sizeof p);
        trace_dev_packet(PCH_TRC_RT_CUS_SEND_TX_PACKET, cu,
                pch_get_devib(cu, ua), p);
        dmachan_start_src_cmdbuf(&cu->tx_channel);
}

void __time_critical_func(pop_tx_list)(pch_cu_t *cu) {
	int16_t current = cu->tx_head;
        assert(current != -1);
	pch_unit_addr_t ua = (pch_unit_addr_t)current;
        pch_devib_t *devib = pch_get_devib(cu, ua);

        pch_unit_addr_t next = devib->next;
	if (next == ua) {
		cu->tx_head = -1;
                cu->tx_tail = -1;
	} else {
		cu->tx_head = (int16_t)next;
		devib->next = ua; // remove from list by pointing at self
	}
}

void __time_critical_func(try_tx_next_command)(pch_cu_t *cu) {
	if (cu->tx_head > -1)
                cus_send_command_to_css(cu);
}

// push_tx_list pushes ua onto the singly-linked list with head and
// tail cu->tx_head and cu->tx_tail and returns the old tail.
// All manipulation is done under the devibs_lock.
int16_t __time_critical_func(push_tx_list)(pch_cu_t *cu, pch_unit_addr_t ua) {
        uint32_t status = devibs_lock();
	int16_t tx_tail = cu->tx_tail;
	if (tx_tail < 0) {
		cu->tx_head = (uint16_t)ua;
		cu->tx_tail = (uint16_t)ua;
	} else {
		// There's already a pending list: add ourselves at the end
		pch_unit_addr_t tx_tail_ua = (pch_unit_addr_t)tx_tail;
                pch_devib_t *tx_tail_devib = pch_get_devib(cu, tx_tail_ua);
		tx_tail_devib->next = ua;
		cu->tx_tail = (int16_t)ua;
	}

        devibs_unlock(status);
	return tx_tail;
}
