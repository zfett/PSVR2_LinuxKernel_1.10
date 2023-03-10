/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/**
 * @file mtk_rbfc_reg.h
 * Header of mtk-rbfc.c
 */

#ifndef __MTK_RBFC_REG_H__
#define __MTK_RBFC_REG_H__

/* ----------------- Register Definitions ------------------- */
#define RBFC_FRAME_WIDTH				0x00000000
	#define CFG_FRAME_WIDTH				GENMASK(13, 0)
#define RBFC_FRAME_HEIGHT				0x00000004
	#define CFG_FRAME_HEIGHT			GENMASK(13, 0)
#define RBFC_RBF_ROW_STRIDE				0x00000008
	#define CFG_RBF_ROW_STRIDE			GENMASK(14, 0)
#define RBFC_RBF_ROW_NO					0x0000000c
	#define CFG_RBF_ROW_NO				GENMASK(13, 0)
#define RBFC_EN_RD_TH					0x00000010
	#define CFG_EN_RD_TH				GENMASK(12, 0)
#define RBFC_WR_FULL_ROW_TH				0x00000014
	#define CFG_WR_FULL_ROW_TH			GENMASK(12, 0)
	#define CFG_RPTR_DROP_CNT_OFSET			GENMASK(28, 16)
#define RBFC_CON					0x00000018
	#define CFG_SEL_SYSRAM				BIT(0)
	#define CFG_ADR_TRANS_EN			BIT(8)
	#define CFG_CUR_ROW_CNT_JMPX2			BIT(16)
	#define CFG_SNAPSHOT_TRIG_SEL			BIT(20)
	#define CFG_SNAPSHOT_SET			BIT(21)
	#define CFG_SNAPSHOT_SET_ENABLE			BIT(22)
	#define CFG_SNAPSHOT_OR_TIMEOUT_FLAG_CLEAR	BIT(23)
	#define CFG_DEBUG_VALUE_SEL			BIT(24)
	#define CFG_WAIT_FOR_WRITE_SOF_FLAG_CLEAR	BIT(25)
	#define CFG_INCOMPLETE_FRAME_TEST_CLEAR		BIT(26)
	#define CFG_INCOMPLETE_FRAME_TEST_ENABLE	BIT(27)
	#define CFG_INCOMPLETE_FRAME_TEST_FRAME		GENMASK(31, 28)
#define RBFC_RING_BUF_BASE				0x0000001c
	#define CFG_RBF_BASE				GENMASK(31, 0)
#define RBFC_1ST_ROW_EN_RD_TH				0x00000020
	#define CFG_1ST_ROW_EN_RD_TH			GENMASK(12, 0)
#define RBFC_CUR_ROW_CNT_INI_VAL			0x00000024
	#define CFG_CUR_ROW_CNT_INI_VAL			GENMASK(13, 0)
#define RBFC_IRQ_EN0					0x00000028
	#define CFG_N_LINE_RDY_IRQ_EN			BIT(0)
	#define CFG_WR_FULL_IRQ_EN			BIT(1)
	#define CFG_WEN_W_INCOMPLETE_IRQ_EN		BIT(2)
	#define CFG_WEN_R_INCOMPLETE_IRQ_EN		BIT(3)
	#define CFG_MODE_CHANGE_IRQ_EN			BIT(4)
	#define CFG_REN_R_INCOMPLETE_IRQ_EN		BIT(6)
	#define CFG_REN_W_INCOMPLETE_IRQ_EN		BIT(7)
	#define CFG_INCOMPLETE_FRAME_TEST_ROW		GENMASK(21, 8)
#define RBFC_IRQ_STATUS0				0x0000002c
	#define CFG_N_LINE_RDY_IRQ_STA			BIT(0)
	#define CFG_WR_FULL_IRQ_STA			BIT(1)
	#define CFG_WEN_W_INCOMPLETE_IRQ_STA		BIT(2)
	#define CFG_WEN_R_INCOMPLETE_IRQ_STA		BIT(3)
	#define CFG_MODE_CHANGE_IRQ_STA			BIT(4)
	#define CFG_REN_R_INCOMPLETE_IRQ_STA		BIT(6)
	#define CFG_REN_W_INCOMPLETE_IRQ_STA		BIT(7)
