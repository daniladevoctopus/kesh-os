#ifndef KESHOS_AHCI_H
#define KESHOS_AHCI_H

#include <stdint.h>

#define AHCI_PORTS 32
#define AHCI_GHC_AE (1U << 31)
#define AHCI_GHC_HR (1U << 0)
#define AHCI_PXCMD_ST (1U << 0)
#define AHCI_PXCMD_FRE (1U << 4)
#define AHCI_PXCMD_FR (1U << 14)
#define AHCI_PXCMD_CR (1U << 15)
#define AHCI_PXIS_TFES (1U << 30)
#define AHCI_SIG_ATA 0x00000101U
#define AHCI_CAP_S64A (1U << 31)
#define AHCI_FIS_REG_H2D 0x27U
#define AHCI_ATA_IDENTIFY 0xECU
#define AHCI_ATA_READ_DMA_EXT 0x25U
#define AHCI_ATA_WRITE_DMA_EXT 0x35U
#define AHCI_CMD_WRITE (1U << 6)

typedef volatile struct {
    uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
} ahci_hba_t;

typedef volatile struct {
    uint32_t clb, clbu, fb, fbu, is, ie, cmd, reserved0, tfd, sig, ssts, sctl, serr, sact, ci, sntf, fbs;
    uint32_t devslp;
    uint8_t reserved1[0x70 - 0x48];
    uint8_t vendor[0x80 - 0x70];
} ahci_port_t;

typedef struct { uint8_t cfl; uint8_t flags; uint16_t prdtl; uint32_t prdbc, ctba, ctbau; uint32_t reserved[4]; } __attribute__((packed)) ahci_command_header_t;
typedef struct { uint32_t dba, dbau, reserved; uint32_t dbc_i; } __attribute__((packed)) ahci_prdt_t;
typedef struct { uint8_t cfis[64], acmd[16]; uint8_t reserved[48]; ahci_prdt_t prdt[1]; } __attribute__((packed)) ahci_command_table_t;

_Static_assert(sizeof(ahci_command_header_t) == 32, "AHCI command header size");
_Static_assert(sizeof(ahci_command_table_t) == 144, "AHCI command table size");
_Static_assert(sizeof(ahci_hba_t) == 256, "AHCI HBA register layout");
_Static_assert(sizeof(ahci_port_t) == 128, "AHCI port register layout");

#endif
