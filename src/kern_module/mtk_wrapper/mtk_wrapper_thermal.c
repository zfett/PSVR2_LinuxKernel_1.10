/*
 *  Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *                     All Rights Reserved.
 *
 **/

/*
 * Copyright (c) 2015 MediaTek Inc.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/types.h>
#include <soc/mediatek/mtk_thermal.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <mtk_wrapper_thermal.h>

#define TEMP_MONCTL1_PERIOD_UNIT(x)	((x) & 0x3ff)

#define TEMP_MONCTL2_FILTER_INTERVAL(x)	(((x) & 0x3ff) << 16)
#define TEMP_MONCTL2_SENSOR_INTERVAL(x)	((x) & 0x3ff)

#define TEMP_AHBPOLL_ADC_POLL_INTERVAL(x)	(x)

#define TEMP_ADCWRITECTRL_ADC_PNP_WRITE		BIT(0)
#define TEMP_ADCWRITECTRL_ADC_MUX_WRITE		BIT(1)

#define TEMP_ADCVALIDMASK_VALID_HIGH		BIT(5)
#define TEMP_ADCVALIDMASK_VALID_POS(bit)	(bit)

/*
 * Layout of the fuses providing the calibration data
 * MT3612 has eight sensors and need five VTS calibration data.
 */
#define CALIB_BUF0_ADC_OE(x)	(((x) >> 12) & 0x3ff)
#define CALIB_BUF0_ADC_GE(x)	(((x) >> 22) & 0x3ff)
#define CALIB_BUF1_VALID		BIT(0)
#define CALIB_BUF1_O_SLOPE(x)	(((x) >> 2) & 0x3f)
#define CALIB_BUF1_DEGC_CALI(x)	(((x) >> 8) & 0x3f)
#define CALIB_BUF1_VTS_TS0(x)	(((x) >> 23) & 0x1ff)
#define CALIB_BUF2_VTS_TS1(x)	(((x) >> 0) & 0x1ff)
#define CALIB_BUF2_VTS_TS2(x)	(((x) >> 9) & 0x1ff)
#define CALIB_BUF2_VTS_TS3(x)	(((x) >> 18) & 0x1ff)
#define CALIB_BUF3_VTS_TS4(x)	(((x) >> 0) & 0x1ff)
#define CALIB_BUF3_VTS_TS5(x)	(((x) >> 9) & 0x1ff)
#define CALIB_BUF3_VTS_TS6(x)	(((x) >> 18) & 0x1ff)
#define CALIB_BUF4_VTS_TS7(x)	(((x) >> 0) & 0x1ff)
#define CALIB_BUF4_VTS_TS8(x)	(((x) >> 9) & 0x1ff)

/* thermal sensors */
#define TS0	0
#define TS1	1
#define TS2	2
#define TS3	3
#define TS4	4
#define TS5	5
#define TS6	6
#define TS7	7
#define TS8	8
#define TS9	9

/* AUXADC channel 11 is used for the temperature sensors */
#define MT3612_TEMP_AUXADC_CHANNEL	11

/* The total number of temperature sensors in the MT3612 */
#define MT3612_NUM_SENSORS	8
#define MT3612_NUM_ZONES	1

#define THERMAL_NAME    "mtk-thermal"

/* The number of sensing points per bank */
#define MT3612_NUM_SENSORS_PER_ZONE	8

struct mtk_thermal;

struct thermal_bank_cfg {
	unsigned int num_sensors;
	const int *sensors;
};

struct mtk_thermal_bank {
	struct mtk_thermal *mt;
	int id;
};

struct mtk_thermal_data {
	s32 num_banks;
	s32 num_sensors;
	s32 auxadc_channel;
	const u8 *sensor_mux_values;
	const int *msr;
	const int *adcpnp;
	s32 deg_const;
	struct thermal_bank_cfg bank_data[];
};

struct mtk_thermal {
	struct device *dev;
	void __iomem *thermal_base;

	struct clk *clk_peri_therm;
	struct clk *clk_auxadc;
	/* lock: for getting and putting banks */
	struct mutex lock;

	/* Calibration values */
	s32 adc_ge;
	s32 adc_oe;
	s32 degc_cali;
	s32 o_slope;
	s32 vts[MT3612_NUM_SENSORS];

	const struct mtk_thermal_data *conf;
	struct mtk_thermal_bank banks[];
};

/* MT3612 thermal sensor data */
static const int mt3612_bank_data[MT3612_NUM_ZONES][8] = {
	{ TS1, TS2, TS3, TS4, TS5, TS7, TS8, TS9 },
};

