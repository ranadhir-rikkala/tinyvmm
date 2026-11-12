# tinyvmm design

## Goals

- Boot an unmodified x86_64 Linux kernel on KVM with the smallest amount of VMM code that is still correct.
- Timestamp each startup phase so that boot latency can be attributed (VMM work, KVM work, guest kernel work).
- Keep the boot-critical logic (zero page, e820, page tables, GDT, command line, UART) pure and unit tested, so
  most of the VMM can be developed and tested on a machine without `/dev/kvm`.

Non-goals for milestone 1: PCI, ACPI, SMP, migration, security hardening (seccomp, jailer), live devices other than
a serial console.

## Components

| Component | Files | KVM needed? |
|---|---|---|
| Error handling (`Result<T>`, `Status`) | `src/common/result.h` | no |
| RAII fd and mmap | `src/common/unique_fd.h`, `src/common/mapping.h` | no |
| Guest memory | `src/memory/guest_memory.*` | no |
| KVM wrappers (`Kvm`, `Vm`, `Vcpu`, `CpuidTable`) | `src/kvm/kvm.*` | yes |
| x86 vCPU state (CPUID, segments, CRs, FPU, LAPIC) | `src/kvm/x86_cpu.*` | yes |
| GDT encode/decode | `src/x86/gdt.*` | no |
| Identity page tables + software page walk | `src/x86/page_tables.*` | no |
| e820 map | `src/x86/e820.*` | no |
| `boot_params` (zero page) | `src/x86/zero_page.*` | no |
| Kernel command line | `src/boot/cmdline.*` | no |
| Kernel image parsing (ELF vmlinux, bzImage) | `src/boot/kernel_image.*` | no |
| Guest memory layout / Linux loader | `src/boot/linux_loader.*` | no |
| 8250 UART | `src/devices/uart8250.*` | no |
| Boot tracer + streaming marker matcher | `src/trace/*` | no |
| Smoke guests (embedded machine code) | `src/guests/smoke_guests.h`, `src/vmm/smoke.cc` | yes |
| Linux boot + run loop | `src/vmm/linux_boot.cc` | yes (except `--dry-run`) |

The pure library (`tinyvmm_core`) builds on any POSIX host. The KVM library and the `tinyvmm` binary build on
Linux; the x86 vCPU code is compiled only when the host is x86_64, so the project also builds (and its tests run)
on aarch64 Linux.

## Guest physical memory map

| Guest physical address | Contents |
|---|---|
| `0x0000_0500` | GDT: null, null, 64-bit code (`0x10`), data (`0x18`), TSS (`0x20`) |
| `0x0000_0520` | IDT placeholder (8 bytes, limit 7); Linux installs its own |
| `0x0000_1000` | smoke guest code |
| `0x0000_7000` | `struct boot_params` (zero page) |
| `0x0000_8ff0` | initial `rsp` / `rbp` |
| `0x0000_9000` | PML4, then PDPT at `0xa000`, then one page directory per mapped GiB from `0xb000` |
| `0x0002_0000` | kernel command line (NUL terminated) |
| `0x0009_fc00` - `0x0010_0000` | reserved in e820 (EBDA, VGA, BIOS hole) |
| `0x0010_0000` | bzImage protected-mode code (64-bit entry at `+0x200`) |
| `p_paddr` (usually `0x0100_0000`) | ELF vmlinux `PT_LOAD` segments |
| top of RAM, page aligned | initramfs |
| `0xfffb_c000`, `0xfffb_d000` | KVM identity-map and TSS pages (`KVM_SET_IDENTITY_MAP_ADDR`, `KVM_SET_TSS_ADDR`) |

Guest RAM is a single anonymous mapping registered as KVM memory slot 0 at GPA 0. RAM is limited to 3 GiB so it
never overlaps the 32-bit MMIO gap; above that the e820 map would need a hole and a second slot.

## Boot flow

1. `KVM_GET_API_VERSION`, `KVM_CHECK_EXTENSION(KVM_CAP_USER_MEMORY)`, `KVM_GET_VCPU_MMAP_SIZE`.
2. `KVM_CREATE_VM`, `KVM_SET_TSS_ADDR`, `KVM_SET_IDENTITY_MAP_ADDR`.
3. `KVM_CREATE_IRQCHIP` (in-kernel PIC, IOAPIC, LAPIC) and `KVM_CREATE_PIT2` (i8254 timer).
4. `KVM_CREATE_VCPU` and `mmap` of the shared `kvm_run` page.
5. `mmap` guest RAM and `KVM_SET_USER_MEMORY_REGION`.
6. Read and parse the kernel; copy `PT_LOAD` segments (vmlinux) or the protected-mode payload (bzImage); zero BSS.
7. Write the command line, the initramfs, the GDT/IDT, the page tables and the zero page.
8. `KVM_GET_SUPPORTED_CPUID` then `KVM_SET_CPUID2`. This must happen before `KVM_SET_SREGS`: KVM rejects
   `EFER.LME` unless the guest CPUID advertises long mode.
9. `KVM_SET_SREGS` (64-bit `CS`, flat data segments, busy TSS, `CR0 = PE|ET|PG`, `CR3 = PML4`, `CR4 = PAE`,
   `EFER = LME|LMA`), `KVM_SET_REGS` (`rip` = entry, `rsi` = zero page, `rflags = 0x2`), `KVM_SET_FPU`,
   and `KVM_SET_LAPIC` with LINT0 = ExtINT and LINT1 = NMI.
