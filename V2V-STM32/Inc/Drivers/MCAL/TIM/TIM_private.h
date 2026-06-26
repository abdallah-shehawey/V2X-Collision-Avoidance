    /**
 * @file    TIM_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Timer (TIM) Private Header File
 * @details This file contains private macros and register definitions for the Timer driver.
 */

#ifndef TIM_PRIVATE_H_
#define TIM_PRIVATE_H_

/**************************************         TIM Register Bits
 * ******************************************/

/* TIMx_CR1 */
#define TIM_CR1_CEN     0   /* Counter enable */
#define TIM_CR1_UDIS    1   /* Update disable */
#define TIM_CR1_URS     2   /* Update request source */
#define TIM_CR1_OPM     3   /* One-pulse mode */
#define TIM_CR1_DIR     4   /* Direction */
#define TIM_CR1_CMS     5   /* Center-aligned mode selection */
#define TIM_CR1_ARPE    7   /* Auto-reload preload enable */
#define TIM_CR1_CKD     8   /* Clock division */

/* TIMx_CR2 */
#define TIM_CR2_CCPC    0   /* Capture/compare preloaded control */
#define TIM_CR2_CCUS    2   /* Capture/compare control update selection */
#define TIM_CR2_CCDS    3   /* Capture/compare DMA selection */
#define TIM_CR2_MMS     4   /* Master mode selection */
#define TIM_CR2_TI1S    7   /* TI1 selection */

/* TIMx_SMCR */
#define TIM_SMCR_SMS    0   /* Slave mode selection */
#define TIM_SMCR_TS     4   /* Trigger selection */
#define TIM_SMCR_MSM    7   /* Master/Slave mode */
#define TIM_SMCR_ETF    8   /* External trigger filter */
#define TIM_SMCR_ETPS   12  /* External trigger prescaler */
#define TIM_SMCR_ECE    14  /* External clock enable */
#define TIM_SMCR_ETP    15  /* External trigger polarity */

/* TIMx_DIER */
#define TIM_DIER_UIE    0   /* Update interrupt enable */
#define TIM_DIER_CC1IE  1   /* Capture/Compare 1 interrupt enable */
#define TIM_DIER_CC2IE  2   /* Capture/Compare 2 interrupt enable */
#define TIM_DIER_CC3IE  3   /* Capture/Compare 3 interrupt enable */
#define TIM_DIER_CC4IE  4   /* Capture/Compare 4 interrupt enable */
#define TIM_DIER_COMIE  5   /* COM interrupt enable */
#define TIM_DIER_TIE    6   /* Trigger interrupt enable */
#define TIM_DIER_BIE    7   /* Break interrupt enable */
#define TIM_DIER_UDE    8   /* Update DMA request enable */
#define TIM_DIER_CC1DE  9   /* Capture/Compare 1 DMA request enable */
#define TIM_DIER_CC2DE  10  /* Capture/Compare 2 DMA request enable */
#define TIM_DIER_CC3DE  11  /* Capture/Compare 3 DMA request enable */
#define TIM_DIER_CC4DE  12  /* Capture/Compare 4 DMA request enable */
#define TIM_DIER_COMDE  13  /* COM DMA request enable */
#define TIM_DIER_TDE    14  /* Trigger DMA request enable */

/* TIMx_SR */
#define TIM_SR_UIF      0   /* Update interrupt flag */
#define TIM_SR_CC1IF    1   /* Capture/Compare 1 interrupt flag */
#define TIM_SR_CC2IF    2   /* Capture/Compare 2 interrupt flag */
#define TIM_SR_CC3IF    3   /* Capture/Compare 3 interrupt flag */
#define TIM_SR_CC4IF    4   /* Capture/Compare 4 interrupt flag */
#define TIM_SR_COMIF    5   /* COM interrupt flag */
#define TIM_SR_TIF      6   /* Trigger interrupt flag */
#define TIM_SR_BIF      7   /* Break interrupt flag */
#define TIM_SR_CC1OF    9   /* Capture/Compare 1 overcapture flag */
#define TIM_SR_CC2OF    10  /* Capture/Compare 2 overcapture flag */
#define TIM_SR_CC3OF    11  /* Capture/Compare 3 overcapture flag */
#define TIM_SR_CC4OF    12  /* Capture/Compare 4 overcapture flag */