static const int mt3612_msr[MT3612_NUM_SENSORS_PER_ZONE] = {
	TEMP_MSR0, TEMP_MSR1, TEMP_MSR2, TEMP_MSR3,
	TEMP_MSR0_1, TEMP_MSR1_1, TEMP_MSR2_1, TEMP_MSR3_1
};

static const int mt3612_adcpnp[MT3612_NUM_SENSORS_PER_ZONE] = {
	TEMP_ADCPNP0, TEMP_ADCPNP1, TEMP_ADCPNP2, TEMP_ADCPNP3,
	TEMP_ADCPNP0_1, TEMP_ADCPNP1_1, TEMP_ADCPNP2_1, TEMP_ADCPNP3_1
};

static const u8 mt3612_mux_values[MT3612_NUM_SENSORS] = {
	0, 1, 2, 3, 4, 6, 7, 8 };

#define ADC_FULL 4096
#define ADC_ROUND 512
#define SCALING 10000
#define VAL_OFFSET 2900		/* 3350 */
#define THRM_CONST 2288818	/* (=1.5/1.6/4096*10^6)*10000), 2034505 */
#define MT3612_DEG_CONST 1518	/* 1579, 1650 */
#define DEG_CALI_CONST 500

static const struct mtk_thermal_data mt3612_thermal_data = {
	.auxadc_channel = MT3612_TEMP_AUXADC_CHANNEL,
	.num_banks = MT3612_NUM_ZONES,
	.num_sensors = MT3612_NUM_SENSORS,
	.bank_data = {
		{
			.num_sensors = 8,
			.sensors = mt3612_bank_data[0],
		},
	},
	.msr = mt3612_msr,
	.adcpnp = mt3612_adcpnp,
	.sensor_mux_values = mt3612_mux_values,
	.deg_const = MT3612_DEG_CONST,
};

/**
 * raw_to_mcelsius - convert a raw ADC value to mcelsius
 * @mt:		The thermal controller
 * @raw:	raw ADC value
 *
 * This converts the raw ADC value to mcelsius using the SoC specific
 * calibration constants
 */
static int raw_to_mcelsius(struct mtk_thermal *mt, int sensno, s32 raw)
{
	const struct mtk_thermal_data *conf = mt->conf;
	s32 x_roomt = 0, i;
	s32 format_1 = 0;
	s32 format_2 = 0;
	long format_4 = 0;
	s32 gain_err = 0;
	s32 gain = 0;
	s32 curr_temp = 0;

	raw &= 0xfff;
	gain_err = ((mt->adc_ge - ADC_ROUND) * SCALING) / ADC_FULL;
	gain = (SCALING + gain_err);
	if (sensno == 0) {
		x_roomt = (mt->vts[0] + VAL_OFFSET);
	} else {
		for (i = 0; i < mt->conf->num_sensors; i++) {
			if (sensno == mt->conf->bank_data[0].sensors[i]) {
				x_roomt = (mt->vts[i] + VAL_OFFSET);
				break;
			}
		}
		if (i == mt->conf->num_sensors)
			pr_err("TS%d not found\n", sensno);
	}
	format_1 = mt->degc_cali * DEG_CALI_CONST;
	format_2 = raw - x_roomt;
	/* format_3 = 2034505 * format_2 / gain; */
	/* if (g_o_slope_sign == 0) */
	/*	format_4 = ((format_3 * 1000) / (1528 + g_o_slope * 10)); */
	/* else */
	/*	format_4 = ((format_3 * 1000) / (1528 - g_o_slope * 10)); */
	format_4 = ((THRM_CONST * format_2) / conf->deg_const) * 1000 / gain;
	curr_temp = format_1 - format_4;
	/*pr_err("TS%d: vts=0x%x, gain=0x%x\n", sensno, mt->vts[sensno],*/
	/*     gain);*/
	/*pr_err("TS%d: fmt1=0x%x, fmt2:0x%x, fmt4:0x%lx\n",*/
	/*     sensno, format_1, format_2, format_4);*/
	pr_debug("TS%d: raw=0x%x, temp=%d (mdegC)\n", sensno, raw, curr_temp);
	return curr_temp;

}

/**
 * mtk_thermal_get_bank - get bank
 * @bank:	The bank
 *
 * The bank registers are banked, we have to select a bank in the
 * PTPCORESEL register to access it.
 */
