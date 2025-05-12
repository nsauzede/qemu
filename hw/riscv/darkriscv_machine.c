#include "hw/misc/darkriscv_soc.h"
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/boards.h"
#include "target/riscv/cpu.h"
#include "hw/loader.h"
#include "qapi/error.h"
#define DARKIO_BASE 0x40000000
static void darkriscv_board_init(MachineState *machine)
{
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ram, NULL, "darkriscv.ram", machine->ram_size, &error_abort);        // Allocate RAM
    memory_region_add_subregion(sysmem, 0x00000000, ram);
    cpu_create(machine->cpu_type);                                                              // Create a single CPU
    sysbus_create_simple(TYPE_DARKIO_DEVICE, DARKIO_BASE, NULL);                                // Instantiate darkio device
}
static void darkriscv_board_machine_class_init(MachineClass *mc)
{
    mc->desc = "RISC-V Board compatible with Darkriscv";
    mc->init = darkriscv_board_init;
    mc->default_ram_size = 64 * KiB;                            // 32 * KiB;
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;                 //RISCV_CPU_TYPE_NAME("rv32e") => resets PC on "csrr a0,mhartid"?
}
DEFINE_MACHINE("darkriscv", darkriscv_board_machine_class_init)
