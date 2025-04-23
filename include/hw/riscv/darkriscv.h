/*
 * QEMU RISC-V darkriscv Board
 *
 * Copyright (c) 2025 Nicolas Sauzede <nicolas.sauzede@gmail.com>
 *
 * Based on QEMU RISC-V VirtIO Board
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * RISC-V machine with 16550a UART and VirtIO MMIO
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hw/riscv/virt.h"

#define DARKRISCV
#define DARKRISCV_SOC_MAX_SIZE 1U

enum { DARKRISCV_SOC = VIRT_PCIE_MMIO };
