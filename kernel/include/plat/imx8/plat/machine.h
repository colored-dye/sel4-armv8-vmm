/*
 * Copyright 2017, DornerWorks
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_DORNERWORKS_GPL)
 */


#ifndef __PLAT_MACHINE_H
#define __PLAT_MACHINE_H

#include <plat/machine/devices.h>
#include <machine/io.h>
#include <plat_mode/machine.h>

enum IRQConstants {
    /* Private Peripheral Interrupts */
    //    INTERRUPT_RESERVED               = 0,
    //    INTERRUPT_RESERVED               = 1,
    //    INTERRUPT_RESERVED               = 2,
    //    INTERRUPT_RESERVED               = 3,
    //    INTERRUPT_RESERVED               = 4,
    //    INTERRUPT_RESERVED               = 5,
    //    INTERRUPT_RESERVED               = 6,
    //    INTERRUPT_RESERVED               = 7,
    //    INTERRUPT_RESERVED               = 8,
    //    INTERRUPT_RESERVED               = 9,
    //    INTERRUPT_RESERVED               = 10,
    //    INTERRUPT_RESERVED               = 11,
    //    INTERRUPT_RESERVED               = 12,
    //    INTERRUPT_RESERVED               = 13,
    //    INTERRUPT_RESERVED               = 14,
    //    INTERRUPT_RESERVED               = 15,
    //    INTERRUPT_RESERVED               = 16,
    //    INTERRUPT_RESERVED               = 17,
    //    INTERRUPT_RESERVED               = 18,
    //    INTERRUPT_RESERVED               = 19,
    //    INTERRUPT_RESERVED               = 20,
    //    INTERRUPT_RESERVED               = 21,
    INTERRUPT_PPI_ID22                     = 22, /* Debug Communications Channel (DCC) */
    INTERRUPT_PPI_ID23                     = 23, /* Performance Counter (PMU) overflow */
    INTERRUPT_PPI_ID24                     = 24, /* Cross Trigger (CTI) */
    INTERRUPT_PPI_ID25                     = 25, /* Virtual CPU interface maintenance*/
    INTERRUPT_PPI_ID26                     = 26, /* hypervisor timer */
    INTERRUPT_PPI_ID27                     = 27, /* virtual timer */
    INTERRUPT_PPI_ID28                     = 28, /* PPI ID28 */
    INTERRUPT_PPI_ID29                     = 29, /* PPI ID29 */
    INTERRUPT_PPI_ID30                     = 30, /* Non-secure physical timer */
    INTERRUPT_PPI_ID31                     = 31, /* PPI ID31 */
    /* Shared Peripheral Interrupt Mapping */
    INTERRUPT_A53_nEXTERR                  = 32,
    INTERRUPT_A53_nINTERR                  = 33,
    INTERRUPT_A72_nEXTERR                  = 34,
    INTERRUPT_A72_nINTERR                  = 35,
    INTERRUPT_VPU_nEXTERR                  = 36,
    //    INTERRUPT_RESERVED               = 37,
    //    INTERRUPT_RESERVED               = 38,
    //    INTERRUPT_RESERVED               = 39,
    INTERRUPT_CCI_nERROR                   = 40,
    INTERRUPT_CCI_nEVNTCNTOVERFLOW_0       = 41,
    INTERRUPT_CCI_nEVNTCNTOVERFLOW_1       = 42,
    INTERRUPT_CCI_nEVNTCNTOVERFLOW_2       = 43,
    INTERRUPT_CCI_nEVNTCNTOVERFLOW_3       = 44,
    INTERRUPT_CCI_nEVNTCNTOVERFLOW_4       = 45,
    //    INTERRUPT_RESERVED               = 46,
    //    INTERRUPT_RESERVED               = 47,
    INTERRUPT_M40_INT_OUT_0                = 48,
    INTERRUPT_M40_INT_OUT_1                = 49,
    INTERRUPT_M40_INT_OUT_2                = 50,
    INTERRUPT_M40_INT_OUT_3                = 51,
    INTERRUPT_M40_INT_OUT_4                = 52,
    INTERRUPT_M40_INT_OUT_5                = 53,
    INTERRUPT_M40_INT_OUT_6                = 54,
    INTERRUPT_M40_INT_OUT_7                = 55,
    INTERRUPT_M41_INT_OUT_0                = 56,
    INTERRUPT_M41_INT_OUT_1                = 57,
    INTERRUPT_M41_INT_OUT_2                = 58,
    INTERRUPT_M41_INT_OUT_3                = 59,
    INTERRUPT_M41_INT_OUT_4                = 60,
    INTERRUPT_M41_INT_OUT_5                = 61,
    INTERRUPT_M41_INT_OUT_6                = 62,
    INTERRUPT_M41_INT_OUT_7                = 63,
    INTERRUPT_SMMU                         = 64,
    INTERRUPT_DBLog_comb_irpt_s            = 65,
    INTERRUPT_DBLog_gbl_flt_irpt_ns        = 66,
    INTERRUPT_DBLog_gbl_flt_irpt_s         = 67,
    INTERRUPT_DBLog_perf_irpt_imx8_0       = 68,
    INTERRUPT_DBLog_perf_irpt_imx8_1       = 69,
    INTERRUPT_DBLog_perf_irpt_imx8_2       = 70,
    INTERRUPT_DBLog_perf_irpt_imx8_3       = 71,
    INTERRUPT_DISP_CTRLR0_INT_OUT_0        = 72,
    INTERRUPT_DISP_CTRLR0_INT_OUT_1        = 73,
    INTERRUPT_DISP_CTRLR0_INT_OUT_2        = 74,
    INTERRUPT_DISP_CTRLR0_INT_OUT_3        = 75,
    INTERRUPT_DISP_CTRLR0_INT_OUT_4        = 76,
    INTERRUPT_DISP_CTRLR0_INT_OUT_5        = 77,
    INTERRUPT_DISP_CTRLR0_INT_OUT_6        = 78,
    INTERRUPT_DISP_CTRLR0_INT_OUT_7        = 79,
    // INTERRUPT_RESERVED                  = 80,
    INTERRUPT_DISP_CNTRLR0_INT_OUT_9       = 81,
    INTERRUPT_DISP_CNTRLR0_INT_OUT_10      = 82,
    INTERRUPT_DISP_CNTRLR0_INT_OUT_11      = 83,
    INTERRUPT_DISP_CNTRLR0_INT_OUT_12      = 84,
    // INTERRUPT_RESERVED                  = 85,
    // INTERRUPT_RESERVED                  = 86,
    // INTERRUPT_RESERVED                  = 87,
    // INTERRUPT_RESERVED                  = 88,
    INTERRUPT_LDVS_0_INT_OUT               = 89,
    INTERRUPT_LDVS_1_INT_OUT               = 90,
    INTERRUPT_MIPI_DSI_0_INT_OUT           = 91,
    INTERRUPT_MIPI_DSI_1_INT_OUT           = 92,
    INTERRUPT_HDMI_TX_INT_OUT              = 93,
    //    INTERRUPT_RESERVED               = 94,
    //    INTERRUPT_RESERVED               = 95,
    INTERRUPT_GPU0_xaq2_intr               = 96,
    INTERRUPT_GPU1_xaq2_intr               = 97,
    INTERRUPT_DMA_eDMA0_INT                = 98,
    INTERRUPT_DMA_eDMA0_ERR_INT            = 99,
    INTERRUPT_DMA_eDMA1_INT                = 100,
    INTERRUPT_DMA_eDMA1_ERR_INT            = 101,
    INTERRUPT_PCIeA_MSI_CTRL               = 102,
    INTERRUPT_PCIeA_CLK_REQ                = 103,
    INTERRUPT_PCIeA_DMA                    = 104,
    INTERRUPT_PCIeA_INT_D                  = 105,
    INTERRUPT_PCIeA_INT_C                  = 106,
    INTERRUPT_PCIeA_INT_B                  = 107,
    INTERRUPT_PCIeA_INT_A                  = 108,
    INTERRUPT_PCIeA_SMLH_REQ_RST           = 109,
    INTERRUPT_PCIeA_GPIO_WAKEUP_0          = 110,
    INTERRUPT_PCIeA_GPIO_WAKEUP_1          = 111,
    INTERRUPT_LS_GPT0                      = 112,
    INTERRUPT_LS_GPT1                      = 113,
    INTERRUPT_LS_GPT2                      = 114,
    INTERRUPT_LS_GPT3                      = 115,
    INTERRUPT_LS_GPT4                      = 116,
    INTERRUPT_LS_KPP                       = 117,
    //    INTERRUPT_RESERVED               = 118,
    //    INTERRUPT_RESERVED               = 119,
    INTERRUPT_SATA_0                       = 120,
    INTERRUPT_SATA_1                       = 121,
    //    INTERRUPT_RESERVED               = 122,
    //    INTERRUPT_RESERVED               = 123,
    INTERRUPT_LSIO_OctaSPI0                = 124,
    INTERRUPT_LSIO_OctaSPI1                = 125,
    INTERRUPT_LSIO_PWM0                    = 126,
    INTERRUPT_LSIO_PWM1                    = 127,
    INTERRUPT_LSIO_PWM2                    = 128,
    INTERRUPT_LSIO_PWM3                    = 129,
    INTERRUPT_LSIO_PWM4                    = 130,
    INTERRUPT_LSIO_PWM5                    = 131,
    INTERRUPT_LSIO_PWM6                    = 132,
    INTERRUPT_LSIO_PWM7                    = 133,
    INTERRUPT_PCIeB_MSI_CTRL               = 134,
    INTERRUPT_PCIeB_CLK_REQ                = 135,
    INTERRUPT_PCIeB_DMA                    = 136,
    INTERRUPT_PCIeB_INT_D                  = 137,
    INTERRUPT_PCIeB_INT_C                  = 138,
    INTERRUPT_PCIeB_INT_B                  = 139,
    INTERRUPT_PCIeB_INT_A                  = 140,
    INTERRUPT_PCIeB_INT_SMLH_REQ_RST       = 141,
    INTERRUPT_PCIeB_INT_GPIO_WAKEUP0       = 142,
    INTERRUPT_PCIeB_INT_GPIO_WAKEUP1       = 143,
    INTERRUPT_SCU_INT_OUT_0                = 144,
    INTERRUPT_SCU_INT_OUT_1                = 145,
    INTERRUPT_SCU_INT_OUT_2                = 146,
    INTERRUPT_SCU_INT_OUT_3                = 147,
    INTERRUPT_SCU_INT_OUT_4                = 148,
    INTERRUPT_SCU_INT_OUT_5                = 149,
    INTERRUPT_SCU_INT_OUT_6                = 150,
    INTERRUPT_SCU_INT_OUT_7                = 151,
    INTERRUPT_SCU_SYS_COUNT_INT_0          = 152,
    INTERRUPT_SCU_SYS_COUNT_INT_1          = 153,
    INTERRUPT_SCU_SYS_COUNT_INT_2          = 154,
    INTERRUPT_SCU_SYS_COUNT_INT_3          = 155,
    //    INTERRUPT_RESERVED               = 156,
    //    INTERRUPT_RESERVED               = 157,
    //    INTERRUPT_RESERVED               = 158,
    //    INTERRUPT_RESERVED               = 159,
    INTERRUPT_DRC0_DFI_ALERT_ERR           = 160,
    INTERRUPT_DRC1_DFI_ALERT_ERR           = 161,
    INTERRUPT_DRC0_PERF_CNT_FULL           = 162,
    INTERRUPT_DRC1_PERF_CNT_FULL           = 163,
    //    INTERRUPT_RESERVED               = 164,
    //    INTERRUPT_RESERVED               = 165,
    //    INTERRUPT_RESERVED               = 166,
    //    INTERRUPT_RESERVED               = 167,
    INTERRUPT_GPIO_INT_0                   = 168,
    INTERRUPT_GPIO_INT_1                   = 169,
    INTERRUPT_GPIO_INT_2                   = 170,
    INTERRUPT_GPIO_INT_3                   = 171,
    INTERRUPT_GPIO_INT_4                   = 172,
    INTERRUPT_GPIO_INT_5                   = 173,
    INTERRUPT_GPIO_INT_6                   = 174,
    INTERRUPT_GPIO_INT_7                   = 175,
    //    INTERRUPT_RESERVED               = 176,
    //    INTERRUPT_RESERVED               = 177,
    //    INTERRUPT_RESERVED               = 178,
    //    INTERRUPT_RESERVED               = 179,
    //    INTERRUPT_RESERVED               = 180,
    //    INTERRUPT_RESERVED               = 181,
    //    INTERRUPT_RESERVED               = 182,
    //    INTERRUPT_RESERVED               = 183,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_0       = 184,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_1       = 185,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_2       = 186,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_3       = 187,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_4       = 188,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_5       = 189,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_6       = 190,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_7       = 191,
    //    INTERRUPT_DISP_CNTRLR1_INT_OUT_8 = 192,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_9       = 193,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_10      = 194,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_11      = 195,
    INTERRUPT_DISP_CNTRLR1_INT_OUT_12      = 196,
    //    INTERRUPT_RESERVED               = 197,
    //    INTERRUPT_RESERVED               = 198,
    //    INTERRUPT_RESERVED               = 199,
    INTERRUPT_VPU_SYS_INT_0                = 200,
    INTERRUPT_VPU_SYS_INT_1                = 201,
    //    INTERRUPT_RESERVED               = 202,
    //    INTERRUPT_RESERVED               = 203,
    //    INTERRUPT_RESERVED               = 204,
    //    INTERRUPT_RESERVED               = 205,
    //    INTERRUPT_RESERVED               = 206,
    //    INTERRUPT_RESERVED               = 207,
    INTERRUPT_LS_MU0                       = 208,
    INTERRUPT_LS_MU1                       = 209,
    INTERRUPT_LS_MU2                       = 210,
    INTERRUPT_LS_MU3                       = 211,
    INTERRUPT_LS_MU4                       = 212,
    //    INTERRUPT_RESERVED               = 213,
    //    INTERRUPT_RESERVED               = 214,
    //    INTERRUPT_RESERVED               = 215,
    INTERRUPT_LS_MU5_A                     = 216,
    INTERRUPT_LS_MU6_A                     = 217,
    INTERRUPT_LS_MU7_A                     = 218,
    INTERRUPT_LS_MU8_A                     = 219,
    INTERRUPT_LS_MU9_A                     = 220,
    INTERRUPT_LS_MU10_A                    = 221,
    INTERRUPT_LS_MU11_A                    = 222,
    INTERRUPT_LS_MU12_A                    = 223,
    INTERRUPT_LS_MU13_A                    = 224,
    //    INTERRUPT_RESERVED               = 225,
    //    INTERRUPT_RESERVED               = 226,
    //    INTERRUPT_RESERVED               = 227,
    //    INTERRUPT_RESERVED               = 228,
    //    INTERRUPT_RESERVED               = 229,
    //    INTERRUPT_RESERVED               = 230,
    //    INTERRUPT_RESERVED               = 231,
    INTERRUPT_LS_MU5_B                     = 232,
    INTERRUPT_LS_MU6_B                     = 233,
    INTERRUPT_LS_MU7_B                     = 234,
    INTERRUPT_LS_MU8_B                     = 235,
    INTERRUPT_LS_MU9_B                     = 236,
    INTERRUPT_LS_MU10_B                    = 237,
    INTERRUPT_LS_MU11_B                    = 238,
    INTERRUPT_LS_MU12_B                    = 239,
    INTERRUPT_LS_MU13_B                    = 240,
    //    INTERRUPT_RESERVED               = 241,
    //    INTERRUPT_RESERVED               = 242,
    //    INTERRUPT_RESERVED               = 243,
    //    INTERRUPT_RESERVED               = 244,
    //    INTERRUPT_RESERVED               = 245,
    //    INTERRUPT_RESERVED               = 246,
    //    INTERRUPT_RESERVED               = 247,
    INTERRUPT_DMA_SPI0                     = 248,
    INTERRUPT_DMA_SPI1                     = 249,
    INTERRUPT_DMA_SPI2                     = 250,
    INTERRUPT_DMA_SPI3                     = 251,
    INTERRUPT_DMA_I2C0                     = 252,
    INTERRUPT_DMA_I2C1                     = 253,
    INTERRUPT_DMA_I2C2                     = 254,
    INTERRUPT_DMA_I2C3                     = 255,
    INTERRUPT_DMA_I2C4                     = 256,
    INTERRUPT_DMA_UART0                    = 257,
    INTERRUPT_DMA_UART1                    = 258,
    INTERRUPT_DMA_UART2                    = 259,
    INTERRUPT_DMA_UART3                    = 260,
    INTERRUPT_DMA_UART4                    = 261,
    INTERRUPT_DMA_SIM0                     = 262,
    INTERRUPT_DMA_SIM1                     = 263,
    INTERRUPT_uSDHC0                       = 264,
    INTERRUPT_uSDHC1                       = 265,
    INTERRUPT_uSDHC2                       = 266,
    INTERRUPT_DMA_FlexCAN0                 = 267,
    INTERRUPT_DMA_FlexCAN1                 = 268,
    INTERRUPT_DMA_FlexCAN2                 = 269,
    INTERRUPT_DMA_FTM0                     = 270,
    INTERRUPT_DMA_FTM1                     = 271,
    INTERRUPT_DMA_ADC0                     = 272,
    INTERRUPT_DMA_ADC1                     = 273,
    INTERRUPT_DMA_EXTERNAL_0               = 274,
    INTERRUPT_DMA_EXTERNAL_1               = 275,
    INTERRUPT_DMA_EXTERNAL_2               = 276,
    INTERRUPT_DMA_EXTERNAL_3               = 277,
    INTERRUPT_DMA_EXTERNAL_4               = 278,
    INTERRUPT_DMA_EXTERNAL_5               = 279,
    //    INTERRUPT_RESERVED               = 280,
    //    INTERRUPT_RESERVED               = 281,
    //    INTERRUPT_RESERVED               = 282,
    //    INTERRUPT_RESERVED               = 283,
    //    INTERRUPT_RESERVED               = 284,
    //    INTERRUPT_RESERVED               = 285,
    //    INTERRUPT_RESERVED               = 286,
    //    INTERRUPT_RESERVED               = 287,
    INTERRUPT_ENET0_FRAME1                 = 288,
    INTERRUPT_ENET0_FRAME2                 = 289,
    INTERRUPT_ENET0_FRAME0_EVENT           = 290,
    INTERRUPT_ENET0_TIMER                  = 291,
    INTERRUPT_ENET1_FRAME1                 = 292,
    INTERRUPT_ENET1_FRAME2                 = 293,
    INTERRUPT_ENET1_FRAME0_EVENT           = 294,
    INTERRUPT_ENET1_TIMER                  = 295,
    INTERRUPT_DTCP                         = 296,
    INTERRUPT_MLB                          = 297,
    INTERRUPT_MLB_AHB                      = 298,
    INTERRUPT_USB_OTG                      = 299,
    INTERRUPT_USB_HOST                     = 300,
    INTERRUPT_UTMI                         = 301,
    INTERRUPT_WAKEUP                       = 302,
    INTERRUPT_USB3                         = 303,
    INTERRUPT_ND_FLASH_BCH                 = 304,
    INTERRUPT_ND_FLASH_GPMI                = 305,
    INTERRUPT_APBHDMA                      = 306,
    INTERRUPT_DMA                          = 307,
    INTERRUPT_DMA_ERR                      = 308,
    //    INTERRUPT_RESERVED               = 309,
    //    INTERRUPT_RESERVED               = 310,
    //    INTERRUPT_RESERVED               = 311,
    //    INTERRUPT_RESERVED               = 312,
    //    INTERRUPT_RESERVED               = 313,
    //    INTERRUPT_RESERVED               = 314,
    //    INTERRUPT_RESERVED               = 315,
    //    INTERRUPT_RESERVED               = 316,
    //    INTERRUPT_RESERVED               = 317,
    //    INTERRUPT_RESERVED               = 318,
    //    INTERRUPT_RESERVED               = 319,
    INTERRUPT_MSI                          = 320,
    //    INTERRUPT_RESERVED               = 321,
    //    INTERRUPT_RESERVED               = 322,
    //    INTERRUPT_RESERVED               = 323,
    //    INTERRUPT_RESERVED               = 324,
    //    INTERRUPT_RESERVED               = 325,
    //    INTERRUPT_RESERVED               = 326,
    //    INTERRUPT_RESERVED               = 327,
    //    INTERRUPT_RESERVED               = 328,
    INTERRUPT_PDMA_STREAM0                 = 329,
    INTERRUPT_PDMA_STREAM1                 = 330,
    INTERRUPT_PDMA_STREAM2                 = 331,
    INTERRUPT_PDMA_STREAM3                 = 332,
    INTERRUPT_PDMA_STREAM4                 = 333,
    INTERRUPT_PDMA_STREAM5                 = 334,
    INTERRUPT_PDMA_STREAM6                 = 335,
    INTERRUPT_PDMA_STREAM7                 = 336,
    INTERRUPT_MJPEG_ENC0                   = 337,
    INTERRUPT_MJPEG_ENC1                   = 338,
    INTERRUPT_MJPEG_ENC2                   = 339,
    INTERRUPT_MJPEG_ENC3                   = 340,
    INTERRUPT_MJPEG_DEC0                   = 341,
    INTERRUPT_MJPEG_DEC1                   = 342,
    INTERRUPT_MJPEG_DEC2                   = 343,
    INTERRUPT_MJPEG_DEC3                   = 344,
    //    INTERRUPT_RESERVED               = 345,
    INTERRUPT_SAI0_MOD                     = 346,
    INTERRUPT_SAI0_DMA                     = 347,
    INTERRUPT_SAI1_MOD                     = 348,
    INTERRUPT_SAI1_DMA                     = 349,
    INTERRUPT_SAI2_MOD                     = 350,
    INTERRUPT_SAI2_DMA                     = 351,
    INTERRUPT_MIPI_CSI0_OUT                = 352,
    INTERRUPT_MIPI_CSI1_OUT                = 353,
    INTERRUPT_HDMI_RX_OUT                  = 354,
    INTERRUPT_SAI3_MOD                     = 355,
    INTERRUPT_SAI3_DMA                     = 356,
    INTERRUPT_SAI_HDMI_RX_MOD              = 357,
    INTERRUPT_SAI_HDMI_RX_DMA              = 358,
    INTERRUPT_SAI_HDMI_TX_MOD              = 359,
    INTERRUPT_SAI_HDMI_TX_DMA              = 360,
    INTERRUPT_SAI6_MOD                     = 361,
    INTERRUPT_SAI6_DMA                     = 362,
    INTERRUPT_SAI7_MOD                     = 363,
    INTERRUPT_SAI7_DMA                     = 364,
    //    INTERRUPT_RESERVED               = 365,
    //    INTERRUPT_RESERVED               = 366,
    //    INTERRUPT_RESERVED               = 367,
    INTERRUPT_SPI0_MOD                     = 368,
    INTERRUPT_SPI1_MOD                     = 369,
    INTERRUPT_SPI2_MOD                     = 370,
    INTERRUPT_SPI3_MOD                     = 371,
    INTERRUPT_I2C0                         = 372,
    INTERRUPT_I2C1                         = 373,
    INTERRUPT_I2C2                         = 374,
    INTERRUPT_I2C3                         = 375,
    INTERRUPT_I2C4                         = 376,
    INTERRUPT_UART0_MOD                    = 377,
    INTERRUPT_UART1_MOD                    = 378,
    INTERRUPT_UART2_MOD                    = 379,
    INTERRUPT_UART3_MOD                    = 380,
    INTERRUPT_UART4_MOD                    = 381,
    INTERRUPT_SIM0_MOD                     = 382,
    INTERRUPT_SIM1_MOD                     = 383,
    INTERRUPT_FlexCAN0_MOD                 = 384,
    INTERRUPT_FlexCAN1_MOD                 = 385,
    INTERRUPT_FlexCAN2_MOD                 = 386,
    INTERRUPT_FTM0_MOD                     = 387,
    INTERRUPT_FTM1_MOD                     = 388,
    INTERRUPT_ADC0_MOD                     = 389,
    INTERRUPT_ADC1_MOD                     = 390,
    INTERRUPT_FlexCAN0_DMA                 = 391,
    INTERRUPT_FlexCAN1_DMA                 = 392,
    INTERRUPT_FlexCAN2_DMA                 = 393,
    INTERRUPT_FTM0_DMA                     = 394,
    INTERRUPT_FTM1_DMA                     = 395,
    INTERRUPT_ADC0_DMA                     = 396,
    INTERRUPT_ADC1_DMA                     = 397,
    //    INTERRUPT_RESERVED               = 398,
    //    INTERRUPT_RESERVED               = 399,
    INTERRUPT_eDMA0                        = 400,
    INTERRUPT_eDMA0_ERR                    = 401,
    INTERRUPT_eDMA1                        = 402,
    INTERRUPT_eDMA1_ERR                    = 403,
    INTERRUPT_ASRC0_INT1                   = 404,
    INTERRUPT_ASRC0_INT2                   = 405,
    INTERRUPT_DMA0_CH0                     = 406,
    INTERRUPT_DMA0_CH1                     = 407,
    INTERRUPT_DMA0_CH2                     = 408,
    INTERRUPT_DMA0_CH3                     = 409,
    INTERRUPT_DMA0_CH4                     = 410,
    INTERRUPT_DMA0_CH5                     = 411,
    INTERRUPT_ASRC1_INT1                   = 412,
    INTERRUPT_ASRC1_INT2                   = 413,
    INTERRUPT_DMA1_CH0                     = 414,
    INTERRUPT_DMA1_CH1                     = 415,
    INTERRUPT_DMA1_CH2                     = 416,
    INTERRUPT_DMA1_CH3                     = 417,
    INTERRUPT_DMA1_CH4                     = 418,
    INTERRUPT_DMA1_CH5                     = 419,
    INTERRUPT_ESAI0                        = 420,
    INTERRUPT_ESAI1                        = 421,
    //    INTERRUPT_UNUSED                 = 422,
    INTERRUPT_GPT0                         = 423,
    INTERRUPT_GPT1                         = 424,
    INTERRUPT_GPT2                         = 425,
    INTERRUPT_GPT3                         = 426,
    INTERRUPT_GPT4                         = 427,
    INTERRUPT_GPT5                         = 428,
    INTERRUPT_SAI0                         = 429,
    INTERRUPT_SAI1                         = 430,
    INTERRUPT_SAI2                         = 431,
    INTERRUPT_SAI3                         = 432,
    INTERRUPT_SAI_HDMI_RX                  = 433,
    INTERRUPT_SAI_HDMI_TX                  = 434,
    INTERRUPT_SAI6                         = 435,
    INTERRUPT_SAI7                         = 436,
    INTERRUPT_SPDIF0_RX                    = 437,
    INTERRUPT_SPDIF0_TX                    = 438,
    INTERRUPT_SPDIF1_RX                    = 439,
    INTERRUPT_SPDIF1_TX                    = 440,
    INTERRUPT_ESAI0_MOD                    = 441,
    INTERRUPT_ESAI0_DMA                    = 442,
    INTERRUPT_ESAI1_MOD                    = 443,
    INTERRUPT_ESAI1_DMA                    = 444,
    //    INTERRUPT_RESERVED               = 445,
    //    INTERRUPT_RESERVED               = 446,
    //    INTERRUPT_RESERVED               = 447,
    INTERRUPT_DMA_SPI0_RX                  = 448,
    INTERRUPT_DMA_SPI0_TX                  = 449,
    INTERRUPT_DMA_SPI1_RX                  = 450,
    INTERRUPT_DMA_SPI1_TX                  = 451,
    INTERRUPT_DMA_SPI2_RX                  = 452,
    INTERRUPT_DMA_SPI2_TX                  = 453,
    INTERRUPT_DMA_SPI3_RX                  = 454,
    INTERRUPT_DMA_SPI3_TX                  = 455,
    INTERRUPT_DMA_I2C0_RX                  = 456,
    INTERRUPT_DMA_I2C0_TX                  = 457,
    INTERRUPT_DMA_I2C1_RX                  = 458,
    INTERRUPT_DMA_I2C1_TX                  = 459,
    INTERRUPT_DMA_I2C2_RX                  = 460,
    INTERRUPT_DMA_I2C2_TX                  = 461,
    INTERRUPT_DMA_I2C3_RX                  = 462,
    INTERRUPT_DMA_I2C3_TX                  = 463,
    INTERRUPT_DMA_I2C4_RX                  = 464,
    INTERRUPT_DMA_I2C4_TX                  = 465,
    INTERRUPT_UART0_RX                     = 466,
    INTERRUPT_UART0_TX                     = 467,
    INTERRUPT_UART1_RX                     = 468,
    INTERRUPT_UART1_TX                     = 469,
    INTERRUPT_UART2_RX                     = 470,
    INTERRUPT_UART2_TX                     = 471,
    INTERRUPT_UART3_RX                     = 472,
    INTERRUPT_UART3_TX                     = 473,
    INTERRUPT_UART4_RX                     = 474,
    INTERRUPT_UART4_TX                     = 475,
    INTERRUPT_SIM0_RX                      = 476,
    INTERRUPT_SIM0_TX                      = 477,
    INTERRUPT_SIM1_RX                      = 478,
    INTERRUPT_SIM1_TX                      = 479,
    INTERRUPT_SEC_MU1_A                    = 480,
    INTERRUPT_SEC_MU2_A                    = 481,
    INTERRUPT_SEC_MU3_A                    = 482,
    INTERRUPT_CAAM_INT0                    = 483,
    INTERRUPT_CAAM_INT1                    = 484,
    INTERRUPT_CAAM_INT2                    = 485,
    INTERRUPT_CAAM_INT3                    = 486,
    INTERRUPT_CAAM_RTIC                    = 487,
    INTERRUPT_SPDIF0_RX_MOD                = 488,
    INTERRUPT_SPDIF0_RX_DMA                = 489,
    INTERRUPT_SPDIF0_TX_MOD                = 490,
    INTERRUPT_SPDIF0_TX_DMA                = 491,
    INTERRUPT_SPDIF1_RX_MOD                = 492,
    INTERRUPT_SPDIF1_RX_DMA                = 493,
    INTERRUPT_SPDIF1_TX_MOD                = 494,
    INTERRUPT_SPDIF1_TX_DMA                = 495,
    INTERRUPT_VPU_INT_0                    = 496,
    INTERRUPT_VPU_INT_1                    = 497,
    INTERRUPT_VPU_INT_2                    = 498,
    INTERRUPT_VPU_INT_3                    = 499,
    INTERRUPT_VPU_INT_4                    = 500,
    INTERRUPT_VPU_INT_5                    = 501,
    INTERRUPT_VPU_INT_6                    = 502,
    INTERRUPT_VPU_INT_7                    = 503,
    //    INTERRUPT_RESERVED               = 504,
    //    INTERRUPT_RESERVED               = 505,
    //    INTERRUPT_RESERVED               = 506,
    //    INTERRUPT_RESERVED               = 507,
    //    INTERRUPT_RESERVED               = 508,
    //    INTERRUPT_RESERVED               = 509,
    //    INTERRUPT_RESERVED               = 510,
    //    INTERRUPT_UNUSED                 = 511,
    maxIRQ = 503
} platform_interrupt_t;

#define N_INTERRUPTS                512

#define INTERRUPT_VGIC_MAINTENANCE  INTERRUPT_PPI_ID25

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define KERNEL_TIMER_IRQ            INTERRUPT_PPI_ID26
#else
#define KERNEL_TIMER_IRQ            INTERRUPT_PPI_ID27
#define KERNEL_PMU_IRQ              INTERRUPT_PPI_ID23
#endif

#include <arch/machine/gic_500.h>

#endif /* __PLAT_MACHINE_H */
