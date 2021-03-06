/*
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received stack copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>

#include <lib/esim/esim.h>
#include <lib/esim/trace.h>
#include <lib/util/debug.h>
#include <lib/util/linked-list.h>
#include <lib/util/list.h>
#include <lib/util/string.h>
#include <network/network.h>
#include <network/node.h>

#include "cache.h"
#include "directory.h"
#include "mem-system.h"
#include "nmoesi-wt.h"
#include "mod-stack.h"
#include "prefetcher.h"
#include "plotter.h"

/* Events */

int EV_MOD_NMOESI_LOAD_WT;
int EV_MOD_NMOESI_LOAD_LOCK_WT;
int EV_MOD_NMOESI_LOAD_ACTION_WT;
int EV_MOD_NMOESI_LOAD_MISS_WT;
int EV_MOD_NMOESI_LOAD_UNLOCK_WT;
int EV_MOD_NMOESI_LOAD_FINISH_WT;

int EV_MOD_NMOESI_STORE_WT;
int EV_MOD_NMOESI_STORE_LOCK_WT;
int EV_MOD_NMOESI_STORE_ACTION_WT;
int EV_MOD_NMOESI_STORE_UNLOCK_WT;
int EV_MOD_NMOESI_STORE_FINISH_WT;

int EV_MOD_NMOESI_NC_STORE_WT;
int EV_MOD_NMOESI_NC_STORE_LOCK_WT;
int EV_MOD_NMOESI_NC_STORE_ACTION_WT;
int EV_MOD_NMOESI_NC_STORE_MISS_WT;
int EV_MOD_NMOESI_NC_STORE_UNLOCK_WT;
int EV_MOD_NMOESI_NC_STORE_MERGE_WT;
int EV_MOD_NMOESI_NC_STORE_FINISH_WT;

int EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT;
int EV_MOD_NMOESI_FIND_AND_LOCK_PORT_READ_WT;
int EV_MOD_NMOESI_FIND_AND_LOCK_ACTION_READ_WT;
int EV_MOD_NMOESI_FIND_AND_LOCK_FINISH_READ_WT;

int EV_MOD_NMOESI_FIND_AND_LOCK_WRITE_WT;
int EV_MOD_NMOESI_FIND_AND_LOCK_PORT_WRITE_WT;
int EV_MOD_NMOESI_FIND_AND_LOCK_ACTION_WRITE_WT;
int EV_MOD_NMOESI_FIND_AND_LOCK_FINISH_WRITE_WT;

int EV_MOD_NMOESI_WRITE_REQUEST_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_ACTION_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_REPLY_WT;
int EV_MOD_NMOESI_WRITE_REQUEST_FINISH_WT;

int EV_MOD_NMOESI_WRITE_DATA_WT;
int EV_MOD_NMOESI_WRITE_DATA_RECEIVE_WT;
int EV_MOD_NMOESI_WRITE_DATA_ACTION_WT;
int EV_MOD_NMOESI_WRITE_DATA_BLOCK_WT;
int EV_MOD_NMOESI_WRITE_DATA_LOWER_WT;
int EV_MOD_NMOESI_WRITE_DATA_DONE_WT;
int EV_MOD_NMOESI_WRITE_DATA_REPLY_WT;
int EV_MOD_NMOESI_WRITE_DATA_FINISH_WT;

int EV_MOD_NMOESI_READ_REQUEST_WT;
int EV_MOD_NMOESI_READ_REQUEST_RECEIVE_WT;
int EV_MOD_NMOESI_READ_REQUEST_ACTION_WT;
int EV_MOD_NMOESI_READ_REQUEST_UPDOWN_WT;
int EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS_WT;
int EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH_WT;
int EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WT;
int EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS_WT;
int EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH_WT;
int EV_MOD_NMOESI_READ_REQUEST_REPLY_WT;
int EV_MOD_NMOESI_READ_REQUEST_FINISH_WT;

int EV_MOD_NMOESI_PEER_SEND_WT;
int EV_MOD_NMOESI_PEER_RECEIVE_WT;
int EV_MOD_NMOESI_PEER_REPLY_WT;
int EV_MOD_NMOESI_PEER_FINISH_WT;

int EV_MOD_NMOESI_MESSAGE_WT;
int EV_MOD_NMOESI_MESSAGE_RECEIVE_WT;
int EV_MOD_NMOESI_MESSAGE_ACTION_WT;
int EV_MOD_NMOESI_MESSAGE_REPLY_WT;
int EV_MOD_NMOESI_MESSAGE_FINISH_WT;


static void update_mod_stats(struct mod_t *mod, const struct mod_stack_t *stack)
{
	/* Record access type.  I purposefully chose to record both hits and
	 * misses separately here so that we can sanity check them against
	 * the total number of accesses. */
	if (stack->request_dir == mod_request_up_down)
	{
		if (stack->read)
		{
			mod->reads++;
			if (stack->retry)
				mod->retry_reads++;

			if (stack->hit)
			{
				mod->read_hits++;
				if (stack->retry)
					mod->retry_read_hits++;
			}
			else
			{
				mod->read_misses++;
				if (stack->retry)
					mod->retry_read_misses++;
			}

		}
		else if (stack->nc_write)  /* Must go after read */
		{
			mod->nc_writes++;
			if (stack->retry)
				mod->retry_nc_writes++;

			if (stack->hit)
			{
				mod->nc_write_hits++;
				if (stack->retry)
					mod->retry_nc_write_hits++;
			}
			else
			{
				mod->nc_write_misses++;
				if (stack->retry)
					mod->retry_nc_write_misses++;
			}
		}
		else if (stack->write)
		{
			mod->writes++;
			if (stack->retry)
				mod->retry_writes++;

			if (stack->hit)
			{
				mod->write_hits++;
				if (stack->retry)
					mod->retry_write_hits++;
			}
			else
			{
				mod->write_misses++;
				if (stack->retry)
					mod->retry_write_misses++;
			}
		}
		else if (stack->prefetch)
		{
			mod->prefetches++;
			if (stack->retry)
				mod->retry_prefetches++;
		}
		else
		{
			fatal("Invalid memory operation type");
		}
	}
	else if (stack->request_dir == mod_request_down_up)
	{
		assert(stack->hit);

		if (stack->write)
		{
			mod->write_probes++;
			if (stack->retry)
				mod->retry_write_probes++;
		}
		else if (stack->read)
		{
			mod->read_probes++;
			if (stack->retry)
				mod->retry_read_probes++;
		}
		else
		{
			fatal("Invalid memory operation type");
		}
	}
	else
	{
		mod->hlc_evictions++;
	}

}


/* NMOESI Protocol */