static void mtk_thermal_get_bank(struct mtk_thermal_bank *bank)
{
	struct mtk_thermal *mt = bank->mt;
	/* u32 val; */

	mutex_lock(&mt->lock);
	/* val = readl(mt->thermal_base + PTPCORESEL); */
	/* val &= ~0xf; */
	/* val |= bank->id; */
	/* writel(val, mt->thermal_base + PTPCORESEL); */
}

/**
 * mtk_thermal_put_bank - release bank
 * @bank:	The bank
 *
 * release a bank previously taken with mtk_thermal_get_bank,
 */
static void mtk_thermal_put_bank(struct mtk_thermal_bank *bank)
{
	struct mtk_thermal *mt = bank->mt;

	mutex_unlock(&mt->lock);
}

/**
 * mtk_thermal_bank_temperature - get the temperature of a bank
 * @bank:	The bank
 *
 * The temperature of a bank is considered the maximum temperature of
 * the sensors associated to the bank.
 */
static int mtk_thermal_bank_temperature(struct mtk_thermal_bank *bank)
{
	struct mtk_thermal *mt = bank->mt;
	const struct mtk_thermal_data *conf = mt->conf;
	int i, temp = INT_MIN, max = INT_MIN;
	u32 raw;

	for (i = 0; i < conf->bank_data[bank->id].num_sensors; i++) {
		raw = readl(mt->thermal_base + conf->msr[i]);

		temp = raw_to_mcelsius(mt,
				       conf->bank_data[bank->id].sensors[i],
				       raw);

		/*
		 * The first read of a sensor often contains very high bogus
		 * temperature value. Filter these out so that the system does
		 * not immediately shut down.
		 */
		if (temp > 200000)
			temp = 0;

		if (temp > max)
			max = temp;
	}

	return max;
}

static int mtk_read_temp(void *data, int *temperature)
{
	struct mtk_thermal *mt = data;
	int i;
	int tempmax = INT_MIN;

	for (i = 0; i < mt->conf->num_banks; i++) {
		struct mtk_thermal_bank *bank = &mt->banks[i];

		mtk_thermal_get_bank(bank);
		tempmax = max(tempmax, mtk_thermal_bank_temperature(bank));
		mtk_thermal_put_bank(bank);
	}

	*temperature = tempmax;

	return 0;
}

static const struct thermal_zone_of_device_ops mtk_thermal_ops = {
	.get_temp = mtk_read_temp,
};

