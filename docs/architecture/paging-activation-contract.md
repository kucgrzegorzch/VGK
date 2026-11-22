# Paging Activation Contract (CR3)

**Status:** design specification only.  
**Applies to:** the native x86-64 kernel paging path.  
**Current implementation state:** page tables may be prepared and validated while inactive. This document does not authorize loading the prepared root into CR3.

## 1. Purpose and safety boundary

VGK currently prepares page-table structures without replacing the firmware-provided active page-table root. The next implementation stage must preserve that separation.

On x86-64 UEFI, the loader and kernel are already executing in 64-bit mode with paging enabled. Loading a new CR3 does not turn paging on: it replaces the active top-level page-table root. Therefore, the transition must be treated as a change of the address-translation environment, not as a routine register initialization.

Until every precondition in this document is implemented and tested, the kernel must continue using the existing active translation environment. No code path may load the prepared root speculatively or as a diagnostic shortcut.

## 2. Required invariants before activation

The activation routine must receive an immutable, validated activation descriptor. It must reject the operation unless all of the following are true:

1. **Root validity**
   - The root physical address is page-aligned.
   - It is representable by the detected CPU physical-address width.
   - The root page and every reachable paging structure lie wholly inside the reserved page-table pool.
   - Every present entry points to a valid, reserved paging-structure page or to a deliberately mapped physical range.
   - Reserved bits and unsupported entry encodings are rejected.

2. **Transition continuity**
   - The instruction immediately following the CR3 write is mapped at the same virtual address under both the old and new roots.
   - The active stack, including the full stack range that may be touched during the transition, is mapped with the required permissions.
   - The current GDT, IDT, TSS, and all descriptor/IST stack ranges that may be used are mapped.
   - The boot information, normalized memory map, page-table pool metadata, and any data accessed before the kernel switches to its post-transition environment remain reachable.
   - Any exception handler that can run during or immediately after the transition has a valid mapped code path and stack. A missing mapping must not be handled by attempting an unsafe recovery through unmapped memory.

3. **Kernel image permissions**
   - Every loaded kernel segment is mapped according to its validated ELF permissions.
   - Writable and executable permissions must never be combined for a page.
   - Page rounding must not cause adjacent segments with different permissions to share a page.
   - The entry point and the complete transition stub are executable.
   - Kernel code, stacks, descriptors, and kernel-owned data are supervisor-only; user-accessible mappings are not introduced by this transition.
   - Read-only data and code are not made writable merely to simplify the transition.

4. **CPU feature and control-register compatibility**
   - The detected physical-address width is used when validating all paging-structure and mapped physical addresses.
   - NX is used only when the CPU supports it and the required EFER.NXE state is established before any NX entry can be relied upon.
   - CR4 features assumed by the generated entries (including PAE and any optional paging mode features) are checked explicitly.
   - PCID, LA57, SMEP, SMAP, and global-page behavior must not be assumed from defaults. The activation design must either preserve compatible current settings or explicitly configure and validate them in a separately reviewed step.
   - The implementation must preserve the required CR0 paging/write-protect invariants and must not clear unrelated firmware-established control bits without an explicit policy.

5. **Paging-mode compatibility**
   - The current VGK page-table builder creates four-level paging structures and validates 48-bit canonical virtual addresses. Its root must not be loaded while CR4.LA57 indicates five-level paging is active.
   - The activation preflight must explicitly verify the active paging mode. If the current environment uses five-level paging, activation must be rejected until VGK has a separately designed, implemented, and tested five-level builder and transition path.
   - LA57 must not be toggled as part of this transition while paging is enabled. Changing paging depth requires a separate architecture-level transition design; it is not an ordinary CR3 update.
   - Required long-mode state, including CR0.PE, CR0.PG, EFER.LMA, EFER.LME, and CR4.PAE, must remain valid throughout the transition. The routine must not attempt to leave or re-enter long mode implicitly.

