/*
 * Copyright (C) 2017 Marvell International Ltd.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 * https://spdx.org/licenses
 */

#include <ap810_aro.h>
#include <ap810_setup.h>
#include <ap810_init_clocks.h>
#include <eawg.h>
#include <errno.h>
#include <debug.h>
#include <mmio.h>
#include <plat_def.h>
#include <stdio.h>

/* PLL's registers with local base address since each AP has its own EAWG*/
#define PLL_RING_ADDRESS	(MVEBU_DFX_SAR_LOCAL_AP + 0x2F0)
#define PLL_IO_ADDRESS		(MVEBU_DFX_SAR_LOCAL_AP + 0x2F8)
#define PLL_PIDI_ADDRESS	(MVEBU_DFX_SAR_LOCAL_AP + 0x310)
#define PLL_DSS_ADDRESS		(MVEBU_DFX_SAR_LOCAL_AP + 0x300)

/* frequencies values */
#define PLL_FREQ_3000		0x2D477001 /* 3000 */
#define PLL_FREQ_2700		0x2B06B001 /* 2700 */
#define PLL_FREQ_2400		0x2AE5F001 /* 2400 */
#define PLL_FREQ_2000		0x2FC9F002 /* 2000 */
#define PLL_FREQ_1800		0x2D88F002 /* 1800 */
#define PLL_FREQ_1600		0x2D47F002 /* 1600 */
#define PLL_FREQ_1466		0x3535F012 /* 1466.5 */
#define PLL_FREQ_1400		0x2D26F002 /* 1400 */
#define PLL_FREQ_1333		0x3313F012 /* 1333.5 */
#define PLL_FREQ_1300		0x2B067002 /* 1300 */
#define PLL_FREQ_1200		0x2AE5F002 /* 1200 */
#define PLL_FREQ_1100           0x2AC57002 /* 1100 */
#define PLL_FREQ_1066		0x30CFF012 /* 1066 */
#define PLL_FREQ_1000		0x2AC4F002 /* 1000 */
#define PLL_FREQ_800		0x2883F002 /* 800 */

/* EAWG functionality */
#define SCRATCH_PAD_LOCAL_REG	(MVEBU_REGS_BASE_LOCAL_AP + 0x6F43E0)
#define CPU_WAKEUP_COMMAND(ap)	(MVEBU_CCU_LOCL_CNTL_BASE(ap) + 0x80)

/* fetching target frequencies */
#define SCRATCH_PAD_FREQ_REG	SCRATCH_PAD_ADDR(0, 1)
#define EFUSE_FREQ_REG		(MVEBU_AP_MPP_REGS(0, 0) + 0x410)
#define EFUSE_FREQ_OFFSET	24
#define EFUSE_FREQ_MASK		(0x1 << EFUSE_FREQ_OFFSET)

#define SAR_SUPPORTED_TABLES	2
#define SAR_SUPPORTED_OPTIONS	8

enum pll_type {
	RING,
	IO,
	PIDI,
	DSS,
	PLL_LAST,
	CPU_FREQ,
	DDR_FREQ,
};

unsigned int pll_freq_tables[SAR_SUPPORTED_TABLES]
			    [SAR_SUPPORTED_OPTIONS]
			    [PLL_LAST + 2] = {
	{
		/* RING, IO, PIDI, DSS, CPU_FREQ*/
		{PLL_FREQ_1200, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_800,
		 TARGET_FREQ_1600, DDR_FREQ_800},
		{PLL_FREQ_1200, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2000, DDR_FREQ_1200},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2000, DDR_FREQ_1200},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2200, DDR_FREQ_1200},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1333,
		 TARGET_FREQ_2200, DDR_FREQ_1333},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2500, DDR_FREQ_1200},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1466,
		 TARGET_FREQ_2500, DDR_FREQ_1466},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1600,
		 TARGET_FREQ_2700, DDR_FREQ_1600},
	},
	{
		/* RING, IO, PIDI, DSS, CPU_FREQ*/
		{PLL_FREQ_800, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_800,
		 TARGET_FREQ_1200, DDR_FREQ_800},
		{PLL_FREQ_1000, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_1800, DDR_FREQ_1200},
		{PLL_FREQ_1100, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_1800, DDR_FREQ_1200},
		{PLL_FREQ_1200, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_1800, DDR_FREQ_1200},
		{PLL_FREQ_1400, PLL_FREQ_1000, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_1800, DDR_FREQ_1200},
		{PLL_FREQ_1100, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2000, DDR_FREQ_1200},
		{PLL_FREQ_1200, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2000, DDR_FREQ_1200},
		{PLL_FREQ_1300, PLL_FREQ_800, PLL_FREQ_1000, PLL_FREQ_1200,
		 TARGET_FREQ_2000, DDR_FREQ_1200},
	},
};