static void mtk_thermal_init_bank(struct mtk_thermal *mt, int num,
				  u32 apmixed_phys_base, u32 auxadc_phys_base)
{
	struct mtk_thermal_bank *bank = &mt->banks[num];
	const struct mtk_thermal_data *conf = mt->conf;
	int i;

	bank->id = num;
	bank->mt = mt;

	mtk_thermal_get_bank(bank);

	/* bus clock 66M counting unit is 12 * 15.15ns * 256 = 46.540us */
	writel(TEMP_MONCTL1_PERIOD_UNIT(12), mt->thermal_base + TEMP_MONCTL1);
	writel(TEMP_MONCTL1_PERIOD_UNIT(12), mt->thermal_base + TEMP_MONCTL1_1);

	/*
	 * filt interval is 1 * 46.540us = 46.54us,
	 * sen interval is 429 * 46.540us = 19.96ms
	 */
	writel(TEMP_MONCTL2_FILTER_INTERVAL(1) |
			TEMP_MONCTL2_SENSOR_INTERVAL(429),
			mt->thermal_base + TEMP_MONCTL2);
	writel(TEMP_MONCTL2_FILTER_INTERVAL(1) |
			TEMP_MONCTL2_SENSOR_INTERVAL(429),
			mt->thermal_base + TEMP_MONCTL2_1);

	/* poll is set to 10u */
	writel(TEMP_AHBPOLL_ADC_POLL_INTERVAL(256),
	       mt->thermal_base + TEMP_AHBPOLL);
	writel(TEMP_AHBPOLL_ADC_POLL_INTERVAL(256),
	       mt->thermal_base + TEMP_AHBPOLL_1);

	/* temperature sampling control, 6 sample drop max/min and average */
	writel(0x6db, mt->thermal_base + TEMP_MSRCTL0);		/* 0x0 */
	writel(0x6db, mt->thermal_base + TEMP_MSRCTL0_1);	/* 0x0 */

	/* exceed this polling time, IRQ would be inserted */
	writel(0xffffffff, mt->thermal_base + TEMP_AHBTO);	/* 0xff */
	writel(0xffffffff, mt->thermal_base + TEMP_AHBTO_1);	/* 0xff */

	/* number of interrupts per event, 1 is enough */
	writel(0x0, mt->thermal_base + TEMP_MONIDET0);
	writel(0x0, mt->thermal_base + TEMP_MONIDET1);

	/*
	 * The MT3612 thermal controller does not have its own ADC. Instead it
	 * uses AHB bus accesses to control the AUXADC. To do this the thermal
	 * controller has to be programmed with the physical addresses of the
	 * AUXADC registers and with the various bit positions in the AUXADC.
	 * Also the thermal controller controls a mux in the APMIXEDSYS register
	 * space.
	 */

	/*
	 * this value will be stored to TEMP_PNPMUXADDR
	 * automatically by hw
	 */
	writel(BIT(conf->auxadc_channel), mt->thermal_base + TEMP_ADCMUX);
	writel(BIT(conf->auxadc_channel), mt->thermal_base + TEMP_ADCMUX_1);

	/* AHB address for auxadc mux selection */
	writel(auxadc_phys_base + AUXADC_CON1_CLR_V,
	       mt->thermal_base + TEMP_ADCMUXADDR);
	writel(auxadc_phys_base + AUXADC_CON1_CLR_V,
	       mt->thermal_base + TEMP_ADCMUXADDR_1);

	/* AHB address for pnp sensor mux selection */
	writel(apmixed_phys_base + APMIXED_SYS_TS_CON1,
	       mt->thermal_base + TEMP_PNPMUXADDR);
	writel(apmixed_phys_base + APMIXED_SYS_TS_CON1,
	       mt->thermal_base + TEMP_PNPMUXADDR_1);

	/* AHB value for auxadc enable */
	writel(BIT(conf->auxadc_channel), mt->thermal_base + TEMP_ADCEN);
	writel(BIT(conf->auxadc_channel), mt->thermal_base + TEMP_ADCEN_1);

	/* AHB address for auxadc enable (channel 0 immediate mode selected) */
	writel(auxadc_phys_base + AUXADC_CON1_SET_V,
	       mt->thermal_base + TEMP_ADCENADDR);
	writel(auxadc_phys_base + AUXADC_CON1_SET_V,
	       mt->thermal_base + TEMP_ADCENADDR_1);

	/* AHB address for auxadc valid bit */
	writel(auxadc_phys_base + AUXADC_DATA(conf->auxadc_channel),
	       mt->thermal_base + TEMP_ADCVALIDADDR);
	writel(auxadc_phys_base + AUXADC_DATA(conf->auxadc_channel),
	       mt->thermal_base + TEMP_ADCVALIDADDR_1);

	/* AHB address for auxadc voltage output */
	writel(auxadc_phys_base + AUXADC_DATA(conf->auxadc_channel),
	       mt->thermal_base + TEMP_ADCVOLTADDR);
	writel(auxadc_phys_base + AUXADC_DATA(conf->auxadc_channel),
	       mt->thermal_base + TEMP_ADCVOLTADDR_1);

	/* read valid & voltage are at the same register */
	writel(0x0, mt->thermal_base + TEMP_RDCTRL);
	writel(0x0, mt->thermal_base + TEMP_RDCTRL_1);

	/* indicate where the valid bit is */
	writel(TEMP_ADCVALIDMASK_VALID_HIGH | TEMP_ADCVALIDMASK_VALID_POS(12),
	       mt->thermal_base + TEMP_ADCVALIDMASK);
	writel(TEMP_ADCVALIDMASK_VALID_HIGH | TEMP_ADCVALIDMASK_VALID_POS(12),
	       mt->thermal_base + TEMP_ADCVALIDMASK_1);

	/* no shift */
	writel(0x0, mt->thermal_base + TEMP_ADCVOLTAGESHIFT);
	writel(0x0, mt->thermal_base + TEMP_ADCVOLTAGESHIFT_1);

	/* enable auxadc mux write transaction */
	writel(TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       mt->thermal_base + TEMP_ADCWRITECTRL);
	writel(TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       mt->thermal_base + TEMP_ADCWRITECTRL_1);

	for (i = 0; i < conf->bank_data[num].num_sensors; i++)
		writel(conf->sensor_mux_values[i],
		       mt->thermal_base + conf->adcpnp[i]);

	writel((1 << conf->bank_data[num].num_sensors) - 1,
	       mt->thermal_base + TEMP_MONCTL0);
	writel((1 << conf->bank_data[num].num_sensors) - 1,
	       mt->thermal_base + TEMP_MONCTL0_1);

	writel(TEMP_ADCWRITECTRL_ADC_PNP_WRITE |
	       TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       mt->thermal_base + TEMP_ADCWRITECTRL);
	writel(TEMP_ADCWRITECTRL_ADC_PNP_WRITE |
	       TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       mt->thermal_base + TEMP_ADCWRITECTRL_1);

	mtk_thermal_put_bank(bank);
}