#define RBFC_CON1					0x00000030
	#define CFG_DIS_GLCOMD_AT_EOL			BIT(8)
	#define CFG_DIS_GLCOMD_AT_SOF			BIT(16)
#define RBFC_RFID					0x00000034
#define RBFC_CURR_FBASE					0x00000038
#define RBFC_PREV_FBASE					0x0000003c
#define RBFC_FID_CON					0x00000040
#define RBFC_READ_TMOUT_VAL				0x00000044
#define RBFC_READ_TMOUT_STA				0x00000048
#define RBFC_CON2					0x0000004c
	#define CFG_RBFC_EN				BIT(0)
	#define CFG_RBFC_CKEN				BIT(4)
	#define CFG_RWEN_CTL_DIS			BIT(8)
	#define CFG_R_SOF_AHEAD_W_SOF_EN		BIT(16)
	#define CFG_SOF_MERGE_DIS			BIT(20)
	#define CFG_READ_FOLLOW_WRITE_DIS		BIT(24)
#define RBFC_CON3					0x00000050
	#define CFG_W_ACTIVE_MASTER			GENMASK(3, 0)
	#define CFG_R_ACTIVE_MASTER			GENMASK(11, 8)
	#define CFG_ACTIVE_TIME_AFT_MRG_SOF		GENMASK(20, 16)
	#define CFG_WFULL_TIMEOUT_EN			BIT(24)
#define RBFC_RESET					0x00000054
	#define CFG_RBFC_RST				BIT(0)
#define RBFC_WFID					0x00000058
#define RBFC_ERRFLAG					0x0000005c
	#define VMRG_ERR_I_U0_U0			BIT(0)
	#define VMRG_ERR_I_U1_U1			BIT(1)
	#define RRSHAPE_ERR_W1_W1_STA			BIT(2)
	#define RRSHAPE_ERR_R1_R1_STA			BIT(3)
#define RBFC_CON4					0x00000060
	#define CFG_BYPASS_MODE_SET			BIT(0)
	#define CFG_BYPASS_MODE_CLR			BIT(8)
#define RBFC_CON5					0x00000064
	#define CFG_W_SOF_SEL				GENMASK(2, 0)
	#define CFG_R_SOF_SEL				GENMASK(10, 8)
	#define CFG_OFFSET_ADR_IN_SEL			GENMASK(14, 12)
	#define CFG_BYPASS_MODE_IN_SEL			GENMASK(18, 16)
	#define CFG_BYPASS_MODE_OUT_SEL			GENMASK(21, 20)
#define RBFC_GCE_CON					0x00000068
	#define CFG_GCE_W_FULL_EVENT_EN			BIT(0)
	#define CFG_WEN_W_GCE_INCOMPLETE_EVENT_EN	BIT(8)
	#define CFG_WEN_R_GCE_INCOMPLETE_EVENT_EN	BIT(9)
	#define CFG_GCE_WEN_W_OVER_TH_EVENT_EN		BIT(16)
	#define CFG_GCE_MODE_CHANGE_EVENT_EN		BIT(24)
	#define CFG_INCOMPLETE_USE_EOF			BIT(25)
#define RBFC_IRQ_EN1					0x0000006c
	#define CFG_MODE_CHANGE_IRQ_EN_1		BIT(0)
#define RBFC_IRQ_STATUS1				0x00000070
	#define CFG_MODE_CHANGE_IRQ_STA_1		BIT(0)
#define RBFC_W_OVER_TH_VAL				0x00000074
	#define CFG_W_OVER_TH_VAL			GENMASK(14, 0)
	#define CFG_W_OVER_TH_MUTEX_VAL			GENMASK(30, 16)
#define RBFC_GCE_CON1					0x00000078
	#define CFG_REN_R_GCE_INCOMPLETE_EVENT_EN	BIT(0)
	#define CFG_REN_W_GCE_INCOMPLETE_EVENT_EN	BIT(8)
	#define CFG_GCE_REN_W_OVER_TH_EVENT_EN		BIT(24)
