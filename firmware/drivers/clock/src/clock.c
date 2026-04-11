#include "S32K144.h"
#include "clock.h"

void SOSC_init_8MHz(void)
{
    /* SOSCDIV1 = 1, SOSCDIV2 = 1 */
    IP_SCG->SOSCDIV = 0x00000101U;

    /* RANGE = 2 (1 MHz - 8 MHz), HGO = 0, EREFS = 1 */
    IP_SCG->SOSCCFG = 0x00000024U;

    /* Ensure SOSCCSR is unlocked */
    while ((IP_SCG->SOSCCSR & SCG_SOSCCSR_LK_MASK) != 0U)
    {
    }

    /* Enable SOSC */
    IP_SCG->SOSCCSR = 0x00000001U;

    /* Wait until SOSC valid */
    while ((IP_SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0U)
    {
    }
}

void SPLL_init_160MHz(void)
{
    /* Ensure SPLLCSR is unlocked */
    while ((IP_SCG->SPLLCSR & SCG_SPLLCSR_LK_MASK) != 0U)
    {
    }

    /* Disable SPLL before configuration */
    IP_SCG->SPLLCSR = 0x00000000U;

    /* SPLLDIV1 = /2, SPLLDIV2 = /4 */
    IP_SCG->SPLLDIV = 0x00000302U;

    /* PREDIV = 0 (/1), MULT = 24 (+16 => x40), SPLL_CLK = 8 MHz / 1 x 40 / 2 = 160 MHz */
    IP_SCG->SPLLCFG = 0x00180000U;

    /* Ensure SPLLCSR is unlocked */
    while ((IP_SCG->SPLLCSR & SCG_SPLLCSR_LK_MASK) != 0U)
    {
    }

    /* Enable SPLL */
    IP_SCG->SPLLCSR = 0x00000001U;

    /* Wait until SPLL valid */
    while ((IP_SCG->SPLLCSR & SCG_SPLLCSR_SPLLVLD_MASK) == 0U)
    {
    }
}

void NormalRUNmode_80MHz(void)
{
    /* SPLL as system clock source
       DIVCORE = /2  => 80 MHz core clock
       DIVBUS  = /2  => 40 MHz bus clock
       DIVSLOW = /3  => 26.67 MHz flash/slow clock
    */
    IP_SCG->RCCR = SCG_RCCR_SCS(6) | SCG_RCCR_DIVCORE(0b01) | SCG_RCCR_DIVBUS(0b01) | SCG_RCCR_DIVSLOW(0b10);

    /* Wait until system clock source = SPLL */
    while (((IP_SCG->CSR & SCG_CSR_SCS_MASK) >> SCG_CSR_SCS_SHIFT) != 6U)
    {
    }
}