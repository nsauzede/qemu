/*
 * QEMU RISC-V Darkriscv SoC
 *
 * Copyright (c) 2025 Nicolas Sauzede <nicolas.sauzede@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "hw/misc/darkriscv_soc.h"
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "qapi/error.h"
#include "hw/registerfields.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"

#define DARKIO_DEVICE(obj) \
    OBJECT_CHECK(DarkioDeviceState, (obj), TYPE_DARKIO_DEVICE)
#define DARKIO_SIZE 32

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    uint32_t reg;
} DarkioDeviceState;

struct DARKIO {
    unsigned char board_id; // 00
    unsigned char board_cm; // 01
    unsigned char core_id;  // 02
    unsigned char irq;      // 03
    struct DARKUART {
        unsigned char  stat; // 04
        unsigned char  fifo; // 05
        unsigned short baud; // 06/07
    } uart;
    unsigned int led;        // 08
    unsigned int timer;      // 0c
    unsigned int timeus;     // 10
    unsigned int iport;      // 14
    unsigned int oport;      // 18
    struct DARKSPI {
        union {
            unsigned char  spi8;  // 1c                              r: {data}
            unsigned short spi16; // 1c/1d       w: {cmd,data}       r: {dlo,dhi}
            unsigned int   spi32; // 1c/1d/1e/1f w: {00,cmd,dlo,dhi} r: {status,00,dlo,dhi}
        };
    } spi;
};
volatile struct DARKIO io =
{
    0, 100, 0, 0,   // ctrl = { board id, fMHz, fkHz }
    //4, 100, 0, 0,   // ctrl = { board id, fMHz, fkHz }
    { 0, 0, 0 },    // uart = { stat, fifo, baud }
    0,              // led
    1000000,        // timer
    0,              // timerus
    0,              // iport
    0               // oport
};
static uint32_t led_read32(void) {
    uint32_t ret = io.led;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static void led_write32(uint32_t data) {
    printf("%s: data=0x%08" PRIx32 "\n", __func__, data);
    io.led = data;
}
static void spi_master_write16(uint16_t data) {
    printf("%s: data=0x%04" PRIx16 "\n", __func__, data);
    io.spi.spi16 = data;
}
static void spi_master_write32(uint32_t data) {
    printf("%s: data=0x%08" PRIx32 "\n", __func__, data);
    io.spi.spi32 = data;
}
static uint8_t spi_master_read8_status(void) {
    uint8_t ret = 2;    // status = {6'b0, spi_ready & ~WR & ~spi_request, spi_busy};
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint16_t spi_master_read16_data(void) {
    uint32_t spi32 = io.spi.spi32;
    printf("%s: checking spi32=0x%08" PRIx32 "\n", __func__, spi32);
    uint16_t ret = 0;    // data
    switch (spi32) {
        case 0x00e80000:        // Read OUT_X_L (Addr 0x28)
            //ret = (io.oport & 0xffff0000) >> 16;
            ret = ((io.oport & 0xff000000) >> 24) | ((io.oport & 0xff0000) >> 8);
            break;
        default:
            exit(1);
    }
    printf("%s: spi32=0x%08" PRIx32 ", returning 0x%04" PRIx16 "\n", __func__, spi32, ret);
    return ret;
}
static uint8_t spi_master_read8_data(void) {
    uint16_t spi16 = io.spi.spi16;
    printf("%s: checking spi16=0x%04" PRIx16 "\n", __func__, spi16);
    uint8_t ret = 0;    // data
    switch (spi16) {
        case 0x1fc0:            // Enable temperature sensor (Addr 0x1F)
        case 0x2300:            // spi slave LIS3DH: CTRL_REG4, SIM=0: set 4-wire interface
        case 0x2388:            // Enable BDU, High resolution (Addr 0x23)
        case 0x2077:            // Write ODR in CTRL_REG1 (Addr 0x20)
            break;              // don't care
        case 0x8f00:            // whoami
            ret = 0x33;
            break;
        default:
            exit(1);
    }
    printf("%s: spi16=0x%04" PRIx16 ", returning 0x%02" PRIx8 "\n", __func__, spi16, ret);
    return ret;
}
static uint8_t uart_read_stat(void) {
    uint8_t ret = 0;    // UART_STATE = { 6'd0, UART_RREQ!=UART_RACK, UART_XREQ!=UART_XACK };
    //printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint16_t uart_read16_baud(void) {
    uint16_t ret = io.uart.baud;    // baud
    //printf("%s: spi32=0x%08" PRIx32 ", returning 0x%04" PRIx16 "\n", __func__, spi32, ret);
    return ret;
}
static void uart_write8_fifo(uint8_t data) {
    //printf("%s: data=0x%02" PRIx8 "\n", __func__, data);
    printf("%c", data);fflush(stdout);
}
static void ctrl_write8_irq(uint8_t data) {
    printf("%s: data=0x%02" PRIx8 "\n", __func__, data);
    io.irq &= ~data;
}
static uint8_t ctrl_read8_irq(void) {
    uint8_t ret = io.irq;
//    ret |= 0x80;        // IRQ_TIMR
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint8_t ctrl_read8_board_id(void) {
    uint8_t ret = io.board_id;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint8_t ctrl_read8_board_cm(void) {
    uint8_t ret = io.board_cm;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint8_t ctrl_read8_core_id(void) {
    uint8_t ret = io.core_id;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint32_t timeus_read32(void) {
    uint32_t ret = io.timeus++;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint32_t timer_read32(void) {
    uint32_t ret = io.timer++;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static uint32_t oport_read32(void) {
    uint32_t ret = io.oport;
    printf("%s: returning 0x%02" PRIx8 "\n", __func__, ret);
    return ret;
}
static void oport_write32(uint32_t data) {
    printf("%s: data=0x%08" PRIx32 "\n", __func__, data);
    io.oport = data;
}
/*
// Function to generate a trap
static void generate_trap(CPURISCVState *env) {
    // Set the exception cause to illegal instruction (for example)
    env->pending_exception = EXCP_ILLEGAL_INST;
    // Set the program counter to the exception handler
    env->pc = env->exception_pc;
    // Indicate that an exception has occurred
    env->exception_index = EXCP_ILLEGAL_INST;
    // Set the exception PC to the current PC
    env->exception_pc = env->pc;
}
*/
static uint64_t darkio_read(void *opaque, hwaddr addr, unsigned size)
{//    DarkioDeviceState *s = opaque;
    uint64_t res = 0;
    //printf("DarkriscvDevice: read %u bytes at addr 0x%" HWADDR_PRIx "\n", size, addr);
    switch (addr) {
        case 0x00:      // ctrl = { board id, fMHz, fkHz }
            switch (size) {
                case 1:
                    return ctrl_read8_board_id();
                    break;
            }
            break;
        case 0x01:      // board_cm
            switch (size) {
                case 1:
                    return ctrl_read8_board_cm();
                    break;
            }
            break;
        case 0x02:      // core_id
            switch (size) {
                case 1:
                    return ctrl_read8_core_id();
                    break;
            }
            break;
        case 0x03:      // irq
            switch (size) {
                case 1:
                    return ctrl_read8_irq();
                    break;
            }
            break;
        case 0x04:      // uart = { stat, fifo, baud }
            switch (size) {
                case 1:
                    return uart_read_stat();
                    break;
            }
            break;
        case 0x06:      // uart = { stat, fifo, baud }
            switch (size) {
                case 2:
                    return uart_read16_baud();
                    break;
            }
            break;
        case 0x08:      // led
            switch (size) {
                case 4: {
                    return led_read32();
                    break;
                }
            }
            break;
        case 0x0c:      // timer
            switch (size) {
                case 4: {
                    return timer_read32();
                    break;
                }
            }
            break;
        case 0x10:      // timeus
            switch (size) {
                case 4: {
                    return timeus_read32();
                    break;
                }
            }
            break;
        case 0x18:      // oport
            switch (size) {
                case 4: {
                    return oport_read32();
                    break;
                }
            }
            break;
        case 0x1c:      // spi_master
            switch (size) {
                case 1:
                    return spi_master_read8_data();
                    break;
                case 2:
                    return spi_master_read16_data();
                    break;
            }
            break;
        case 0x1f:      // spi_master
            switch (size) {
                case 1:
                    return spi_master_read8_status();
                    break;
            }
            break;
    }
    printf("%s: read %u bytes at addr 0x%" HWADDR_PRIx "\n", __func__, size, addr);
    exit(1);
    return res;
}
static void darkio_write(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{//DarkioDeviceState *s = opaque;
    //printf("DarkriscvDevice: write %u bytes 0x%lx to addr 0x%" HWADDR_PRIx "\n", size, data, addr);
    //unsigned char *bytes = (unsigned char *)&io;
    switch (addr) {
        case 0x03: // irq
            switch (size) {
                case 1:
                    ctrl_write8_irq(data);
                    return;
            }
            break;
        case 0x05: // uart = { stat, fifo, baud }
            switch (size) {
                case 1:
                    uart_write8_fifo(data);
                    return;
            }
            break;
        case 0x08: // led
            switch (size) {
                case 1:
                case 4:
                    led_write32(data);
                    return;
            }
            break;
        case 0x18: // oport
            switch (size) {
                case 4:
                    oport_write32(data);
                    return;
            }
            break;
        case 0x1c: // spi_master
            switch (size) {
                case 2:
                    spi_master_write16(data);
                    return;
                case 4:
                    spi_master_write32(data);
                    return;
            }
            break;
    }
    printf("%s: write %u bytes 0x%lx to addr 0x%" HWADDR_PRIx "\n", __func__, size, data, addr);
    exit(1);
}

static const MemoryRegionOps darkio_ops = {
    .read = darkio_read,
    .write = darkio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

static void darkio_init(Object *obj)
{
    DarkioDeviceState *s = DARKIO_DEVICE(obj);

    s->reg = 0;
    printf("%s: reg=%x\n", __func__, s->reg);
    //exit(1);
    memory_region_init_io(&s->mmio, obj, &darkio_ops, s, TYPE_DARKIO_DEVICE, DARKIO_SIZE);     // last arg 0x1000 is the mapping size
    printf("%s: CALLING sysbus_init_mmio                (FIRST)\n", __func__);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
    //exit(1);
}

static void darkio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->desc = "Darkio 32-bit MMIO Device: ctrl, uart, led, timer, gpio. spi";
}
static const TypeInfo darkio_info = {
    .name = TYPE_DARKIO_DEVICE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DarkioDeviceState),
    .instance_init = darkio_init,
    .class_init = darkio_class_init,
};

static void darkio_register_types(void)
{
    //printf("%s: CALLING type_register_static\n", __func__);
    type_register_static(&darkio_info);
}

type_init(darkio_register_types)
