
#include "ti_clocktree_pll_config.h"
extern uint32_t gR5ClockFrequency;





void Bootloader_socConfigurePll(void)
{

    SOC_RcmPllHsDivOutConfig hsDivCfg;

    uint32_t r5ClkSrc_restore;

    /* Pre Requisite Sequence to relock core pll needs to be done */
    r5ClkSrc_restore = SOC_rcmCoreApllRelockPreRequisite();

        hsDivCfg.hsDivOutFreqHz[0] = SOC_RCM_FREQ_MHZ2HZ(400U);
    hsDivCfg.hsDivOutFreqHz[1] = SOC_RCM_FREQ_MHZ2HZ(500U);
    hsDivCfg.hsDivOutFreqHz[2] = SOC_RCM_FREQ_MHZ2HZ(400U);
    hsDivCfg.hsdivOutEnMask = RCM_PLL_HSDIV_OUTPUT_ENABLE_0 | RCM_PLL_HSDIV_OUTPUT_ENABLE_1 | RCM_PLL_HSDIV_OUTPUT_ENABLE_2;
    SOC_rcmCoreApllConfig(RCM_PLL_FOUT_FREQID_CLK_2000MHZ, &hsDivCfg);

    hsDivCfg.hsDivOutFreqHz[0] = SOC_RCM_FREQ_MHZ2HZ(160U);
    hsDivCfg.hsDivOutFreqHz[1] = SOC_RCM_FREQ_MHZ2HZ(192U);
    hsDivCfg.hsdivOutEnMask = RCM_PLL_HSDIV_OUTPUT_ENABLE_0 | RCM_PLL_HSDIV_OUTPUT_ENABLE_1;
    SOC_rcmPerApllConfig(RCM_PLL_FOUT_FREQID_CLK_1920MHZ, &hsDivCfg);


    /* Restore R5F source clock*/
    SOC_rcmSetR5ClockSource(r5ClkSrc_restore);
}