unsigned int pll_base_address[PLL_LAST] = {
	PLL_RING_ADDRESS, /* RING */
	PLL_IO_ADDRESS, /* IO */
	PLL_PIDI_ADDRESS, /* PIDI */
	PLL_DSS_ADDRESS, /* DSS */
};

/* read efuse value which device if it's low/high frequency
 * fetch frequency values from appropriate table
 */
void clocks_fetch_options(uint32_t *freq_mode, uint32_t *clk_index)
{
	/* fetch eFuse value and device whether it's H/L */
	*freq_mode = mmio_read_32(EFUSE_FREQ_REG);
	*freq_mode &= EFUSE_FREQ_MASK;
	*freq_mode = (*freq_mode) >> EFUSE_FREQ_OFFSET;

	/* in A0 sampled-at-reset register is not functional */
	if (ap810_rev_id_get(MVEBU_AP0) == MVEBU_AP810_REV_ID_A0)
		*clk_index = (mmio_read_32(SCRATCH_PAD_ADDR(0, 1)) & 0x7);
	else
		ERROR("sample at reset register is missing - failed to configure clocks\n");
}

/* prepares the transactions to be send to each AP's EAWG FIFO */
static int clocks_prepare_transactions(uint32_t *plls_clocks_vals,
				       struct eawg_transaction *trans_array,
				       struct eawg_transaction *primary_cpu_trans)
{
	int pll, i;

	/*build transactions array to be written to EAWGs' FIFO */
	for (pll = 0, i = 0 ; pll < PLL_LAST ; pll++) {

		if (pll_base_address[pll] == -1) {
			printf("PLL number %d value is not intialized", pll);
			return -1;
		}

		/* For each PLL type there's 4 transactions to be written */
		/* setting use RF bit to 1 */
		trans_array[i].address = pll_base_address[pll] + 0x4;
		trans_array[i].data = 0x200;
		trans_array[i].delay = 0x1;

		i++;

		/* ring bypass while still in RF conf */
		trans_array[i].address = pll_base_address[pll] + 0x4;
		trans_array[i].data = 0x201;
		trans_array[i].delay = 0x1;

		i++;

		/* setting the new desired frequency */
		trans_array[i].address = pll_base_address[pll];
		trans_array[i].data = plls_clocks_vals[pll];
		trans_array[i].delay = 0x1;

		i++;

		/* turning off ring bypass while leaving RF conf on */
		trans_array[i].address = pll_base_address[pll] + 0x4;
		trans_array[i].data = 0x200;
		trans_array[i].delay = 0x0;

		i++;
	}

	/* one extra transaction to write to a scratch-pad register in each AP */
	trans_array[i].address = SCRATCH_PAD_LOCAL_REG;
	trans_array[i].data = 0x1;
	trans_array[i].delay = 0x0;

	i = 0;

	/* Loading a wake up command to primary CPU */
	primary_cpu_trans[i].address = CPU_WAKEUP_COMMAND(0);
	primary_cpu_trans[i].data = 0x1;
	primary_cpu_trans[i].delay = 0x3;

	return 0;
}

