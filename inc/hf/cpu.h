/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hf/arch/cpu.h"

#include "hf/addr.h"
#include "hf/spinlock.h"

#include "vmapi/hf/types.h"

/** The number of bits in each element of the interrupt bitfields. */
#define INTERRUPT_REGISTER_BITS 32

enum vcpu_state {
	/** The vcpu is switched off. */
	VCPU_STATE_OFF,

	/** The vcpu is ready to be run. */
	VCPU_STATE_READY,

	/** The vcpu is waiting for a message. */
	VCPU_STATE_BLOCKED_MAILBOX,

	/** The vcpu is waiting for an interrupt. */
	VCPU_STATE_BLOCKED_INTERRUPT,

	/** The vcpu has aborted. */
	VCPU_STATE_ABORTED,
};

struct interrupts {
	/** Bitfield keeping track of which interrupts are enabled. */
	uint32_t interrupt_enabled[HF_NUM_INTIDS / INTERRUPT_REGISTER_BITS];
	/** Bitfield keeping track of which interrupts are pending. */
	uint32_t interrupt_pending[HF_NUM_INTIDS / INTERRUPT_REGISTER_BITS];
	/**
	 * The number of interrupts which are currently both enabled and
	 * pending. i.e. the number of bits set in interrupt_enable &
	 * interrupt_pending.
	 */
	uint32_t enabled_and_pending_count;
};

struct vcpu_fault_info {
	ipaddr_t ipaddr;
	vaddr_t vaddr;
	vaddr_t pc;
	int mode;
};

struct vcpu {
	/*
	 * Protects accesses to vCPU's state and architecture registers. If a
	 * vCPU is running, its execution lock is logically held by the
	 * running pCPU.
	 */
	struct spinlock execution_lock;
	
	
	/*
	 * Protects accesses to vCPU's interrupts.
	 */
	struct spinlock interrupts_lock;

	/*
	 * The state is only changed in the context of the vCPU being run. This
	 * ensures the scheduler can easily keep track of the vCPU state as
	 * transitions are indicated by the return code from the run call.
	 */
	enum vcpu_state state;

	struct cpu *cpu;
	struct vm *vm;
	struct arch_regs regs;
	struct interrupts interrupts;
};

/** Encapsulates a vCPU whose execution lock is held. */
struct vcpu_execution_locked {
	struct vcpu *vcpu;
};

/* TODO: Update alignment such that cpus are in different cache lines. */
struct cpu {
	/** CPU identifier. Doesn't have to be contiguous. */
	cpu_id_t id;

	/** Pointer to bottom of the stack. */
	void *stack_bottom;

	/** See api.c for the partial ordering on locks. */
	struct spinlock lock;

	/** Determines whether or not the cpu is currently on. */
	bool is_on;
};

void cpu_module_init(const cpu_id_t *cpu_ids, size_t count);

size_t cpu_index(struct cpu *c);
bool cpu_on(struct cpu *c, ipaddr_t entry, uintreg_t arg);
void cpu_off(struct cpu *c);
struct cpu *cpu_find(cpu_id_t id);

struct vcpu_execution_locked vcpu_lock(struct vcpu *vcpu);
bool vcpu_try_lock(struct vcpu *vcpu, vcpu_execution_locked *locked);
void vcpu_unlock(struct vcpu_execution_locked *locked);
void vcpu_init(struct vcpu *vcpu, struct vm *vm);
void vcpu_on(struct vcpu_execution_locked vcpu, ipaddr_t entry, uintreg_t arg);
spci_vcpu_index_t vcpu_index(const struct vcpu *vcpu);
bool vcpu_is_off(struct vcpu_execution_locked vcpu);
bool vcpu_secondary_reset_and_start(struct vcpu *vcpu, ipaddr_t entry,
				    uintreg_t arg);

bool vcpu_handle_page_fault(const struct vcpu *current,
			    struct vcpu_fault_info *f);