#define RBFC_FRAME_MASK_PAT				0x0000007c
#define RBFC_FRAME_MASK_CTL				0x00000080
#define RBFC_FRAME_MASK					0x00000084
#define RBFC_R_UNDER_TH					0x00000088
#define RBFC_DBG_OUT_SEL				0x0000008c
	#define CFG_DBG_OUT_SEL				GENMASK(3, 0)
	#define CFG_DBG_INT_FIFO_STA_SEL		GENMASK(11, 8)
	#define CFG_ATPG_MUX				BIT(31)
#define RBFC_MUTEX_CON					0x00000090
	#define CFG_WEN_MUTEX_ERR_W_INCOMPLETE_EN	BIT(0)
	#define CFG_WEN_MUTEX_ERR_RBFC_FULL_EN		BIT(1)
	#define CFG_WEN_MUTEX_SOF_RBFC_W_OV_TH_EN	BIT(2)
	#define CFG_REN_MUTEX_ERR_R_INCOMPLETE_EN	BIT(16)
	#define CFG_REN_MUTEX_SOF_RBFC_W_OV_TH_EN	BIT(18)
#define RBFC_CON7					0x00000094
	#define CFG_MASK_INCOMPLETE_FRAME_CHK		BIT(0)
	#define CFG_MASK_INCOMPLETE_FRAME_QUICK_CHK	BIT(8)
#define RBFC_DCM_CON					0x000000a0
	#define CFG_TOP_R_CK_DCM_ON			BIT(0)
	#define CFG_TOP_W_CK_DCM_ON			BIT(1)
	#define CFG_REG_DCM_ON				BIT(2)
	#define CFG_SMI_DCM_ON				BIT(3)
	#define CFG_SLICER_DCM_ON			BIT(4)
	#define CFG_R_CK_ACTIVE_CNT			GENMASK(11, 8)
	#define CFG_W_CK_ACTIVE_CNT			GENMASK(19, 16)
#define RBFC_CON6					0x000000a4
	#define CFG_ISP_ERR_HANDLING_EN			BIT(0)
#define RBFC_TOCK_DIV					0x000000a8
#define RBFC_WFULL_TMOUT_VAL				0x000000ac
	#define CFG_WFULL_TIMEOUT_VAL			GENMASK(31, 0)
#define RBFC_W_OP_CNT					0x000000b0
	#define VALID_ROW_CNT				GENMASK(14, 0)
	#define VALID_FRAME_CNT				GENMASK(19, 16)
#define RBFC_R_OP_CNT					0x000000b4
	#define CURRENT_ROW_CNT				GENMASK(14, 0)
	#define CURRENT_FRAME_CNT			GENMASK(19, 16)
#define RBFC_BUF_CNT					0x000000b8
	#define FILL_CNT				GENMASK(14, 0)
	#define CURRENT_FRAME_CNT			GENMASK(19, 16)
#define RBFC_W_OP_ALL_CNT				0x000000bc
	#define VALID_ROW_EVEN_CNT			GENMASK(14, 0)
	#define VALID_ROW_ODD_CNT			GENMASK(30, 16)
	#define W_VALID_FRAME_CNT			BIT(31)
#define RBFC_DP_CNT					0x000000c0
	#define DP_ROW_CNT				GENMASK(14, 0)
	#define FRAME_CNT				GENMASK(19, 16)
#define RBFC_INT_FIFO_STA				0x000000ec
#define RBFC_MON0					0x000000f0
	#define CFG_ALL_FIFO_EMPTY			BIT(3)
	#define BYPASS_MODE_FINAL_SOF			BIT(15)
	#define CFG_TIMEOUT_FLAG_1			BIT(30)
	#define CFG_TIMEOUT_FLAG_2			BIT(31)
#define RBFC_MON1					0x000000f4
	#define ALL_FIFO_OVERFLOW			BIT(16)
	#define ALL_FIFO_OVERFLOW_TIMEOUT		BIT(17)
	#define ALL_FIFO_UNDERFLOW			BIT(23)
	#define ALL_FIFO_UNDERFLOW_TIMEOUT		BIT(24)


#endif /*__MTK_RBFC_REG_H__*/