static u64 of_get_phys_base(struct device_node *np)
{
	u64 size64;
	const __be32 *regaddr_p;

	regaddr_p = of_get_address(np, 0, &size64, NULL);
	if (!regaddr_p)
		return OF_BAD_ADDR;

	return of_translate_address(np, regaddr_p);
}

static int mtk_thermal_get_calibration_data(struct device *dev,
					    struct mtk_thermal *mt)
{
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len;
	int i, ret = 0;

	/* Start with default values */
	mt->adc_ge = 0x230;     /* 512 */
	mt->adc_oe = 0x1f7;     /* 512 */
	for (i = 0; i < mt->conf->num_sensors; i++)
		mt->vts[i] = 0x158;     /* 260 */
	mt->degc_cali = 0x3b;           /* 40 */
	mt->o_slope = 0;

	cell = nvmem_cell_get(dev, "calibration-data");
	if (IS_ERR(cell)) {
		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return PTR_ERR(cell);
		return 0;
	}

	buf = (u32 *)nvmem_cell_read(cell, &len);

	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	if (len < 3 * sizeof(u32)) {
		dev_warn(dev, "invalid calibration data\n");
		ret = -EINVAL;
		goto out;
	}

	if (buf[1] & CALIB_BUF1_VALID) {
		mt->adc_ge = CALIB_BUF0_ADC_GE(buf[0]);
		mt->adc_oe = CALIB_BUF0_ADC_OE(buf[0]);
		for (i = 0; i < mt->conf->num_sensors; i++) {
			switch (mt->conf->bank_data[0].sensors[i]) {
			case TS0:
			case TS1:
				mt->vts[i] = CALIB_BUF1_VTS_TS0(buf[1]);
				break;
			case TS2:
				mt->vts[i] = CALIB_BUF2_VTS_TS1(buf[2]);
				break;
			case TS3:
				mt->vts[i] = CALIB_BUF2_VTS_TS2(buf[2]);
				break;
			case TS4:
				mt->vts[i] = CALIB_BUF2_VTS_TS3(buf[2]);
				break;
			case TS5:
				mt->vts[i] = CALIB_BUF3_VTS_TS4(buf[3]);
				break;
			case TS6:
				mt->vts[i] = CALIB_BUF3_VTS_TS5(buf[3]);
				break;
			case TS7:
				mt->vts[i] = CALIB_BUF3_VTS_TS6(buf[3]);
				break;
			case TS8:
				mt->vts[i] = CALIB_BUF4_VTS_TS7(buf[4]);
				break;
			case TS9:
				mt->vts[i] = CALIB_BUF4_VTS_TS8(buf[4]);
				break;
			}
		}
		mt->degc_cali = CALIB_BUF1_DEGC_CALI(buf[1]);
		mt->o_slope = CALIB_BUF1_O_SLOPE(buf[1]);
	} else {
		dev_info(dev, "Device not calibrated, using default values\n");
	}
	dev_info(dev, "EFUSE TSAUX data = %x %x %x %x %x\n",
		 buf[0], buf[1], buf[2], buf[3], buf[4]);
	dev_info(dev, "GE=0x%x, OE=0x%x, DEGC_CALI=0x%x, O_SLOPE=0x%x\n",
		 mt->adc_ge, mt->adc_oe, mt->degc_cali, mt->o_slope);
	dev_info(dev, "O_VTS0=0x%x, O_VTS1=0x%x, O_VTS2=0x%x, O_VTS3=0x%x\n",
		 mt->vts[TS0], mt->vts[TS1], mt->vts[TS2], mt->vts[TS3]);
	dev_info(dev, "O_VTS4=0x%x, O_VTS5=0x%x, O_VTS6=0x%x, O_VTS7=0x%x\n",
		 mt->vts[TS4], mt->vts[TS5], mt->vts[TS6], mt->vts[TS7]);

out:
	kfree(buf);

	return ret;
}

static const struct of_device_id mtk_thermal_of_match[] = {
	{
		.compatible = "mediatek,mt3612-thermal",
		.data = (void *)&mt3612_thermal_data,
	}, {
	},
};
MODULE_DEVICE_TABLE(of, mtk_thermal_of_match);

