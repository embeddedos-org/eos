/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 EoS Project */
/* STM32F407 Startup Assembly — Cortex-M4 Vector Table + Reset Handler */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

.global g_pfnVectors
.global Default_Handler
.global Reset_Handler

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function

Reset_Handler:
    /* Set stack pointer */
    ldr   r0, =_estack
    mov   sp, r0

    /* Enable FPU (CP10 + CP11 full access) */
    ldr   r0, =0xE000ED88
    ldr   r1, [r0]
    orr   r1, r1, #(0xF << 20)
    str   r1, [r0]
    dsb
    isb

    /* Copy .data from FLASH to RAM */
    ldr   r0, =_sdata
    ldr   r1, =_edata
    ldr   r2, =_sidata
    movs  r3, #0
    b     LoopCopyDataInit

CopyDataInit:
    ldr   r4, [r2, r3]
    str   r4, [r0, r3]
    adds  r3, r3, #4

LoopCopyDataInit:
    adds  r4, r0, r3
    cmp   r4, r1
    bcc   CopyDataInit

    /* Zero .bss */
    ldr   r2, =_sbss
    ldr   r4, =_ebss
    movs  r3, #0
    b     LoopFillZerobss

FillZerobss:
    str   r3, [r2]
    adds  r2, r2, #4

LoopFillZerobss:
    cmp   r2, r4
    bcc   FillZerobss

    /* Call SystemInit (clock setup) */
    bl    SystemInit

    /* Call main */
    bl    main

    /* If main returns, loop forever */
    b     .

    .size Reset_Handler, .-Reset_Handler

/* ---- Default Handler (infinite loop) ---- */
    .section .text.Default_Handler, "ax", %progbits
Default_Handler:
    b     .
    .size Default_Handler, .-Default_Handler