10. Loop on `KVM_RUN` and dispatch exits.

### Kernel formats

- **ELF vmlinux** (what Firecracker boots). Segments are loaded at `p_paddr`; `e_entry` is the physical address of
  `startup_64`. There is no setup header in the file, so the loader synthesizes the fields Linux reads:
  `boot_flag = 0xaa55`, `header = "HdrS"`, `kernel_alignment`, `cmdline_size`, `type_of_loader = 0xff`.
- **bzImage**. The setup header (`0x1f1` up to `0x202 + byte[0x201]`) is copied into the zero page. Requires boot
  protocol >= 2.12 and `XLF_KERNEL_64`. The protected-mode payload starts at `(setup_sects + 1) * 512` and is loaded
  at 1 MiB; the 64-bit entry is at load address + `0x200`. Memory must cover `max(1 MiB, pref_address) + init_size`
  because the decompressor runs in place.

## Exit handling

| Exit | Handling |
|---|---|
| `KVM_EXIT_IO` on `0x3f8`-`0x3ff` | 8250 UART register access |
| `KVM_EXIT_IO` write `0xfe` to `0x64` | guest reset request (`reboot=k`), stop the VM |
| other port reads | return `0xff` (floating bus, "no device") |
| other port writes | ignored (for example port `0x80` POST delays) |
| `KVM_EXIT_MMIO` | reads return `0xff`, writes ignored |
| `KVM_EXIT_HLT` | stop (only reachable without an in-kernel irqchip, i.e. in the smoke guests) |
| `KVM_EXIT_SHUTDOWN` | stop (triple fault) |
| `KVM_EXIT_FAIL_ENTRY` / `KVM_EXIT_INTERNAL_ERROR` | error with the hardware reason / suberror |

Returning `0xff` from unknown ports makes Linux's i8042 probe fail fast instead of waiting on controller timeouts.

### Serial console and interrupts

The UART models the registers Linux's 8250 driver touches: THR/RBR, IER, IIR/FCR, LCR (with DLAB), MCR (with
loopback), LSR, MSR and SCR. Transmit is instantaneous, so LSR always reports THRE|TEMT. The THRE interrupt follows
real 16550 behavior: it becomes pending when THR is written or when IER.THRI goes from 0 to 1, and is cleared by
reading IIR. Linux's `UART_BUG_THRE` probe (enable THRI, read IIR, disable, enable, read IIR again) therefore sees a
working UART and uses interrupt-driven transmit.

The UART exposes an interrupt line level; the VMM converts rising edges into a pulse on IRQ 4
(`KVM_IRQ_LINE` 1 then 0), which suits the edge-triggered ISA interrupt on the in-kernel PIC.

### Stopping the guest

With an in-kernel LAPIC, `HLT` is handled inside KVM and never reaches userspace, so the VMM stops on: the init
marker (with `--exit-on-marker`), an i8042 reset, a triple fault, or `--timeout`. The timeout uses `SIGALRM`; the
handler sets `kvm_run->immediate_exit`, which makes the next or current `KVM_RUN` return `EINTR` without the race of
a signal arriving just before entering the guest.

## Boot-latency tracing

Timestamps use `CLOCK_MONOTONIC`. Each phase records the first time it is reached; the table prints the time since
`vmm_start` and the delta from the previous phase that was reached.

| Phase | Taken when |
|---|---|
| `vmm_start` | first line of `main` (process exec and dynamic linking are not included) |
| `kvm_ready` | VM, irqchip, PIT and vCPU exist |
| `memory_mapped` | guest RAM is mapped and registered with KVM |
| `kernel_loaded` | kernel and initramfs read, parsed and copied; zero page, tables and command line written |
| `first_vcpu_entry` | just before the first `KVM_RUN` (vCPU registers already set) |
| `first_serial_byte` | first byte written to the UART THR |
| `init_marker` | the marker string completes on the serial stream (streaming KMP match) |

`--trace-json` writes the same data as nanoseconds for scripting. Every serial byte is one VM exit, so console
verbosity directly affects guest boot time; CI boots twice (default and `quiet`) to show this.

## Testing strategy

- Unit tests (GoogleTest) for everything that does not need KVM, including synthetic ELF and bzImage images
  generated in the test, a software page walk over the generated page tables, the UART probe sequences Linux uses,
  and an end-to-end load into real `GuestMemory`.
- `tinyvmm boot --dry-run` runs the full loader on a real kernel without KVM.
- CI on `ubuntu-latest` (x86_64, `/dev/kvm` available) runs the real-mode and long-mode smoke guests. The long-mode
  guest uses the same GDT, page tables and segment setup as the Linux path, so it checks that state against real
  hardware virtualization.
- CI then boots the Firecracker CI kernel with a one-file initramfs. This job is marked experimental until it has
  been observed passing.

## Known gaps and risks

- No MP table or ACPI tables. With the `no-acpi` Firecracker kernel, Linux should fall back to "virtual wire" mode
  (PIC through LAPIC LINT0). If the Linux boot fails in CI, writing an MP table (as Firecracker does) is the first
  thing to add.
- No MSR setup (`MTRRdefType`, `MISC_ENABLE`); Linux will report MTRRs as disabled.
- One vCPU; no CPUID topology leaves are adjusted beyond leaf 1.
- Guest RAM is capped at 3 GiB.
- The bzImage path is only tested against synthetic headers.