/* ///////////////////////////////////////////////////// */
/* for sysfs */
#define MTK_WRAPPER_THERMAL_CLASS "thermal"
#define MTK_WRAPPER_THERMAL_DEVICE "mtk_wrapper_thermal"

/* get temperature from sensor i */
static int get_temperature_from_sensor(struct mtk_thermal *mt, const int bank_id, const int sensor_id)
{
	const struct mtk_thermal_data *conf = mt->conf;
	int temp;
	u32 raw = readl(mt->thermal_base + conf->msr[sensor_id]);

	temp = raw_to_mcelsius(mt, conf->bank_data[bank_id].sensors[sensor_id], raw);
	return temp;
}

static ssize_t thermal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int sensor_id = INT_MIN;
	int temperature = INT_MIN;
	struct mtk_thermal *mt = dev_get_drvdata(dev);
	int bank_id = mt->banks->id;

	/* get sensor index from attributes name. */
	if (kstrtoint(attr->attr.name + strlen(attr->attr.name) - 1, 10, &sensor_id) != 0) {
		pr_err("failed to get sensor_idx from attributes");
		return sensor_id;
	}
	pr_debug("bank_id:%d, sensor_idx:%d", bank_id, sensor_id);

	/* get temp */
	temperature = get_temperature_from_sensor(mt, bank_id, sensor_id);
	return scnprintf(buf, PAGE_SIZE, "%d", temperature);
}

/* attributes for showing temperatures */
static DEVICE_ATTR(sensor0, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor1, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor2, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor3, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor4, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor5, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor6, S_IRUGO, thermal_show, NULL);
static DEVICE_ATTR(sensor7, S_IRUGO, thermal_show, NULL);

static struct attribute *wrapper_thermal_attrs[] = {
	&dev_attr_sensor0.attr,
	&dev_attr_sensor1.attr,
	&dev_attr_sensor2.attr,
	&dev_attr_sensor3.attr,
	&dev_attr_sensor4.attr,
	&dev_attr_sensor5.attr,
	&dev_attr_sensor6.attr,
	&dev_attr_sensor7.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wrapper_thermal);

static dev_t           wrapper_thermal_devid;
static struct cdev	   wrapper_thermal_cdev;
static struct device  *wrapper_thermal_dev;
static struct class   *wrapper_thermal_cls;

/* for ioctl */
static int open_thermal(struct inode *inode, struct file *file)
{
	pr_debug("open thermal wrapper device");
	/* pr_info("major:%d, minor:%d", MAJOR(inode->i_rdev), MINOR(inode->i_rdev)); */
	file->private_data = (void *)dev_get_drvdata(wrapper_thermal_dev);

	return 0;
}

static int close_thermal(struct inode *inode, struct file *file)
{
	pr_debug("close thermal wrapper device");
	file->private_data = NULL;

	return 0;
}

static int thermal_get_from_id(struct mtk_thermal *mt, unsigned long arg)
{
	args_thermal_get_from_id args;

	/* copy from user */
	if (copy_from_user(&args, (void __user *)arg, sizeof(args)) != 0) {
		return -EFAULT;
	}

	/* get temp */
	if (args.in_bank_id < 0 || args.in_bank_id >= THERMAL_BANK_MAX) {
		pr_err("Invalid in_bank_id = %d", args.in_bank_id);
		return -EINVAL;
	}
	if (args.in_sensor_id < 0 || args.in_sensor_id >= THERMAL_SENSOR_MAX) {
		pr_err("Invalid in_sensor_id = %d", args.in_sensor_id);
		return -EINVAL;
	}
	args.out_temperature = get_temperature_from_sensor(mt, args.in_bank_id, args.in_sensor_id);

	/* copy to user */
	if (copy_to_user((void __user *)arg, &args, sizeof(args)) != 0) {
		return -EFAULT;
	}

	/* print info */
	pr_debug("bank_id:%d, sensor_id:%d, temperature:%d",
		 args.in_bank_id, args.in_sensor_id, args.out_temperature);

	return 0;
}