6. **CR3 encoding and TLB policy**
   - The CR3 value must be constructed from the validated physical root and the explicitly detected current CR4.PCIDE state; it must not be formed by blindly OR-ing arbitrary low bits into the root address.
   - When PCID is disabled, reserved CR3 bits must be zero and the defined PWT/PCD fields must follow an explicit cache policy. When PCID is enabled, the PCID field and the no-flush bit must be handled according to a separately validated policy.
   - The initial activation must not request the CR3 no-flush behavior unless the implementation proves that the selected PCID context and all retained translations are valid for the new root.
   - The transition must define and test how stale non-global translations are invalidated. It must also account for global translations: a CR3 write alone must not be assumed to remove global TLB entries while CR4.PGE is enabled.
   - The implementation must use a documented, CPU-supported invalidation strategy or reject activation when it cannot guarantee that translations from the old environment cannot affect execution under the new root. INVPCID and any PGE changes must be feature-checked and reviewed rather than assumed.

7. **Memory ownership and cache policy**
   - The page-table pool is reserved, normally write-back cacheable, and must not overlap the kernel, boot information, memory map, framebuffer, or any other live object.
   - Page-table pages must not have conflicting cache-type aliases.
   - Every MMIO mapping has passed the existing firmware-range and CPU physical-width checks.
   - MMIO mappings use the approved uncached memory type and NX permissions. No APIC register is accessed merely because its virtual mapping was prepared.
   - No page-table page may be returned to the allocator while it is reachable from the active root.

## 3. Required activation sequence

The future implementation must make the transition small, auditable, and preferably assembly-contained:

1. Finish all allocations, firmware interaction, ACPI parsing, memory-map normalization, and page-table construction before entering the transition stub.
2. Validate the complete activation descriptor and all invariants above while the old root is still active.
3. Disable maskable interrupts for the transition. This does not replace correct exception handling; NMI and machine-check behavior must be considered separately.
4. Ensure the transition stub, its stack, descriptor tables, and required data are mapped under both roots at identical virtual addresses.
5. Load the new CR3 root using an architecture-specific, reviewed primitive and the validated CR3/TLB policy.
6. Execute only instructions and access only data whose mappings and permissions have been proven valid under the new root.
7. Verify the active root and required post-transition invariants before enabling any future interrupt or device path.

The transition must not call firmware services, allocate memory, print through unvalidated device mappings, or depend on a high-level C call chain whose code, stack, and data mappings have not been proven.

## 4. Failure behavior

- **Before the CR3 write:** any failed check aborts activation. The old CR3 remains active, the prepared tables remain inactive, and the caller receives a precise failure result.
- **During/after the CR3 write:** ordinary C cleanup is not a valid rollback strategy. If the new root is invalid, the processor may fault before a software recovery path can execute. Safety therefore depends on complete preflight and on a transition mapping that keeps the fault path valid.
- A future recovery mechanism, if introduced, must be designed and tested independently. This contract does not claim that a failed CR3 transition can be rolled back safely.

## 5. Required tests before implementation can be enabled

At minimum, add tests for:

- root alignment, physical-width overflow, and root outside the reserved pool;
- every paging-structure page being inside the pool and correctly aligned;
- absent, reserved-bit, malformed, and out-of-range entries;
- missing mappings for the transition instruction, stack, GDT, IDT, TSS, IST stacks, boot data, and page-table metadata;
- segment permission enforcement, W^X, and page-boundary collisions;
- NX and optional control-register feature combinations;
- four-level versus five-level paging-mode rejection;
- CR3 field construction with PCID enabled and disabled, including rejection of unsupported or unsafe no-flush requests;
- invalidation of non-global translations and handling of global translations when PGE is enabled;
- MMIO memory-type and NX policy;
- pool exhaustion and all pre-activation failure paths leaving the old root untouched.

Then add controlled QEMU/OVMF tests that actually execute the transition and verify continued execution, exception handling, TLB invalidation, and memory permissions. QEMU success is necessary but not sufficient: a physical-machine test is still required before enabling the feature in a public release.

## 6. Explicit non-goals for the current stage

This specification does not:

- load CR3 or change any control register;
- enable LAPIC, IOAPIC, x2APIC, or interrupts;
- enable SMEP, SMAP, PCID, LA57, or global-page optimizations;
- alter the current UEFI page tables;
- claim that APIC MMIO has been validated on physical hardware;
- authorize merging or activating any existing draft PR.

## 7. Acceptance gate

CR3 activation may be implemented only in a separate change after the code paths, tests, and review demonstrate every invariant above. The activation change must remain isolated from APIC activation and interrupt enablement. It must pass the complete CI suite and dedicated transition tests. Until then, inactive page-table preparation remains the only supported behavior.