/* ---- Vector Table ---- */
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack                   /* Initial SP */
    .word Reset_Handler             /* Reset */
    .word NMI_Handler               /* NMI */
    .word HardFault_Handler         /* Hard Fault */
    .word MemManage_Handler         /* MPU Fault */
    .word BusFault_Handler          /* Bus Fault */
    .word UsageFault_Handler        /* Usage Fault */
    .word 0                         /* Reserved */
    .word 0                         /* Reserved */
    .word 0                         /* Reserved */
    .word 0                         /* Reserved */
    .word SVC_Handler               /* SVCall */
    .word DebugMon_Handler          /* Debug Monitor */
    .word 0                         /* Reserved */
    .word PendSV_Handler            /* PendSV */
    .word SysTick_Handler           /* SysTick */

    /* STM32F407 External Interrupts */
    .word WWDG_IRQHandler           /* 0:  Window Watchdog */
    .word PVD_IRQHandler            /* 1:  PVD */
    .word TAMP_STAMP_IRQHandler     /* 2:  Tamper/Timestamp */
    .word RTC_WKUP_IRQHandler       /* 3:  RTC Wakeup */
    .word FLASH_IRQHandler          /* 4:  Flash */
    .word RCC_IRQHandler            /* 5:  RCC */
    .word EXTI0_IRQHandler          /* 6:  EXTI Line 0 */
    .word EXTI1_IRQHandler          /* 7:  EXTI Line 1 */
    .word EXTI2_IRQHandler          /* 8:  EXTI Line 2 */
    .word EXTI3_IRQHandler          /* 9:  EXTI Line 3 */
    .word EXTI4_IRQHandler          /* 10: EXTI Line 4 */
    .word DMA1_Stream0_IRQHandler   /* 11: DMA1 Stream 0 */
    .word DMA1_Stream1_IRQHandler   /* 12: DMA1 Stream 1 */
    .word DMA1_Stream2_IRQHandler   /* 13: DMA1 Stream 2 */
    .word DMA1_Stream3_IRQHandler   /* 14: DMA1 Stream 3 */
    .word DMA1_Stream4_IRQHandler   /* 15: DMA1 Stream 4 */
    .word DMA1_Stream5_IRQHandler   /* 16: DMA1 Stream 5 */
    .word DMA1_Stream6_IRQHandler   /* 17: DMA1 Stream 6 */
    .word ADC_IRQHandler            /* 18: ADC1/2/3 */
    .word CAN1_TX_IRQHandler        /* 19: CAN1 TX */
    .word CAN1_RX0_IRQHandler       /* 20: CAN1 RX0 */
    .word CAN1_RX1_IRQHandler       /* 21: CAN1 RX1 */
    .word CAN1_SCE_IRQHandler       /* 22: CAN1 SCE */
    .word EXTI9_5_IRQHandler        /* 23: EXTI Lines 5-9 */
    .word TIM1_BRK_TIM9_IRQHandler  /* 24: TIM1 Break / TIM9 */
    .word TIM1_UP_TIM10_IRQHandler  /* 25: TIM1 Update / TIM10 */
    .word TIM1_TRG_COM_TIM11_IRQHandler /* 26: TIM1 Trigger / TIM11 */
    .word TIM1_CC_IRQHandler        /* 27: TIM1 Capture Compare */
    .word TIM2_IRQHandler           /* 28: TIM2 */
    .word TIM3_IRQHandler           /* 29: TIM3 */
    .word TIM4_IRQHandler           /* 30: TIM4 */
    .word I2C1_EV_IRQHandler        /* 31: I2C1 Event */
    .word I2C1_ER_IRQHandler        /* 32: I2C1 Error */
    .word I2C2_EV_IRQHandler        /* 33: I2C2 Event */
    .word I2C2_ER_IRQHandler        /* 34: I2C2 Error */
    .word SPI1_IRQHandler           /* 35: SPI1 */
    .word SPI2_IRQHandler           /* 36: SPI2 */
    .word USART1_IRQHandler         /* 37: USART1 */
    .word USART2_IRQHandler         /* 38: USART2 */
    .word USART3_IRQHandler         /* 39: USART3 */
    .word EXTI15_10_IRQHandler      /* 40: EXTI Lines 10-15 */
    .word RTC_Alarm_IRQHandler      /* 41: RTC Alarm */
    .word OTG_FS_WKUP_IRQHandler    /* 42: USB OTG FS Wakeup */
    .word TIM8_BRK_TIM12_IRQHandler /* 43: TIM8 Break / TIM12 */
    .word TIM8_UP_TIM13_IRQHandler  /* 44: TIM8 Update / TIM13 */
    .word TIM8_TRG_COM_TIM14_IRQHandler /* 45: TIM8 Trigger / TIM14 */
    .word TIM8_CC_IRQHandler        /* 46: TIM8 Capture Compare */
    .word DMA1_Stream7_IRQHandler   /* 47: DMA1 Stream 7 */
    .word FSMC_IRQHandler           /* 48: FSMC */
    .word SDIO_IRQHandler           /* 49: SDIO */
    .word TIM5_IRQHandler           /* 50: TIM5 */
    .word SPI3_IRQHandler           /* 51: SPI3 */
    .word UART4_IRQHandler          /* 52: UART4 */
    .word UART5_IRQHandler          /* 53: UART5 */
    .word TIM6_DAC_IRQHandler       /* 54: TIM6 / DAC1&2 underrun */
    .word TIM7_IRQHandler           /* 55: TIM7 */
    .word DMA2_Stream0_IRQHandler   /* 56: DMA2 Stream 0 */
    .word DMA2_Stream1_IRQHandler   /* 57: DMA2 Stream 1 */
    .word DMA2_Stream2_IRQHandler   /* 58: DMA2 Stream 2 */
    .word DMA2_Stream3_IRQHandler   /* 59: DMA2 Stream 3 */
    .word DMA2_Stream4_IRQHandler   /* 60: DMA2 Stream 4 */
    .word ETH_IRQHandler            /* 61: Ethernet */
    .word ETH_WKUP_IRQHandler       /* 62: Ethernet Wakeup */
    .word CAN2_TX_IRQHandler        /* 63: CAN2 TX */
    .word CAN2_RX0_IRQHandler       /* 64: CAN2 RX0 */
    .word CAN2_RX1_IRQHandler       /* 65: CAN2 RX1 */
    .word CAN2_SCE_IRQHandler       /* 66: CAN2 SCE */
    .word OTG_FS_IRQHandler         /* 67: USB OTG FS */
    .word DMA2_Stream5_IRQHandler   /* 68: DMA2 Stream 5 */
    .word DMA2_Stream6_IRQHandler   /* 69: DMA2 Stream 6 */
    .word DMA2_Stream7_IRQHandler   /* 70: DMA2 Stream 7 */
    .word USART6_IRQHandler         /* 71: USART6 */
    .word I2C3_EV_IRQHandler        /* 72: I2C3 Event */
    .word I2C3_ER_IRQHandler        /* 73: I2C3 Error */
    .word OTG_HS_EP1_OUT_IRQHandler /* 74: USB OTG HS EP1 OUT */
    .word OTG_HS_EP1_IN_IRQHandler  /* 75: USB OTG HS EP1 IN */
    .word OTG_HS_WKUP_IRQHandler    /* 76: USB OTG HS Wakeup */
    .word OTG_HS_IRQHandler         /* 77: USB OTG HS */
    .word DCMI_IRQHandler           /* 78: DCMI */
    .word 0                         /* 79: Reserved */
    .word HASH_RNG_IRQHandler       /* 80: Hash / RNG */
    .word FPU_IRQHandler            /* 81: FPU */