static int thermal_get_max(struct mtk_thermal *mt, unsigned long arg)
{
	args_thermal_get args;
	int i, j;
	int max_temp = INT_MIN;
	int max_temp_bank_id = -EFAULT;
	int max_temp_sensor_id = -EFAULT;

	/* copy from user */
	if (copy_from_user(&args, (void __user *)arg, sizeof(args)) != 0) {
		return -EFAULT;
	}

	/* get max temp */
	for (i = 0; i < THERMAL_BANK_MAX; i++) {
		for (j = 0; j < THERMAL_SENSOR_MAX; j++) {
			int current_max_temp = get_temperature_from_sensor(mt, i, j);

			if (current_max_temp > max_temp) {
				max_temp = current_max_temp;
				max_temp_bank_id = i;
				max_temp_sensor_id = j;
			}
		}
	}
	args.out_bank_id = max_temp_bank_id;
	args.out_sensor_id = max_temp_sensor_id;
	args.out_temperature = max_temp;

	/* copy to user */
	if (copy_to_user((void __user *)arg, &args, sizeof(args)) != 0) {
		return -EFAULT;
	}

	/* print info */
	pr_debug("bank_id:%d, sensor_id:%d, max temperature:%d",
		 args.out_bank_id, args.out_sensor_id, args.out_temperature);

	return 0;
}

static int thermal_get_min(struct mtk_thermal *mt, unsigned long arg)
{
	args_thermal_get args;
	int i, j;
	int min_temp = INT_MAX;
	int min_temp_bank_id = -EFAULT;
	int min_temp_sensor_id = -EFAULT;

	/* copy from user */
	if (copy_from_user(&args, (void __user *)arg, sizeof(args)) != 0) {
		return -EFAULT;
	}

	/* get min temp */
	for (i = 0; i < THERMAL_BANK_MAX; i++) {
		for (j = 0; j < THERMAL_SENSOR_MAX; j++) {
			int current_min_temp = get_temperature_from_sensor(mt, i, j);

			if (current_min_temp < min_temp) {
				min_temp = current_min_temp;
				min_temp_bank_id = i;
				min_temp_sensor_id = j;
			}
		}
	}
	args.out_bank_id = min_temp_bank_id;
	args.out_sensor_id = min_temp_sensor_id;
	args.out_temperature = min_temp;

	/* copy to user */
	if (copy_to_user((void __user *)arg, &args, sizeof(args)) != 0) {
		return -EFAULT;
	}

	/* print info */
	pr_debug("bank_id:%d, sensor_id:%d, min temperature:%d",
		 args.out_bank_id, args.out_sensor_id, args.out_temperature);

	return 0;
}

static long get_thermal_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;

	switch (cmd) {
	case THERMAL_GET_FROM_ID:
		pr_debug("[THERMAL IOCTL] thermal get from id");
		ret = thermal_get_from_id((struct mtk_thermal *)file->private_data, arg);
		break;

	case THERMAL_GET_MAX:
		pr_debug("[THERMAL IOCTL] thermal get max");
		ret = thermal_get_max((struct mtk_thermal *)file->private_data, arg);
		break;

	case THERMAL_GET_MIN:
		pr_debug("[THERMAL IOCTL] thermal get min");
		ret = thermal_get_min((struct mtk_thermal *)file->private_data, arg);
		break;
	default:
		pr_err("unknown command");
		break;
	}
	return ret;
}

static const struct file_operations wrapper_thermal_fops = {
	.open = open_thermal,
	.release = close_thermal,
	.unlocked_ioctl = get_thermal_ioctl,
	.compat_ioctl = get_thermal_ioctl,
};

/* /////////////////////////////////////////////////////////// */