/* 1.constructs array of transactions built on the configuration
 *   chosen in SAR.
 * 2.for each AP, load transactions using CPU 0 to APs' EAWG FIFO.
 * 3.trigger each AP's EAWG and finally CPU 0 in AP0.
 * 4.when CPU0 wakes from EAWG, read SCRATCH_PAD registers to check
 *   whether all APs' EAWG finished.
 */
int ap810_clocks_init(int ap_count)
{
	/* for each PLL there's 4 transactions and another transaction for
	 * writing to each AP's scratch-pad register to notify EAWG is done.
	 *
	 * extra 3 transactions are needed
	 * for CPU0 in AP0:
	 * 1.reseting the fast2slow ips (2 transactions).
	 * 2.loading a wake up command to CPU0 (1 transaction).
	 *
	 * plls_clocks_vals contains info about each PLL clock value
	 * and one extra value on cpu freq for ARO use.
	 */
	struct eawg_transaction trans_array[(PLL_LAST * TRANS_PER_PLL) + 1];
	struct eawg_transaction primary_cpu_trans[PRIMARY_CPU_TRANS];
	uint32_t plls_clocks_vals[PLL_LAST];
	uint32_t freq_mode, clk_config;
	int cpu_clock_val, ddr_clock_option;
	int ap;

	/* check if the total number of transactions doesn't exceeds EAWG's
	 * FIFO capacity.
	 */
	if (((PLL_LAST * TRANS_PER_PLL) + PRIMARY_CPU_TRANS) > (MAX_TRANSACTIONS - 1)) {
		printf("transactions number exceeded fifo size\n");
		return -1;
	}

	/* fetch frequency option*/
	clocks_fetch_options(&freq_mode, &clk_config);
	if (clk_config < 0 || clk_config > (SAR_SUPPORTED_OPTIONS - 1)) {
		printf("clk option 0x%d is not supported\n", clk_config);
		return -1;
	}

	plls_clocks_vals[RING] = pll_freq_tables[freq_mode][clk_config][RING];
	plls_clocks_vals[IO] = pll_freq_tables[freq_mode][clk_config][IO];
	plls_clocks_vals[PIDI] = pll_freq_tables[freq_mode][clk_config][PIDI];
	plls_clocks_vals[DSS] = pll_freq_tables[freq_mode][clk_config][DSS];
	cpu_clock_val = pll_freq_tables[freq_mode][clk_config][CPU_FREQ - 1];
	ddr_clock_option = pll_freq_tables[freq_mode][clk_config][DDR_FREQ - 1];

	plat_dram_freq_update(ddr_clock_option);

	if (clocks_prepare_transactions(plls_clocks_vals, trans_array, primary_cpu_trans))
		return -1;


	/* write transactions to each APs' EAWG FIFO */
	for (ap = 0 ; ap < ap_count ; ap++) {
		if (eawg_load_transactions(trans_array, (PLL_LAST * TRANS_PER_PLL) + 1, ap)) {
			printf("CPU0 couldn't load all transactions to AP%d EAWG FIFO\n", ap);
			return -1;
		}
		if (eawg_load_transactions(primary_cpu_trans, PRIMARY_CPU_TRANS, ap)) {
			printf("CPU0 couldn't load all transactions to AP%d EAWG FIFO\n", ap);
			return -1;
		}
	}

	/* trigger each AP's EAWG and finally CPU 0 in AP0,
	 * after this step all CPUs are in WFE status.
	 */
	for (ap = ap_count - 1 ; ap >= 0 ; ap--)
		eawg_start(ap);

	/* when CPU0 wakes from EAWG, read SCRATCH_PAD registers to check
	 * whether all APs' EAWG finished.
	 */
	for (ap = ap_count - 1 ; ap >= 0 ; ap--) {
		if (!eawg_check_is_done(SCRATCH_PAD_ADDR(ap, 0), ap))
			disable_eawg(ap);
	}

	/* configure CPU's frequencies in suppored AP's */
	for (ap = 0 ; ap < ap_count ; ap++)
		ap810_aro_init(cpu_clock_val, ap);

	return 0;
}