void mod_handler_nmoesi_load_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_LOAD_WT)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s load\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"load\" "
				"state=\"%s:load\" addr=0x%x\n",
				stack->id, mod->name, stack->addr);
		if (mem_snap_period != 0)
		{
			assert(main_snapshot);
			long long cycle = esim_domain_cycle(mem_domain_index);

			mem_system_snapshot_record(main_snapshot, cycle, stack->addr, 0);
		}

		/* Record access */
		mod_access_start(mod, stack, mod_access_load);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_load, stack->addr, stack);
		if (master_stack)
		{
			mod->coalesced_reads++;
			mod_coalesce(mod, master_stack, stack);
			mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_LOAD_FINISH_WT);
			return;
		}

		/* Next event */
		esim_schedule_event(EV_MOD_NMOESI_LOAD_LOCK_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_LOCK_WT)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s load lock\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_lock\"\n",
				stack->id, mod->name);

		/* If there is any older write, wait for it */
		older_stack = mod_in_flight_write(mod, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n",
					stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_LOAD_LOCK_WT);
			return;
		}

		/* If there is any older access to the same address that this access could not
		 * be coalesced with, wait for it. */
		older_stack = mod_in_flight_address(mod, stack->addr, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for access %lld\n",
					stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_LOAD_LOCK_WT);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
				EV_MOD_NMOESI_LOAD_ACTION_WT, stack);
		new_stack->blocking = 1;
		new_stack->read = 1;
		new_stack->retry = stack->retry;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_ACTION_WT)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s load action\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_action\"\n",
				stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_LOAD_LOCK_WT, stack, retry_lat);
			return;
		}

		/* Hit */
		if (stack->state)
		{
			esim_schedule_event(EV_MOD_NMOESI_LOAD_UNLOCK_WT, stack, 0);

			/* The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may have been a miss. */
			prefetcher_access_hit(stack, mod);

			return;
		}

		/* Miss */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_LOAD_MISS_WT, stack);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_WT, new_stack, 0);

		/* The prefetcher may be interested in this miss */
		prefetcher_access_miss(stack, mod);

		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_MISS_WT)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s load miss\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_miss\"\n",
				stack->id, mod->name);

		/* Error on read request. Unlock block and retry load. */
		if (stack->err)
		{
			retry_lat = mod_get_retry_latency(mod);
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_LOAD_LOCK_WT, stack, retry_lat);
			return;
		}

		/* Set block state to excl/shared depending on return var 'shared'.
		 * Also set the tag of the block. */
		cache_set_block(mod->cache, stack->set, stack->way, stack->tag,
				stack->shared ? cache_block_shared : cache_block_exclusive);

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_LOAD_UNLOCK_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_UNLOCK_WT)
	{
		mem_debug("  %lld %lld 0x%x %s load unlock\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_unlock\"\n",
				stack->id, mod->name);

		/* Unlock directory entry */
		dir_entry_unlock(mod->dir, stack->set, stack->way);

		/* Impose the access latency before continuing */
		esim_schedule_event(EV_MOD_NMOESI_LOAD_FINISH_WT, stack,
				mod->data_latency);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_FINISH_WT)
	{
		mem_debug("%lld %lld 0x%x %s load finish\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_finish\"\n",
				stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
				stack->id);

		/* Increment witness variable */
		if (stack->witness_ptr)
			(*stack->witness_ptr)++;

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_store_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_STORE_WT)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s store\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"store\" "
				"state=\"%s:store\" addr=0x%x\n",
				stack->id, mod->name, stack->addr);
		if (mem_snap_period != 0)
		{
			assert(main_snapshot);
			long long cycle = esim_domain_cycle(mem_domain_index);

			mem_system_snapshot_record(main_snapshot, cycle, stack->addr, 1);
		}

		/* Record access */
		mod_access_start(mod, stack, mod_access_store);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_store, stack->addr, stack);
		if (master_stack)
		{
			mod->coalesced_writes++;
			mod_coalesce(mod, master_stack, stack);
			mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_STORE_FINISH_WT);

			/* Increment witness variable */
			if (stack->witness_ptr)
				(*stack->witness_ptr)++;

			return;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_STORE_LOCK_WT, stack, 0);
		return;
	}


	if (event == EV_MOD_NMOESI_STORE_LOCK_WT)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s store lock\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_lock\"\n",
				stack->id, mod->name);

		/* If there is any older access, wait for it */
		older_stack = stack->access_list_prev;
		if (older_stack)
		{
			mem_debug("    %lld wait for access %lld\n",
					stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_STORE_LOCK_WT);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
				EV_MOD_NMOESI_STORE_ACTION_WT, stack);
		new_stack->blocking = 1;
		new_stack->write = 1;
		new_stack->retry = stack->retry;
		new_stack->witness_ptr = stack->witness_ptr;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT, new_stack, 0);

		/* Set witness variable to NULL so that retries from the same
		 * stack do not increment it multiple times */
		stack->witness_ptr = NULL;

		return;
	}

	if (event == EV_MOD_NMOESI_STORE_ACTION_WT)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s store action\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_action\"\n",
				stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_STORE_LOCK_WT, stack, retry_lat);
			return;
		}

		/* Hit - state=M/E */
		if (stack->state == cache_block_modified ||
				stack->state == cache_block_exclusive)
		{
			esim_schedule_event(EV_MOD_NMOESI_STORE_UNLOCK_WT, stack, 0);

			/* The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may have been a miss. */
			prefetcher_access_hit(stack, mod);

			return;
		}

		/* Miss - state=O/S/I/N */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_STORE_UNLOCK_WT, stack);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_WT, new_stack, 0);

		/* The prefetcher may be interested in this miss */
		prefetcher_access_miss(stack, mod);

		return;
	}

	if (event == EV_MOD_NMOESI_STORE_UNLOCK_WT)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s store unlock\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_unlock\"\n",
				stack->id, mod->name);

		/* Error in write request, unlock block and retry store. */
		if (stack->err)
		{
			retry_lat = mod_get_retry_latency(mod);
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_STORE_LOCK_WT, stack, retry_lat);
			return;
		}

		/* Update tag/state and unlock */
		cache_set_block(mod->cache, stack->set, stack->way,
				stack->tag, cache_block_modified);
		dir_entry_unlock(mod->dir, stack->set, stack->way);

		/* Impose the access latency before continuing */
		mod->data_accesses++;
		esim_schedule_event(EV_MOD_NMOESI_STORE_FINISH_WT, stack,
				mod->data_latency);
		return;
	}

	if (event == EV_MOD_NMOESI_STORE_FINISH_WT)
	{
		mem_debug("%lld %lld 0x%x %s store finish\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_finish\"\n",
				stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
				stack->id);

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_nc_store_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_NC_STORE_WT)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s nc store\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"nc_store\" "
				"state=\"%s:nc store\" addr=0x%x\n", stack->id, mod->name, stack->addr);

		if (mem_snap_period != 0)
		{
			assert(main_snapshot);
			long long cycle = esim_domain_cycle(mem_domain_index);

			mem_system_snapshot_record(main_snapshot, cycle, stack->addr, 1);
		}

		/* Record access */
		mod_access_start(mod, stack, mod_access_nc_store);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_nc_store, stack->addr, stack);
		if (master_stack)
		{
			mod->coalesced_nc_writes++;
			mod_coalesce(mod, master_stack, stack);
			mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_NC_STORE_FINISH_WT);
			return;
		}

		/* Next event */
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_LOCK_WT)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s nc store lock\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_lock\"\n",
				stack->id, mod->name);

		/* If there is any older write, wait for it */
		older_stack = mod_in_flight_write(mod, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n", stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_NC_STORE_LOCK_WT);
			return;
		}

		/* If there is any older access to the same address that this access could not
		 * be coalesced with, wait for it. */
		older_stack = mod_in_flight_address(mod, stack->addr, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n", stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_NC_STORE_LOCK_WT);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
				EV_MOD_NMOESI_NC_STORE_ACTION_WT, stack);
		new_stack->blocking = 1;
		new_stack->nc_write = 1;
		new_stack->retry = stack->retry;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_WRITE_WT, new_stack, 0);
		return;
	}
	/*
	if (event == EV_MOD_NMOESI_NC_STORE_WRITEBACK)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s nc store writeback\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_writeback\"\n",
			stack->id, mod->name);

		 Error locking
		if (stack->err)
		{
			mod->nc_write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK, stack, retry_lat);
			return;
		}

		 In write-through there shouldn't be ever an eviction
		if (stack->state == cache_block_modified || stack->state == cache_block_owned)
		{
			stack->eviction = 1;
			new_stack = mod_stack_create(stack->id, mod, 0,
				EV_MOD_NMOESI_NC_STORE_ACTION, stack);
			new_stack->set = stack->set;
			new_stack->way = stack->way;
			esim_schedule_event(EV_MOD_NMOESI_EVICT, new_stack, 0);
			return;
		}

		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_ACTION, stack, 0);
		return;
	}
	 */
	if (event == EV_MOD_NMOESI_NC_STORE_ACTION_WT)
	{
		int retry_lat;
		stack->pending = 1;

		mem_debug("  %lld %lld 0x%x %s nc store action\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_action\"\n",
				stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK_WT, stack, retry_lat);
			return;
		}

		/* Main memory modules are a special case */
		if (mod->kind == mod_kind_main_memory)
		{
			assert(stack->state);
			/* For non-coherent stores, finding an E or M for the state of
			 * a cache block in the directory still requires a message to 
			 * the lower-level module so it can update its owner field.
			 * These messages should not be sent if the module is a main
			 * memory module. */
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_UNLOCK_WT, stack, 0);
			return;
		}

		/* In either case, hit or miss we are initiating a write-request to the
		 * lower mods.
		 */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_NC_STORE_MISS_WT, stack);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->nc_write = 1;
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_WT, new_stack, 0);
		stack->pending++;
		mem_debug("  %lld %lld 0x%x %s nc store thread launch (pending=%d)\n", esim_time, stack->id,
				stack->addr, mod->name, stack->pending);
		/* N/S are hit
		if (stack->state == cache_block_shared || stack->state == cache_block_noncoherent)
		{
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_UNLOCK, stack, 0);
		}*/
		/* E state must tell the lower-level module to remove this module as an owner
		if (stack->state == cache_block_exclusive)
		{
			new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_NC_STORE_MISS, stack);
			new_stack->message = message_clear_owner;
			new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
			esim_schedule_event(EV_MOD_NMOESI_MESSAGE, new_stack, 0);
		}
		 Modified and Owned states need to call read request because we've already
		 * evicted the block so that the lower-level cache will have the latest value
		 * before it becomes non-coherent
		else
		{

			new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_NC_STORE_MISS, stack);
			new_stack->peer = mod_stack_set_peer(mod, stack->state);
			new_stack->nc_write = 1;
			new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
			new_stack->request_dir = mod_request_up_down;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);*/
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_UNLOCK_WT, stack, 0);
		//		}

		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_MISS_WT)
	{
		//		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s nc store miss\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_miss\"\n",
				stack->id, mod->name);

		//		Error on read request. Unlock block and retry nc store.
		if (stack->err)
		{
			mem_debug("  %lld %lld 0x%x %s nc store -- problem with write-through\n", esim_time, stack->id,
					stack->addr, mod->name);
			mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_miss\"\n",
					stack->id, mod->name);
			/*
			mod->nc_write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK, stack, retry_lat);
			return; */
		}

		//		Continue
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_MERGE_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_UNLOCK_WT)
	{
		mem_debug("  %lld %lld 0x%x %s nc store unlock\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_unlock\"\n",
				stack->id, mod->name);

		/* Set block state to excl/shared depending on return var 'shared'.
		 * Also set the tag of the block. */
		if (stack->state)
		{
			cache_set_block(mod->cache, stack->set, stack->way, stack->tag,
					cache_block_noncoherent);

			/* Unlock directory entry */
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_MERGE_WT, stack,
					mod->data_latency);
			return;
		}
		/* Impose the access latency before continuing */
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_MERGE_WT, stack,
				0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_MERGE_WT)
	{
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;

		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_FINISH_WT, stack, 0);
		return;
	}
	if (event == EV_MOD_NMOESI_NC_STORE_FINISH_WT)
	{
		mem_debug("%lld %lld 0x%x %s nc store finish\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_finish\"\n",
				stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
				stack->id);

		/* Increment witness variable */
		if (stack->witness_ptr)
			(*stack->witness_ptr)++;

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_find_and_lock_read_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	//	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock (blocking=%d)\n",
				esim_time, stack->id, stack->addr, mod->name, stack->blocking);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock\"\n",
				stack->id, mod->name);

		/* Default return values */
		ret->err = 0;

		/* If this stack has already been assigned a way, keep using it */
		stack->way = ret->way;

		/* Get a port */
		mod_lock_port(mod, stack, EV_MOD_NMOESI_FIND_AND_LOCK_PORT_READ_WT);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_PORT_READ_WT)
	{
		struct mod_port_t *port = stack->port;
		struct dir_lock_t *dir_lock;

		assert(stack->port);
		mem_debug("  %lld %lld 0x%x %s find and lock port\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_port\"\n",
				stack->id, mod->name);

		/* Statistics */
		mod->accesses++;
		if (stack->retry)
			mod->retry_accesses++;


		/* Set parent stack flag expressing that port has already been locked.
		 * This flag is checked by new writes to find out if it is already too
		 * late to coalesce. */
		ret->port_locked = 1;

		/* Look for block. */
		stack->hit = mod_find_block(mod, stack->addr, &stack->set,
				&stack->way, &stack->tag, &stack->state);

		/* Debug */
		if (stack->hit)
			/*			mem_debug("    %lld 0x%x %s hit: set=%d, way=%d, state=%s\n", stack->id,
					stack->tag, mod->name, stack->set, stack->way,
					str_map_value(&cache_block_state_map, stack->state));

		 Statistics
		mod->accesses++;
		if (stack->hit)
			mod->hits++;

		if (stack->read)*/
		{
			/*	mod->reads++;
			mod->effective_reads++;
			stack->blocking ? mod->blocking_reads++ : mod->non_blocking_reads++;
			if (stack->hit)
				mod->read_hits++;*/
			assert(stack->state);
			mem_debug("    %lld 0x%x %s hit: set=%d, way=%d, "
					"state=%s\n", stack->id, stack->tag, mod->name,
					stack->set, stack->way, str_map_value(
							&cache_block_state_map, stack->state));
		}
		/*
		else if (stack->prefetch)
		{
			mod->prefetches++;
		}
		else if (stack->nc_write)  // Must go after read
		{
			mod->nc_writes++;
			mod->effective_nc_writes++;
			stack->blocking ? mod->blocking_nc_writes++ : mod->non_blocking_nc_writes++;
			if (stack->hit)
				mod->nc_write_hits++;
		}
		else if (stack->write)
		{
			mod->writes++;
			mod->effective_writes++;
			stack->blocking ? mod->blocking_writes++ : mod->non_blocking_writes++;

			// Increment witness variable when port is locked
			if (stack->witness_ptr)
			{
				(*stack->witness_ptr)++;
				stack->witness_ptr = NULL;
			} */

		/* If a store access hits in the cache, we can be sure
		 * that the it will complete and can allow processing to
		 * continue by incrementing the witness pointer.  Misses
		 * cannot do this without violating consistency, so their
		 * witness pointer is updated in the write request logic. */
		if (stack->write && stack->hit && stack->witness_ptr)
			(*stack->witness_ptr)++;

		/*		if (stack->hit)
		{
			 If the request is down-up and the block was not
		 * found, it must be because it was previously
		 * evicted.  We must stop here because going any
		 * further would result in a new space being allocated
		 * for it.
			if (stack->request_dir == mod_request_down_up)
			{
				mem_debug("        %lld block not found",
						stack->id);
				ret->block_not_found = 1;
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				mod_stack_return(stack);
				return;
			}
		 */

		if (!stack->hit)
		{
			/* Find victim */
			if (stack->way < 0)
			{
				stack->way = cache_replace_block(mod->cache, stack->set);
			}
		}
		assert(stack->way >= 0);

		/* If directory entry is locked and the call to FIND_AND_LOCK is not
		 * blocking, release port and return error. */
		dir_lock = dir_lock_get(mod->dir, stack->set, stack->way);
		if (dir_lock->lock && !stack->blocking)
		{
			mem_debug("    %lld 0x%x %s block locked at set=%d, way=%d by A-%lld - aborting\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
			ret->err = 1;
			mod_unlock_port(mod, port, stack);
			ret->port_locked = 0;
			mod_stack_return(stack);

			/* Statistics */
			mod->dir_entry_conflicts++;
			if (stack->retry)
				mod->retry_dir_entry_conflicts++;
			return;
		}

		/* Lock directory entry. If lock fails, port needs to be released to prevent
		 * deadlock.  When the directory entry is released, locking port and
		 * directory entry will be retried. */
		if (!dir_entry_lock(mod->dir, stack->set, stack->way, EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT,
				stack))
		{
			mem_debug("    %lld 0x%x %s block locked at set=%d, way=%d by A-%lld - waiting\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
			mod_unlock_port(mod, port, stack);
			ret->port_locked = 0;

			/* Statistics */
			mod->dir_entry_conflicts++;
			if (stack->retry)
				mod->retry_dir_entry_conflicts++;
			return;
		}

		/* Miss */
		if (!stack->hit)
		{
			/* Find victim */
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state || !dir_entry_group_shared_or_owned(mod->dir,
					stack->set, stack->way));
			mem_debug("    %lld 0x%x %s miss -> lru: set=%d, way=%d, state=%s\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way,
					str_map_value(&cache_block_state_map, stack->state));
		}

		/* Statistics */
		update_mod_stats(mod, stack);

		/* Entry is locked. Record the transient tag so that a subsequent lookup
		 * detects that the block is being brought.
		 * Also, update LRU counters here. */
		cache_set_transient_tag(mod->cache, stack->set, stack->way, stack->tag);
		cache_access_block(mod->cache, stack->set, stack->way);

		/* Access latency */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_ACTION_READ_WT, stack, mod->dir_latency);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_ACTION_READ_WT)
	{
		struct mod_port_t *port = stack->port;

		assert(port);
		mem_debug("  %lld %lld 0x%x %s find and lock action\n", esim_time, stack->id,
				stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_action\"\n",
				stack->id, mod->name);

		/* Release port */
		mod_unlock_port(mod, port, stack);
		ret->port_locked = 0;

		/* On miss, invalidate if victim is a valid block. */
		if (!stack->hit && stack->state)
		{/*
			stack->eviction = 1;
			new_stack = mod_stack_create(stack->id, mod, 0,
				EV_MOD_NMOESI_FIND_AND_LOCK_FINISH, stack);
			new_stack->set = stack->set;
			new_stack->way = stack->way;
			esim_schedule_event(EV_MOD_NMOESI_EVICT, new_stack, 0);
			return;
		 */
			cache_set_block(mod->cache, stack->src_set, stack->src_way,
					0, cache_block_invalid);
			stack->state = cache_block_invalid;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_FINISH_READ_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_FINISH_READ_WT)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock finish (err=%d)\n", esim_time, stack->id,
				stack->tag, mod->name, stack->err);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_finish\"\n",
				stack->id, mod->name);

		/* If evict produced err, return err */
		if (stack->err)
		{
			mem_debug("  XXX %lld %lld 0x%x %s find and lock finish (err=%d)\n", esim_time, stack->id,
					stack->tag, mod->name, stack->err);
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state);
			assert(stack->eviction);
			ret->err = 1;
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mod_stack_return(stack);
			return;
		}

		/* Eviction
		if (stack->eviction)
		{
			mod->evictions++;
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(!stack->state);
		}*/

		/* If this is a main memory, the block is here. A previous miss was just a miss
		 * in the directory. */
		if (mod->kind == mod_kind_main_memory && !stack->state)
		{
			stack->state = cache_block_exclusive;
			cache_set_block(mod->cache, stack->set, stack->way,
					stack->tag, stack->state);
		}

		/* Return */
		ret->err = 0;
		ret->set = stack->set;
		ret->way = stack->way;
		ret->state = stack->state;
		ret->tag = stack->tag;
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_read_request_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;

	struct dir_t *dir;
	struct dir_entry_t *dir_entry;

	uint32_t dir_entry_tag, z;

	if (event == EV_MOD_NMOESI_READ_REQUEST_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s read request\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request\"\n",
				stack->id, mod->name);

		/* Default return values*/
		ret->shared = 0;
		ret->err = 0;

		/* Checks */
		assert(stack->request_dir);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
				stack->request_dir == mod_request_down_up);
		assert(mod_get_low_mod(target_mod, stack->addr) == mod ||
				stack->request_dir == mod_request_up_down);

		/* Get source and destination nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = mod->low_net_node;
			dst_node = target_mod->high_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = mod->high_net_node;
			dst_node = target_mod->low_net_node;
		}

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, 8,
				EV_MOD_NMOESI_READ_REQUEST_RECEIVE_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_RECEIVE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s read request receive\n", esim_time, stack->id,
				stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_receive\"\n",
				stack->id, target_mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);
		else
			net_receive(target_mod->low_net, target_mod->low_net_node, stack->msg);

		/* Find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
				EV_MOD_NMOESI_READ_REQUEST_ACTION_WT, stack);
		new_stack->blocking = stack->request_dir == mod_request_down_up;
		new_stack->read = 1;
		new_stack->retry = 0;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_ACTION_WT)
	{
		mem_debug("  %lld %lld 0x%x %s read request action\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_action\"\n",
				stack->id, target_mod->name);

		/* Check block locking error. If read request is down-up, there should not
		 * have been any error while locking. */
		if (stack->err)
		{
			assert(stack->request_dir == mod_request_up_down);
			ret->err = 1;
			mod_stack_set_reply(ret, reply_ack_error);
			stack->reply_size = 8;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY_WT, stack, 0);
			return;
		}
		esim_schedule_event(stack->request_dir == mod_request_up_down ?
				EV_MOD_NMOESI_READ_REQUEST_UPDOWN_WT : EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_UPDOWN_WT)
	{
		//		struct mod_t *owner;

		mem_debug("  %lld %lld 0x%x %s read request updown\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_updown\"\n",
				stack->id, target_mod->name);

		stack->pending = 1;

		/* Set the initial reply message and size.  This will be adjusted later if
		 * a transfer occur between peers. */
		stack->reply_size = mod->block_size + 8;
		mod_stack_set_reply(stack, reply_ack_data);

		if (stack->state)
		{
			/* Status = M/O/E/S/N
			 * Check: address is a multiple of requester's block_size
			 * Check: no sub-block requested by mod is already owned by mod
			assert(stack->addr % mod->block_size == 0);
			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				assert(dir_entry_tag < stack->tag + target_mod->block_size);
				if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
					continue;
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				assert(dir_entry->owner != mod->low_net_node->index);
			}

			 TODO If there is only sharers, should one of them
			       send the data to mod instead of having target_mod do it?

			 Send read request to owners other than mod for all sub-blocks.
			for (z = 0; z < dir->zsize; z++)
			{
				struct net_node_t *node;

				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;

				 No owner
				if (!DIR_ENTRY_VALID_OWNER(dir_entry))
					continue;

				Owner is mod
				if (dir_entry->owner == mod->low_net_node->index)
					continue;

				Get owner mod
				node = list_get(target_mod->high_net->node_list, dir_entry->owner);
				assert(node->kind == net_node_end);
				owner = node->user_data;
				assert(owner);

				Not the first sub-block
				if (dir_entry_tag % owner->block_size)
					continue;

				Send read request
				stack->pending++;
				new_stack = mod_stack_create(stack->id, target_mod, dir_entry_tag,
						EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack);
				Only set peer if its a subblock that was requested
				if (dir_entry_tag >= stack->addr && 
						dir_entry_tag < stack->addr + mod->block_size)
				{
					new_stack->peer = mod_stack_set_peer(mod, stack->state);
				}
				new_stack->target_mod = owner;
				new_stack->request_dir = mod_request_down_up;
				esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);
			}
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack, 0);

			 * The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may have been a miss.
			 * TODO: I'm not sure how relavant this is here for all states. *
			prefetcher_access_hit(stack, target_mod);
			 */
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH_WT, stack, 0);
			return;
		}
		else
		{
			/* State = I */
			assert(!dir_entry_group_shared_or_owned(target_mod->dir,
					stack->set, stack->way));
			new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
					EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS_WT, stack);
			/* Peer is NULL since we keep going up-down */
			new_stack->target_mod = mod_get_low_mod(target_mod, stack->tag);
			new_stack->request_dir = mod_request_up_down;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_WT, new_stack, 0);

			/* The prefetcher may be interested in this miss */
			prefetcher_access_miss(stack, target_mod);

		}
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS_WT)
	{
		mem_debug("  %lld %lld 0x%x %s read request updown miss\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_updown_miss\"\n",
				stack->id, target_mod->name);

		/* Check error */
		if (stack->err)
		{
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
			ret->err = 1;
			mod_stack_set_reply(ret, reply_ack_error);
			stack->reply_size = 8;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY_WT, stack, 0);
			return;
		}

		/* Set block state to excl/shared depending on the return value 'shared'
		 * that comes from a read request into the next cache level.
		 * Also set the tag of the block. */
		cache_set_block(target_mod->cache, stack->set, stack->way, stack->tag,
				//				stack->shared ? cache_block_shared : cache_block_exclusive);
				cache_block_exclusive);
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH_WT)
	{
		//		int shared;

		/* Ensure that a reply was received */
		assert(stack->reply);

		/* Ignore while pending requests */
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;

		/* Trace */
		mem_debug("  %lld %lld 0x%x %s read request updown finish\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_updown_finish\"\n",
				stack->id, target_mod->name);

		/* If blocks were sent directly to the peer, the reply size would
		 * have been decreased.  Based on the final size, we can tell whether
		 * to send more data or simply ack */
		if (stack->reply_size == 8)
		{
			mod_stack_set_reply(ret, reply_ack);
		}
		else if (stack->reply_size > 8)
		{
			mod_stack_set_reply(ret, reply_ack_data);
		}
		else
		{
			fatal("Invalid reply size: %d", stack->reply_size);
		}

		dir = target_mod->dir;

		//		shared = 0;
		/* With the Owned state, the directory entry may remain owned by the sender *
		if (!stack->retain_owner)
		{
		 * Set owner to 0 for all directory entries not owned by mod. *
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				if (dir_entry->owner != mod->low_net_node->index)
					dir_entry_set_owner(dir, stack->set, stack->way, z, 
							DIR_ENTRY_OWNER_NONE);
			}
		}

		 * For each sub-block requested by mod, set mod as sharer, and
		 * check whether there is other cache sharing it. *
		for (z = 0; z < dir->zsize; z++)
		{
			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
				continue;
			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
			dir_entry_set_sharer(dir, stack->set, stack->way, z, mod->low_net_node->index);
			if (dir_entry->num_sharers > 1 || stack->nc_write || stack->shared)
				shared = 1;

		 * If the block is owned, non-coherent, or shared,
		 * mod (the higher-level cache) should never be exclusive
			if (stack->state == cache_block_owned || 
					stack->state == cache_block_noncoherent ||
					stack->state == cache_block_shared )
				shared = 1;
		}

		 * If no sub-block requested by mod is shared by other cache, set mod
		 * as owner of all of them. Otherwise, notify requester that the block is
		 * shared by setting the 'shared' return value to true.
		ret->shared = shared;
		if (!shared)
		{
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
					continue;
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_set_owner(dir, stack->set, stack->way, z, mod->low_net_node->index);
			}
		}
		 */
		dir_entry_unlock(dir, stack->set, stack->way);

		int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->data_latency;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY_WT, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WT)
	{
		struct mod_t *owner;

		mem_debug("  %lld %lld 0x%x %s read request downup\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_downup\"\n",
				stack->id, target_mod->name);

		/* Check: state must not be invalid or shared.
		 * By default, only one pending request.
		 * Response depends on state */
		assert(stack->state != cache_block_invalid);
		assert(stack->state != cache_block_shared);
		assert(stack->state != cache_block_noncoherent);
		stack->pending = 1;

		/* Send a read request to the owner of each subblock. */
		dir = target_mod->dir;
		for (z = 0; z < dir->zsize; z++)
		{
			struct net_node_t *node;

			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			assert(dir_entry_tag < stack->tag + target_mod->block_size);
			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);

			/* No owner */
			if (!DIR_ENTRY_VALID_OWNER(dir_entry))
				continue;

			/* Get owner mod */
			node = list_get(target_mod->high_net->node_list, dir_entry->owner);
			assert(node && node->kind == net_node_end);
			owner = node->user_data;

			/* Not the first sub-block */
			if (dir_entry_tag % owner->block_size)
				continue;

			stack->pending++;
			new_stack = mod_stack_create(stack->id, target_mod, dir_entry_tag,
					EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS_WT, stack);
			new_stack->target_mod = owner;
			new_stack->request_dir = mod_request_down_up;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_WT, new_stack, 0);
		}

		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS_WT)
	{
		/* Ignore while pending requests */
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;

		mem_debug("  %lld %lld 0x%x %s read request downup wait for reqs\n",
				esim_time, stack->id, stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_downup_wait_for_reqs\"\n",
				stack->id, target_mod->name);

		if (stack->peer)
		{
			/* Send this block (or subblock) to the peer */
			new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
					EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH_WT, stack);
			new_stack->peer = mod_stack_set_peer(stack->peer, stack->state);
			new_stack->target_mod = stack->target_mod;
			esim_schedule_event(EV_MOD_NMOESI_PEER_SEND_WT, new_stack, 0);
		}
		else
		{
			/* No data to send to peer, so finish */
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH_WT, stack, 0);
		}

		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s read request downup finish\n",
				esim_time, stack->id, stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_downup_finish\"\n",
				stack->id, target_mod->name);

		if (stack->reply == reply_ack_data)
		{
			/* If data was received, it was owned or modified by a higher level cache.
			 * We need to continue to propagate it up until a peer is found */

			if (stack->peer)
			{
				/* Peer was found, so this directory entry should be changed
				 * to owned */
				cache_set_block(target_mod->cache, stack->set, stack->way,
						stack->tag, cache_block_owned);

				/* Higher-level cache changed to shared, set owner of
				 * sub-blocks to NONE. */
				dir = target_mod->dir;
				for (z = 0; z < dir->zsize; z++)
				{
					dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
					assert(dir_entry_tag < stack->tag + target_mod->block_size);
					dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
					dir_entry_set_owner(dir, stack->set, stack->way, z,
							DIR_ENTRY_OWNER_NONE);
				}

				stack->reply_size = 8;
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);

				/* Decrease the amount of data that mod will have to send back
				 * to its higher level cache */
				ret->reply_size -= target_mod->block_size;
				assert(ret->reply_size >= 8);

				/* Let the lower-level cache know not to delete the owner */
				ret->retain_owner = 1;
			}
			else
			{
				/* Set state to shared */
				cache_set_block(target_mod->cache, stack->set, stack->way,
						stack->tag, cache_block_shared);

				/* State is changed to shared, set owner of sub-blocks to 0. */
				dir = target_mod->dir;
				for (z = 0; z < dir->zsize; z++)
				{
					dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
					assert(dir_entry_tag < stack->tag + target_mod->block_size);
					dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
					dir_entry_set_owner(dir, stack->set, stack->way, z,
							DIR_ENTRY_OWNER_NONE);
				}

				stack->reply_size = target_mod->block_size + 8;
				mod_stack_set_reply(ret, reply_ack_data);
			}
		}
		else if (stack->reply == reply_ack)
		{
			/* Higher-level cache was exclusive with no modifications above it */
			stack->reply_size = 8;

			/* Set state to shared */
			cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_shared);

			/* State is changed to shared, set owner of sub-blocks to 0. */
			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				assert(dir_entry_tag < stack->tag + target_mod->block_size);
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_set_owner(dir, stack->set, stack->way, z,
						DIR_ENTRY_OWNER_NONE);
			}

			if (stack->peer)
			{
				stack->reply_size = 8;
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);

				/* Decrease the amount of data that mod will have to send back
				 * to its higher level cache */
				ret->reply_size -= target_mod->block_size;
				assert(ret->reply_size >= 8);
			}
			else
			{
				mod_stack_set_reply(ret, reply_ack);
				stack->reply_size = 8;
			}
		}
		else if (stack->reply == reply_none)
		{
			/* This block is not present in any higher level caches */

			if (stack->peer)
			{
				stack->reply_size = 8;
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);

				/* Decrease the amount of data that mod will have to send back
				 * to its higher level cache */
				ret->reply_size -= target_mod->sub_block_size;
				assert(ret->reply_size >= 8);

				if (stack->state == cache_block_modified ||
						stack->state == cache_block_owned)
				{
					/* Let the lower-level cache know not to delete the owner */
					ret->retain_owner = 1;

					/* Set block to owned */
					cache_set_block(target_mod->cache, stack->set, stack->way,
							stack->tag, cache_block_owned);
				}
				else
				{
					/* Set block to shared */
					cache_set_block(target_mod->cache, stack->set, stack->way,
							stack->tag, cache_block_shared);
				}
			}
			else 
			{
				if (stack->state == cache_block_exclusive ||
						stack->state == cache_block_shared)
				{
					stack->reply_size = 8;
					mod_stack_set_reply(ret, reply_ack);

				}
				else if (stack->state == cache_block_owned ||
						stack->state == cache_block_modified ||
						stack->state == cache_block_noncoherent)
				{
					/* No peer exists, so data is returned to mod */
					stack->reply_size = target_mod->sub_block_size + 8;
					mod_stack_set_reply(ret, reply_ack_data);
				}
				else
				{
					fatal("Invalid cache block state: %d\n", stack->state);
				}

				/* Set block to shared */
				cache_set_block(target_mod->cache, stack->set, stack->way,
						stack->tag, cache_block_shared);
			}
		}
		else
		{
			fatal("Unexpected reply type: %d\n", stack->reply);
		}


		dir_entry_unlock(target_mod->dir, stack->set, stack->way);

		int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->data_latency;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY_WT, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_REPLY_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s read request reply\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_reply\"\n",
				stack->id, target_mod->name);

		/* Checks */
		assert(stack->reply_size);
		assert(stack->request_dir);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
				mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = target_mod->high_net_node;
			dst_node = mod->low_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = target_mod->low_net_node;
			dst_node = mod->high_net_node;
		}

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
				EV_MOD_NMOESI_READ_REQUEST_FINISH_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s read request finish\n", esim_time, stack->id,
				stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_finish\"\n",
				stack->id, mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(mod->low_net, mod->low_net_node, stack->msg);
		else
			net_receive(mod->high_net, mod->high_net_node, stack->msg);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_write_data_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;

	if (event == EV_MOD_NMOESI_WRITE_DATA_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s write data \n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request\"\n",
				stack->id, mod->name);

		/* Default return values */
		assert(ret);
		ret->err = 0;

		/* For write data, we need to set the initial reply size
		 */
		stack->reply_size = 8;
		mod_stack_set_reply(stack, reply_ack);

		/* Checks */
		assert(stack->request_dir);
		//		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
		//			stack->request_dir == mod_request_down_up);
		assert(mod_get_low_mod(target_mod, stack->addr) == mod ||
				stack->request_dir == mod_request_up_down);

		/* Get source and destination nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = mod->low_net_node;
			dst_node = target_mod->high_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = mod->high_net_node;
			dst_node = target_mod->low_net_node;

			mem_debug("  XXX %lld %lld 0x%x %s request down_up\n", esim_time, stack->id,
					stack->addr, mod->name);
		}

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, mod->block_size + 8,
				EV_MOD_NMOESI_WRITE_DATA_RECEIVE_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_RECEIVE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write data receive\n", esim_time, stack->id,
				stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_receive\"\n",
				stack->id, target_mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);
		else
		{
			mem_debug("XXX %lld %lld 0x%x %s write data request down-up -- error\n", esim_time, stack->id,
					stack->addr, target_mod->name);
			net_receive(target_mod->low_net, target_mod->low_net_node, stack->msg);
		}


		/* Find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
				EV_MOD_NMOESI_WRITE_DATA_ACTION_WT, stack);
		new_stack->blocking = 1;
		new_stack->nc_write = 1;
		new_stack->retry = 0;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_WRITE_WT, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_ACTION_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write data action\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_action\"\n",
				stack->id, target_mod->name);

		/* Check lock error. If write request is down-up, there should
		 * have been no error. */
		if (stack->err)
		{
			mem_debug("XXX %lld %lld 0x%x %s write data action\n", esim_time, stack->id,
					stack->tag, target_mod->name);
			assert(stack->request_dir == mod_request_up_down);
			ret->err = 1;
			stack->reply_size = 8;
			esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_REPLY_WT, stack, 0);
			return;
		}

		stack->pending = 1;
		/* Main memory modules are a special case */
		if (target_mod->kind == mod_kind_main_memory)
		{
			assert(stack->state);
			/* For non-coherent stores, finding an E or M for the state of
			 * a cache block in the directory still requires a message to
			 * the lower-level module so it can update its owner field.
			 * These messages should not be sent if the module is a main
			 * memory module. */
			stack->reply_size = 8;
			esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_BLOCK_WT, stack, 0);
			return;
		}

		/* In either case, hit or miss we are initiating a write-request to the
		 * lower mods.
		 */
		new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
				EV_MOD_NMOESI_WRITE_DATA_LOWER_WT, stack);
		new_stack->nc_write = 1;
		new_stack->target_mod = mod_get_low_mod(target_mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_WT, new_stack, 0);
		stack->pending++;

		esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_BLOCK_WT, stack, 0);

		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_LOWER_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write data to the lower done \n", esim_time, stack->id,
				stack->tag, target_mod->name);

		/* Check lock error. If write request is down-up, there should
		 * have been no error. */
		if (stack->err)
		{
			mem_debug("XXX %lld %lld 0x%x %s write data to lower -- error \n", esim_time, stack->id,
					stack->tag, target_mod->name);
		}
		esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_DONE_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_BLOCK_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write data block\n", esim_time, stack->id,
				stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_unlock\"\n",
				stack->id, target_mod->name);

		/* Set block state to excl/shared depending on return var 'shared'.
		 * Also set the tag of the block. */
		if (stack->state)
		{
			mem_debug("  %lld %lld 0x%x %s write - block exists \n", esim_time, stack->id,
					stack->addr, target_mod->name);

			cache_set_block(target_mod->cache, stack->set, stack->way, stack->tag,
					cache_block_noncoherent);

			/* Unlock directory entry */
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
			esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_DONE_WT, stack,
					target_mod->data_latency);
			return;
		}
		/* Impose the access latency before continuing */
		esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_DONE_WT, stack,
				0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_DONE_WT)
	{
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;

		mem_debug("%lld %lld 0x%x %s write-data finish\n", esim_time, stack->id,
				stack->addr, target_mod->name);

		assert (!stack->err);
		assert (stack->reply_size == 8);
		mod_stack_set_reply(ret, reply_ack);

		esim_schedule_event(EV_MOD_NMOESI_WRITE_DATA_REPLY_WT, stack,
				0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_REPLY_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s write data reply\n", esim_time, stack->id,
				stack->tag, target_mod->name);

		/* Checks */
		assert(stack->reply_size);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
				mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = target_mod->high_net_node;
			dst_node = mod->low_net_node;
		}
		else
		{
			mem_debug(" XXX %lld %lld 0x%x %s write data down-up\n", esim_time, stack->id,
					stack->tag, target_mod->name);

			net = mod->high_net;
			src_node = target_mod->low_net_node;
			dst_node = mod->high_net_node;
		}

		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
				EV_MOD_NMOESI_WRITE_DATA_FINISH_WT, stack, event, stack);
		return;

	}

	if (event == EV_MOD_NMOESI_WRITE_DATA_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write data finish\n", esim_time, stack->id,
				stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_finish\"\n",
				stack->id, mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
		{
			net_receive(mod->low_net, mod->low_net_node, stack->msg);
		}
		else
		{
			net_receive(mod->high_net, mod->high_net_node, stack->msg);
		}


		/* Return */
		mod_stack_return(stack);
		return;

	}

	abort();
}