static int mtk_thermal_probe(struct platform_device *pdev)
{
	int ret, i;
	struct device_node *auxadc, *apmixedsys, *np = pdev->dev.of_node;
	struct mtk_thermal *mt;
	struct resource *res;
	const struct of_device_id *of_id;
	u64 auxadc_phys_base, apmixed_phys_base;
	int ret_cdev   = 0;
	int ret_chrdev = 0;

	mt = devm_kzalloc(&pdev->dev, sizeof(*mt), GFP_KERNEL);
	if (!mt)
		return -ENOMEM;

	of_id = of_match_device(mtk_thermal_of_match, &pdev->dev);
	if (of_id)
		mt->conf = (const struct mtk_thermal_data *)of_id->data;

	mt->clk_auxadc = devm_clk_get(&pdev->dev, "auxadc");
	if (IS_ERR(mt->clk_auxadc))
		return PTR_ERR(mt->clk_auxadc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mt->thermal_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mt->thermal_base))
		return PTR_ERR(mt->thermal_base);

	ret = mtk_thermal_get_calibration_data(&pdev->dev, mt);
	if (ret)
		return ret;

	mutex_init(&mt->lock);

	mt->dev = &pdev->dev;

	auxadc = of_parse_phandle(np, "mediatek,auxadc", 0);
	if (!auxadc) {
		dev_err(&pdev->dev, "missing auxadc node\n");
		return -ENODEV;
	}

	auxadc_phys_base = of_get_phys_base(auxadc);

	of_node_put(auxadc);

	if (auxadc_phys_base == OF_BAD_ADDR) {
		dev_err(&pdev->dev, "Can't get auxadc phys address\n");
		return -EINVAL;
	}

	apmixedsys = of_parse_phandle(np, "mediatek,apmixedsys", 0);
	if (!apmixedsys) {
		dev_err(&pdev->dev, "missing apmixedsys node\n");
		return -ENODEV;
	}

	apmixed_phys_base = of_get_phys_base(apmixedsys);

	of_node_put(apmixedsys);

	if (apmixed_phys_base == OF_BAD_ADDR) {
		dev_err(&pdev->dev, "Can't get auxadc phys address\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(mt->clk_auxadc);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable auxadc clk: %d\n", ret);
		return ret;
	}

	ret = device_reset(&pdev->dev);
	if (ret)
		goto err_disable_clk_auxadc;

	for (i = 0; i < mt->conf->num_banks; i++)
		mtk_thermal_init_bank(mt, i, apmixed_phys_base,
				      auxadc_phys_base);

	msleep(100);
	platform_set_drvdata(pdev, mt);

	/* get driver number */
	ret_chrdev = alloc_chrdev_region(&wrapper_thermal_devid, 0, 1, MTK_WRAPPER_THERMAL_DEVICE);
	if (ret_chrdev < 0) {
		pr_err("failed to allocate chardev region.");
		goto error;
	}

	/* init cdev */
	cdev_init(&wrapper_thermal_cdev, &wrapper_thermal_fops);
	ret_cdev = cdev_add(&wrapper_thermal_cdev, wrapper_thermal_devid, 1);
	if (ret_cdev < 0) {
		pr_err("failed to init cdev.");
		goto error;
	}

	/* create class */
	wrapper_thermal_cls = class_create(THIS_MODULE, MTK_WRAPPER_THERMAL_CLASS);
	if (IS_ERR(wrapper_thermal_cls)) {
		pr_err("failed to create class.");
		goto error;
	}

	/* create device */
	wrapper_thermal_dev = device_create_with_groups(wrapper_thermal_cls, &pdev->dev, wrapper_thermal_devid, mt, wrapper_thermal_groups, MTK_WRAPPER_THERMAL_DEVICE);
	if (IS_ERR(wrapper_thermal_dev)) {
		pr_err("failed to create class.");
		goto error;
	}
	return 0;

error:
	if (ret == 0)
		clk_disable_unprepare(mt->clk_auxadc);
	if (!IS_ERR(wrapper_thermal_dev))
		device_unregister(wrapper_thermal_dev);
	if (!IS_ERR(wrapper_thermal_cls))
		class_destroy(wrapper_thermal_cls);
	if (ret_cdev == 0)
		cdev_del(&wrapper_thermal_cdev);
	if (ret_chrdev == 0)
		unregister_chrdev_region(wrapper_thermal_devid, 1);
err_disable_clk_auxadc:
	clk_disable_unprepare(mt->clk_auxadc);

	return ret;
}

static int mtk_thermal_remove(struct platform_device *pdev)
{
	struct mtk_thermal *mt = platform_get_drvdata(pdev);

	clk_disable_unprepare(mt->clk_auxadc);

	/* for sysfs */
	device_unregister(wrapper_thermal_dev);
	class_destroy(wrapper_thermal_cls);
	cdev_del(&wrapper_thermal_cdev);
	unregister_chrdev_region(wrapper_thermal_devid, 1);

	return 0;
}

static struct platform_driver mtk_thermal_driver = {
	.probe = mtk_thermal_probe,
	.remove = mtk_thermal_remove,
	.driver = {
		.name = THERMAL_NAME,
		.of_match_table = mtk_thermal_of_match,
	},
};

/* module_platform_driver(mtk_thermal_driver); */
int __init init_module_mtk_wrapper_thermal(void)
{
	pr_info("%s start\n", __func__);
	return platform_driver_register(&mtk_thermal_driver);
}

void __exit cleanup_module_mtk_wrapper_thermal(void)
{
	pr_info("%s end\n", __func__);
	platform_driver_unregister(&mtk_thermal_driver);
}

MODULE_DESCRIPTION("Mediatek thermal driver");
MODULE_LICENSE("GPL v2");