/* TIMx_EGR */
#define TIM_EGR_UG      0   /* Update generation */
#define TIM_EGR_CC1G    1   /* Capture/Compare 1 generation */
#define TIM_EGR_CC2G    2   /* Capture/Compare 2 generation */
#define TIM_EGR_CC3G    3   /* Capture/Compare 3 generation */
#define TIM_EGR_CC4G    4   /* Capture/Compare 4 generation */
#define TIM_EGR_COMG    5   /* Capture/Compare control update generation */
#define TIM_EGR_TG      6   /* Trigger generation */
#define TIM_EGR_BG      7   /* Break generation */

/* TIMx_CCMR1 */
#define TIM_CCMR1_CC1S  0   /* Capture/Compare 1 selection */
#define TIM_CCMR1_OC1FE 2   /* Output Compare 1 fast enable */
#define TIM_CCMR1_OC1PE 3   /* Output Compare 1 preload enable */
#define TIM_CCMR1_OC1M  4   /* Output Compare 1 mode */
#define TIM_CCMR1_OC1CE 7   /* Output Compare 1 clear enable */
#define TIM_CCMR1_CC2S  8   /* Capture/Compare 2 selection */
#define TIM_CCMR1_OC2FE 10  /* Output Compare 2 fast enable */
#define TIM_CCMR1_OC2PE 11  /* Output Compare 2 preload enable */
#define TIM_CCMR1_OC2M  12  /* Output Compare 2 mode */
#define TIM_CCMR1_OC2CE 15  /* Output Compare 2 clear enable */

/* TIMx_CCMR2 */
#define TIM_CCMR2_CC3S  0   /* Capture/Compare 3 selection */
#define TIM_CCMR2_OC3FE 2   /* Output Compare 3 fast enable */
#define TIM_CCMR2_OC3PE 3   /* Output Compare 3 preload enable */
#define TIM_CCMR2_OC3M  4   /* Output Compare 3 mode */
#define TIM_CCMR2_OC3CE 7   /* Output Compare 3 clear enable */
#define TIM_CCMR2_CC4S  8   /* Capture/Compare 4 selection */
#define TIM_CCMR2_OC4FE 10  /* Output Compare 4 fast enable */
#define TIM_CCMR2_OC4PE 11  /* Output Compare 4 preload enable */
#define TIM_CCMR2_OC4M  12  /* Output Compare 4 mode */
#define TIM_CCMR2_OC4CE 15  /* Output Compare 4 clear enable */

/* TIMx_CCER */
#define TIM_CCER_CC1E   0   /* Capture/Compare 1 output enable */
#define TIM_CCER_CC1P   1   /* Capture/Compare 1 output polarity */
#define TIM_CCER_CC1NE  2   /* Capture/Compare 1 complementary output enable */
#define TIM_CCER_CC1NP  3   /* Capture/Compare 1 complementary output polarity */
#define TIM_CCER_CC2E   4   /* Capture/Compare 2 output enable */
#define TIM_CCER_CC2P   5   /* Capture/Compare 2 output polarity */
#define TIM_CCER_CC2NE  6   /* Capture/Compare 2 complementary output enable */
#define TIM_CCER_CC2NP  7   /* Capture/Compare 2 complementary output polarity */
#define TIM_CCER_CC3E   8   /* Capture/Compare 3 output enable */
#define TIM_CCER_CC3P   9   /* Capture/Compare 3 output polarity */
#define TIM_CCER_CC3NE  10  /* Capture/Compare 3 complementary output enable */
#define TIM_CCER_CC3NP  11  /* Capture/Compare 3 complementary output polarity */
#define TIM_CCER_CC4E   12  /* Capture/Compare 4 output enable */
#define TIM_CCER_CC4P   13  /* Capture/Compare 4 output polarity */
#define TIM_CCER_CC4NP  15  /* Capture/Compare 4 complementary output polarity */

/* TIMx_BDTR */
#define TIM_BDTR_DTG    0   /* Dead-time generator setup */
#define TIM_BDTR_LOCK   8   /* Lock configuration */
#define TIM_BDTR_OSSI   10  /* Off-state selection for Idle mode */
#define TIM_BDTR_OSSR   11  /* Off-state selection for Run mode */
#define TIM_BDTR_BKE    12  /* Break enable */
#define TIM_BDTR_BKP    13  /* Break polarity */
#define TIM_BDTR_AOE    14  /* Automatic output enable */
#define TIM_BDTR_MOE    15  /* Main output enable */

#endif /* TIM_PRIVATE_H_ */