void mod_handler_nmoesi_write_request_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s write request\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request\"\n",
				stack->id, mod->name);

		/* Default return values */
		assert(ret);
		ret->err = 0;

		/* For write requests, we need to set the initial reply size because
		 * in updown, peer transfers must be allowed to decrease this value
		 * (during invalidate). If the request turns out to be downup, then
		 * these values will get overwritten. */
		stack->reply_size = mod->block_size + 8;
		mod_stack_set_reply(stack, reply_ack_data);

		/* Checks */
		assert(stack->request_dir);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
				stack->request_dir == mod_request_down_up);
		assert(mod_get_low_mod(target_mod, stack->addr) == mod ||
				stack->request_dir == mod_request_up_down);

		/* Get source and destination nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = mod->low_net_node;
			dst_node = target_mod->high_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = mod->high_net_node;
			dst_node = target_mod->low_net_node;
		}

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, 8,
				EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request receive\n", esim_time, stack->id,
				stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_receive\"\n",
				stack->id, target_mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);
		else
			net_receive(target_mod->low_net, target_mod->low_net_node, stack->msg);

		/* Find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
				EV_MOD_NMOESI_WRITE_REQUEST_ACTION_WT, stack);
		new_stack->blocking = stack->request_dir == mod_request_down_up;
		new_stack->nc_write = 1;
		new_stack->retry = 0;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_WRITE_WT, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_ACTION_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request action\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_action\"\n",
				stack->id, target_mod->name);

		/* Check lock error. If write request is down-up, there should
		 * have been no error. */
		if (stack->err)
		{
			assert(stack->request_dir == mod_request_up_down);
			ret->err = 1;
			stack->reply_size = 8;
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY_WT, stack, 0);
			return;
		}

		/* Invalidate the rest of upper level sharers
		new_stack = mod_stack_create(stack->id, target_mod, 0,
			EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE, stack);
		new_stack->except_mod = mod;
		new_stack->set = stack->set;
		new_stack->way = stack->way;
		new_stack->peer = mod_stack_set_peer(stack->peer, stack->state);
		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE, new_stack, 0);*/
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request exclusive\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_exclusive\"\n",
				stack->id, target_mod->name);

		if (stack->request_dir == mod_request_up_down)
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_WT, stack, 0);
		else
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request updown\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_updown\"\n",
				stack->id, target_mod->name);

		stack->pending = 1;
		/* Main memory modules are a special case */
		if (mod->kind == mod_kind_main_memory)
		{
			/* For non-coherent stores, finding an E or M for the state of
			 * a cache block in the directory still requires a message to
			 * the lower-level module so it can update its owner field.
			 * These messages should not be sent if the module is a main
			 * memory module. */
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH_WT, stack, 0);
			return;
		}

		/* In either case, hit or miss we are initiating a write-request to the
		 * lower mods.
		 */
		new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
				EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH_WT, stack);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->nc_write = 1;
		new_stack->target_mod = mod_get_low_mod(target_mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_WT, new_stack, 0);
		stack->pending++;

		/*
		new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
			ESIM_EV_NONE, NULL);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->target_mod = mod_get_low_mod(target_mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST, new_stack, 0);
		 */
		/* state = M/E */

		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH_WT, stack, 0);

		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request updown finish\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_updown_finish\"\n",
				stack->id, target_mod->name);

		/* Ensure that a reply was received */
		assert(stack->reply);

		/* Error in write request to next cache level */
		if (stack->err)
		{
			ret->err = 1;
			mod_stack_set_reply(ret, reply_ack_error);
			stack->reply_size = 8;
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY_WT, stack, 0);
			return;
		}

		/* Check that addr is a multiple of mod.block_size.
		 * Set mod as sharer and owner. */
		if (stack->state)
		{
			/*			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				assert(stack->addr % mod->block_size == 0);
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				assert(dir_entry_tag < stack->tag + target_mod->block_size);
				if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
					continue;
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_set_sharer(dir, stack->set, stack->way, z, mod->low_net_node->index);
				dir_entry_set_owner(dir, stack->set, stack->way, z, mod->low_net_node->index);
				assert(dir_entry->num_sharers == 1);
			}
			 */
			/* Set state to exclusive */
			cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_noncoherent);
			/* If blocks were sent directly to the peer, the reply size would
			 * have been decreased.  Based on the final size, we can tell whether
			 * to send more data up or simply ack */
			if (stack->reply_size == 8)
			{
				mod_stack_set_reply(ret, reply_ack);
			}
			else if (stack->reply_size > 8)
			{
				mod_stack_set_reply(ret, reply_ack_data);
			}
			else
			{
				fatal("Invalid reply size: %d", stack->reply_size);
			}

			/* Unlock, reply_size is the data of the size of the requester's block. */
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
		}

		int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->data_latency;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY_WT, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request downup\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_downup\"\n",
				stack->id, target_mod->name);

		assert(stack->state != cache_block_invalid);
		assert(!dir_entry_group_shared_or_owned(target_mod->dir, stack->set, stack->way));

		/* Compute reply size */
		if (stack->state == cache_block_exclusive ||
				stack->state == cache_block_shared)
		{
			/* Exclusive and shared states send an ack */
			stack->reply_size = 8;
			mod_stack_set_reply(ret, reply_ack);
		}
		else if (stack->state == cache_block_noncoherent)
		{
			/* Non-coherent state sends data */
			stack->reply_size = target_mod->block_size + 8;
			mod_stack_set_reply(ret, reply_ack_data);
		}
		else if (stack->state == cache_block_modified ||
				stack->state == cache_block_owned)
		{
			if (stack->peer)
			{
				/* Modified or owned entries send data directly to peer
				 * if it exists */
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);
				stack->reply_size = 8;

				/* This control path uses an intermediate stack that disappears, so
				 * we have to update the return stack of the return stack */
				ret->ret_stack->reply_size -= target_mod->block_size;
				assert(ret->ret_stack->reply_size >= 8);

				/* Send data to the peer */
				new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
						EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH_WT, stack);
				new_stack->peer = mod_stack_set_peer(stack->peer, stack->state);
				new_stack->target_mod = stack->target_mod;

				esim_schedule_event(EV_MOD_NMOESI_PEER_SEND_WT, new_stack, 0);
				return;
			}
			else
			{
				/* If peer does not exist, data is returned to mod */
				mod_stack_set_reply(ret, reply_ack_data);
				stack->reply_size = target_mod->block_size + 8;
			}
		}
		else
		{
			fatal("Invalid cache block state: %d\n", stack->state);
		}

		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH_WT, stack, 0);

		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request downup complete\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_downup_finish\"\n",
				stack->id, target_mod->name);

		/* Set state to I, unlock*/
		cache_set_block(target_mod->cache, stack->set, stack->way, 0, cache_block_invalid);
		dir_entry_unlock(target_mod->dir, stack->set, stack->way);

		int latency = ret->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->data_latency;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY_WT, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_REPLY_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s write request reply\n", esim_time, stack->id,
				stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_reply\"\n",
				stack->id, target_mod->name);

		/* Checks */
		assert(stack->reply_size);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
				mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = target_mod->high_net_node;
			dst_node = mod->low_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = target_mod->low_net_node;
			dst_node = mod->high_net_node;
		}

		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
				EV_MOD_NMOESI_WRITE_REQUEST_FINISH_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s write request finish\n", esim_time, stack->id,
				stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_finish\"\n",
				stack->id, mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
		{
			net_receive(mod->low_net, mod->low_net_node, stack->msg);
		}
		else
		{
			net_receive(mod->high_net, mod->high_net_node, stack->msg);
		}


		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_peer_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_t *src = stack->target_mod;
	struct mod_t *peer = stack->peer;

	if (event == EV_MOD_NMOESI_PEER_SEND_WT)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer send\n", esim_time, stack->id,
				stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer\"\n",
				stack->id, src->name);

		/* Send message from src to peer */
		stack->msg = net_try_send_ev(src->low_net, src->low_net_node, peer->low_net_node,
				src->block_size + 8, EV_MOD_NMOESI_PEER_RECEIVE_WT, stack, event, stack);

		return;
	}

	if (event == EV_MOD_NMOESI_PEER_RECEIVE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer receive\n", esim_time, stack->id,
				stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer_receive\"\n",
				stack->id, peer->name);

		/* Receive message from src */
		net_receive(peer->low_net, peer->low_net_node, stack->msg);

		esim_schedule_event(EV_MOD_NMOESI_PEER_REPLY_WT, stack, 0);

		return;
	}

	if (event == EV_MOD_NMOESI_PEER_REPLY_WT)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer reply ack\n", esim_time, stack->id,
				stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer_reply_ack\"\n",
				stack->id, peer->name);

		/* Send ack from peer to src */
		stack->msg = net_try_send_ev(peer->low_net, peer->low_net_node, src->low_net_node,
				8, EV_MOD_NMOESI_PEER_FINISH_WT, stack, event, stack);

		return;
	}

	if (event == EV_MOD_NMOESI_PEER_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer finish\n", esim_time, stack->id,
				stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer_finish\"\n",
				stack->id, src->name);

		/* Receive message from src */
		net_receive(src->low_net, src->low_net_node, stack->msg);

		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_message_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;

	struct dir_t *dir;
	struct dir_entry_t *dir_entry;
	uint32_t z;

	if (event == EV_MOD_NMOESI_MESSAGE_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s message\n", esim_time, stack->id,
				stack->addr, mod->name);

		stack->reply_size = 8;
		stack->reply = reply_ack;

		/* Default return values*/
		ret->err = 0;

		/* Checks */
		assert(stack->message);

		/* Get source and destination nodes */
		net = mod->low_net;
		src_node = mod->low_net_node;
		dst_node = target_mod->high_net_node;

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, 8,
				EV_MOD_NMOESI_MESSAGE_RECEIVE_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_RECEIVE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s message receive\n", esim_time, stack->id,
				stack->addr, target_mod->name);

		/* Receive message */
		net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);

		/* Find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
				EV_MOD_NMOESI_MESSAGE_ACTION_WT, stack);
		new_stack->message = stack->message;
		new_stack->blocking = 0;
		new_stack->retry = 0;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_READ_WT, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_ACTION_WT)
	{
		mem_debug("  %lld %lld 0x%x %s clear owner action\n", esim_time, stack->id,
				stack->tag, target_mod->name);

		assert(stack->message);

		/* Check block locking error. */
		mem_debug("stack err = %u\n", stack->err);
		if (stack->err)
		{
			ret->err = 1;
			mod_stack_set_reply(ret, reply_ack_error);
			esim_schedule_event(EV_MOD_NMOESI_MESSAGE_REPLY_WT, stack, 0);
			return;
		}

		if (stack->message == message_clear_owner)
		{
			/* Remove owner */
			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				/* Skip other subblocks */
				if (stack->addr == stack->tag + z * target_mod->sub_block_size)
				{
					/* Clear the owner */
					dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
					assert(dir_entry->owner == mod->low_net_node->index);
					dir_entry_set_owner(dir, stack->set, stack->way, z,
							DIR_ENTRY_OWNER_NONE);
				}
			}

		}
		else
		{
			fatal("Unexpected message");
		}

		/* Unlock the directory entry */
		dir_entry_unlock(dir, stack->set, stack->way);

		esim_schedule_event(EV_MOD_NMOESI_MESSAGE_REPLY_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_REPLY_WT)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s message reply\n", esim_time, stack->id,
				stack->tag, target_mod->name);

		/* Checks */
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
				mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		net = mod->low_net;
		src_node = target_mod->high_net_node;
		dst_node = mod->low_net_node;

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
				EV_MOD_NMOESI_MESSAGE_FINISH_WT, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_FINISH_WT)
	{
		mem_debug("  %lld %lld 0x%x %s message finish\n", esim_time, stack->id,
				stack->tag, mod->name);

		/* Receive message */
		net_receive(mod->low_net, mod->low_net_node, stack->msg);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_find_and_lock_write_wt(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	//	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_WRITE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock write-through(blocking=%d)\n",
				esim_time, stack->id, stack->addr, mod->name, stack->blocking);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock\"\n",
				stack->id, mod->name);

		/* Default return values */
		ret->err = 0;

		/* If this stack has already been assigned a way, keep using it */
		stack->way = ret->way;

		/* Get a port */
		mod_lock_port(mod, stack, EV_MOD_NMOESI_FIND_AND_LOCK_PORT_WRITE_WT);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_PORT_WRITE_WT)
	{
		struct mod_port_t *port = stack->port;
		struct dir_lock_t *dir_lock;

		assert(stack->port);
		mem_debug("  %lld %lld 0x%x %s find and lock port walk through\n", esim_time, stack->id,
				stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_port\"\n",
				stack->id, mod->name);

		/* Statistics */
		mod->accesses++;
		if (stack->retry)
			mod->retry_accesses++;

		/* Set parent stack flag expressing that port has already been locked.
		 * This flag is checked by new writes to find out if it is already too
		 * late to coalesce. */
		ret->port_locked = 1;

		/* Look for block. */
		stack->hit = mod_find_block(mod, stack->addr, &stack->set,
				&stack->way, &stack->tag, &stack->state);

		/* Debug */
		if (stack->hit)
			mem_debug("    %lld 0x%x %s hit: set=%d, way=%d, state=%s\n", stack->id,
					stack->tag, mod->name, stack->set, stack->way,
					str_map_value(&cache_block_state_map, stack->state));

		/* Statistics - REMOVED by KAVI*/

		/* For write-through this shouldn't happen. In case of a miss
		 * we would carry on without returning a cache block unless it is
		 * a main memory
		 */
		if (mod->kind == mod_kind_main_memory)
		{
			if (!stack->hit)
			{
				// Find victim
				if (stack->way < 0)
				{
					stack->way = cache_replace_block(mod->cache, stack->set);
				}
			}
		}
		if (stack->way >= 0)
		{
			/* If directory entry is locked and the call to FIND_AND_LOCK is not
			 * blocking, release port and return error. */
			dir_lock = dir_lock_get(mod->dir, stack->set, stack->way);
			if (dir_lock->lock && !stack->blocking)
			{
				mem_debug("    %lld 0x%x %s block locked at set=%d, way=%d by A-%lld - aborting\n",
						stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
				ret->err = 1;
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				mod_stack_return(stack);
				return;
			}

			/* Lock directory entry. If lock fails, port needs to be released to prevent
			 * deadlock.  When the directory entry is released, locking port and
			 * directory entry will be retried. */
			mem_debug("    %lld 0x%x %s locking directory at set=%d, way=%d by A-%lld -- should be hit or main-memory\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
			if (!dir_entry_lock(mod->dir, stack->set, stack->way, EV_MOD_NMOESI_FIND_AND_LOCK_WRITE_WT,
					stack))
			{
				mem_debug("    %lld 0x%x %s block locked at set=%d, way=%d by A-%lld - waiting\n",
						stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				return;
			}
		}

		/* Miss */
		if (mod->kind != mod_kind_main_memory && !stack->hit)
		{
			mem_debug("    %lld 0x%x %s miss in write-through set=%d\n",
					stack->id, stack->tag, mod->name, stack->set);

		}

		if (mod->kind == mod_kind_main_memory && !stack->hit)
		{
			/* Find victim */
			assert(stack->state || !dir_entry_group_shared_or_owned(mod->dir,
					stack->set, stack->way));
			mem_debug("    %lld 0x%x %s miss -> main-memory: set=%d, way=%d, state=%s\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way,
					str_map_value(&cache_block_state_map, stack->state));

			/* resolve conflict */
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);

			mem_debug("    %lld 0x%x %s conflict resolved -> lru in main-memory: set=%d, way=%d, state=%s\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way,
					str_map_value(&cache_block_state_map, stack->state));

		}

		if (stack->way >= 0)
		{
			/* Entry is locked. Record the transient tag so that a subsequent lookup
			 * detects that the block is being brought.
			 * Also, update LRU counters here. */
			cache_set_transient_tag(mod->cache, stack->set, stack->way, stack->tag);
			cache_access_block(mod->cache, stack->set, stack->way);
		}

		/* Access latency */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_ACTION_WRITE_WT, stack, mod->dir_latency);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_ACTION_WRITE_WT)
	{
		/*		struct mod_port_t *port = stack->port;

		assert(port);*/
		struct mod_port_t *port = stack->port;

		assert(port);
		mem_debug(" %lld %lld 0x%x %s find and lock action write-through\n", esim_time, stack->id,
				stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_action\"\n",
				stack->id, mod->name);

		/* Release port */
		mod_unlock_port(mod, port, stack);
		ret->port_locked = 0;
		/* On miss, evict if victim is a valid block. */
		if (!stack->hit && stack->state)
		{
			stack->state = cache_block_invalid;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_FINISH_WRITE_WT, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_FINISH_WRITE_WT)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock finish write-through(err=%d)\n", esim_time, stack->id,
				stack->tag, mod->name, stack->err);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_finish\"\n",
				stack->id, mod->name);

		/* If evict produced err, return err */
		if (stack->err)
		{
			mem_debug("  shouldn't error: %lld %lld 0x%x %s find and lock finish (err=%d)\n", esim_time, stack->id,
					stack->tag, mod->name, stack->err);
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state);
			assert(stack->eviction);
			ret->err = 1;
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mod_stack_return(stack);
			return;
		}

		/* Eviction
		if (stack->eviction)
		{
			mod->evictions++;
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(!stack->state);
		}*/

		/* If this is a main memory, the block is here. A previous miss was just a miss
		 * in the directory. */
		if (mod->kind == mod_kind_main_memory && !stack->state)
		{
			stack->state = cache_block_exclusive;
			cache_set_block(mod->cache, stack->set, stack->way,
					stack->tag, stack->state);
		}

		/* Return */
		ret->err = 0;
		ret->set = stack->set;
		ret->way = stack->way;
		ret->state = stack->state;
		ret->tag = stack->tag;
		mod_stack_return(stack);
		return;
	}

	abort();
}