/* ---- Weak aliases for default handlers ---- */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler
    .weak TAMP_STAMP_IRQHandler
    .thumb_set TAMP_STAMP_IRQHandler, Default_Handler
    .weak RTC_WKUP_IRQHandler
    .thumb_set RTC_WKUP_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler, Default_Handler
    .weak EXTI1_IRQHandler
    .thumb_set EXTI1_IRQHandler, Default_Handler
    .weak EXTI2_IRQHandler
    .thumb_set EXTI2_IRQHandler, Default_Handler
    .weak EXTI3_IRQHandler
    .thumb_set EXTI3_IRQHandler, Default_Handler
    .weak EXTI4_IRQHandler
    .thumb_set EXTI4_IRQHandler, Default_Handler
    .weak DMA1_Stream0_IRQHandler
    .thumb_set DMA1_Stream0_IRQHandler, Default_Handler
    .weak DMA1_Stream1_IRQHandler
    .thumb_set DMA1_Stream1_IRQHandler, Default_Handler
    .weak DMA1_Stream2_IRQHandler
    .thumb_set DMA1_Stream2_IRQHandler, Default_Handler
    .weak DMA1_Stream3_IRQHandler
    .thumb_set DMA1_Stream3_IRQHandler, Default_Handler
    .weak DMA1_Stream4_IRQHandler
    .thumb_set DMA1_Stream4_IRQHandler, Default_Handler
    .weak DMA1_Stream5_IRQHandler
    .thumb_set DMA1_Stream5_IRQHandler, Default_Handler
    .weak DMA1_Stream6_IRQHandler
    .thumb_set DMA1_Stream6_IRQHandler, Default_Handler
    .weak ADC_IRQHandler
    .thumb_set ADC_IRQHandler, Default_Handler
    .weak CAN1_TX_IRQHandler
    .thumb_set CAN1_TX_IRQHandler, Default_Handler
    .weak CAN1_RX0_IRQHandler
    .thumb_set CAN1_RX0_IRQHandler, Default_Handler
    .weak CAN1_RX1_IRQHandler
    .thumb_set CAN1_RX1_IRQHandler, Default_Handler
    .weak CAN1_SCE_IRQHandler
    .thumb_set CAN1_SCE_IRQHandler, Default_Handler
    .weak EXTI9_5_IRQHandler
    .thumb_set EXTI9_5_IRQHandler, Default_Handler
    .weak TIM1_BRK_TIM9_IRQHandler
    .thumb_set TIM1_BRK_TIM9_IRQHandler, Default_Handler
    .weak TIM1_UP_TIM10_IRQHandler
    .thumb_set TIM1_UP_TIM10_IRQHandler, Default_Handler
    .weak TIM1_TRG_COM_TIM11_IRQHandler
    .thumb_set TIM1_TRG_COM_TIM11_IRQHandler, Default_Handler
    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler
    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler
    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler
    .weak TIM4_IRQHandler
    .thumb_set TIM4_IRQHandler, Default_Handler
    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler, Default_Handler
    .weak I2C1_ER_IRQHandler
    .thumb_set I2C1_ER_IRQHandler, Default_Handler
    .weak I2C2_EV_IRQHandler
    .thumb_set I2C2_EV_IRQHandler, Default_Handler
    .weak I2C2_ER_IRQHandler
    .thumb_set I2C2_ER_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler
    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
    .weak USART3_IRQHandler
    .thumb_set USART3_IRQHandler, Default_Handler
    .weak EXTI15_10_IRQHandler
    .thumb_set EXTI15_10_IRQHandler, Default_Handler
    .weak RTC_Alarm_IRQHandler
    .thumb_set RTC_Alarm_IRQHandler, Default_Handler
    .weak OTG_FS_WKUP_IRQHandler
    .thumb_set OTG_FS_WKUP_IRQHandler, Default_Handler
    .weak TIM8_BRK_TIM12_IRQHandler
    .thumb_set TIM8_BRK_TIM12_IRQHandler, Default_Handler
    .weak TIM8_UP_TIM13_IRQHandler
    .thumb_set TIM8_UP_TIM13_IRQHandler, Default_Handler
    .weak TIM8_TRG_COM_TIM14_IRQHandler
    .thumb_set TIM8_TRG_COM_TIM14_IRQHandler, Default_Handler
    .weak TIM8_CC_IRQHandler
    .thumb_set TIM8_CC_IRQHandler, Default_Handler
    .weak DMA1_Stream7_IRQHandler
    .thumb_set DMA1_Stream7_IRQHandler, Default_Handler
    .weak FSMC_IRQHandler
    .thumb_set FSMC_IRQHandler, Default_Handler
    .weak SDIO_IRQHandler
    .thumb_set SDIO_IRQHandler, Default_Handler
    .weak TIM5_IRQHandler
    .thumb_set TIM5_IRQHandler, Default_Handler
    .weak SPI3_IRQHandler
    .thumb_set SPI3_IRQHandler, Default_Handler
    .weak UART4_IRQHandler
    .thumb_set UART4_IRQHandler, Default_Handler
    .weak UART5_IRQHandler
    .thumb_set UART5_IRQHandler, Default_Handler
    .weak TIM6_DAC_IRQHandler
    .thumb_set TIM6_DAC_IRQHandler, Default_Handler
    .weak TIM7_IRQHandler
    .thumb_set TIM7_IRQHandler, Default_Handler
    .weak DMA2_Stream0_IRQHandler
    .thumb_set DMA2_Stream0_IRQHandler, Default_Handler
    .weak DMA2_Stream1_IRQHandler
    .thumb_set DMA2_Stream1_IRQHandler, Default_Handler
    .weak DMA2_Stream2_IRQHandler
    .thumb_set DMA2_Stream2_IRQHandler, Default_Handler
    .weak DMA2_Stream3_IRQHandler
    .thumb_set DMA2_Stream3_IRQHandler, Default_Handler
    .weak DMA2_Stream4_IRQHandler
    .thumb_set DMA2_Stream4_IRQHandler, Default_Handler
    .weak ETH_IRQHandler
    .thumb_set ETH_IRQHandler, Default_Handler
    .weak ETH_WKUP_IRQHandler
    .thumb_set ETH_WKUP_IRQHandler, Default_Handler
    .weak CAN2_TX_IRQHandler
    .thumb_set CAN2_TX_IRQHandler, Default_Handler
    .weak CAN2_RX0_IRQHandler
    .thumb_set CAN2_RX0_IRQHandler, Default_Handler
    .weak CAN2_RX1_IRQHandler
    .thumb_set CAN2_RX1_IRQHandler, Default_Handler
    .weak CAN2_SCE_IRQHandler
    .thumb_set CAN2_SCE_IRQHandler, Default_Handler
    .weak OTG_FS_IRQHandler
    .thumb_set OTG_FS_IRQHandler, Default_Handler
    .weak DMA2_Stream5_IRQHandler
    .thumb_set DMA2_Stream5_IRQHandler, Default_Handler
    .weak DMA2_Stream6_IRQHandler
    .thumb_set DMA2_Stream6_IRQHandler, Default_Handler
    .weak DMA2_Stream7_IRQHandler
    .thumb_set DMA2_Stream7_IRQHandler, Default_Handler
    .weak USART6_IRQHandler
    .thumb_set USART6_IRQHandler, Default_Handler
    .weak I2C3_EV_IRQHandler
    .thumb_set I2C3_EV_IRQHandler, Default_Handler
    .weak I2C3_ER_IRQHandler
    .thumb_set I2C3_ER_IRQHandler, Default_Handler
    .weak OTG_HS_EP1_OUT_IRQHandler
    .thumb_set OTG_HS_EP1_OUT_IRQHandler, Default_Handler
    .weak OTG_HS_EP1_IN_IRQHandler
    .thumb_set OTG_HS_EP1_IN_IRQHandler, Default_Handler
    .weak OTG_HS_WKUP_IRQHandler
    .thumb_set OTG_HS_WKUP_IRQHandler, Default_Handler
    .weak OTG_HS_IRQHandler
    .thumb_set OTG_HS_IRQHandler, Default_Handler
    .weak DCMI_IRQHandler
    .thumb_set DCMI_IRQHandler, Default_Handler
    .weak HASH_RNG_IRQHandler
    .thumb_set HASH_RNG_IRQHandler, Default_Handler
    .weak FPU_IRQHandler
    .thumb_set FPU_IRQHandler, Default_Handler
