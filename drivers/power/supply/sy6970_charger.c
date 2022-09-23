/*
 * SY SY6970 charger driver
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
#include "battery_id_via_adc.h"
/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */

#define SY6970_REG_00				0x00
#define SY6970_REG_01				0x01
#define SY6970_REG_02				0x02
#define SY6970_REG_03				0x03
#define SY6970_REG_04				0x04
#define SY6970_REG_05				0x05
#define SY6970_REG_06				0x06
#define SY6970_REG_07				0x07
#define SY6970_REG_08				0x08
#define SY6970_REG_09				0x09
#define SY6970_REG_0A				0x0a
#define SY6970_REG_0B				0x0b
#define SY6970_REG_0C				0x0c
#define SY6970_REG_0D				0x0d
#define SY6970_REG_0E				0x0e
#define SY6970_REG_0F				0x0f
#define SY6970_REG_10				0x10
#define SY6970_REG_11				0x11
#define SY6970_REG_12				0x12
#define SY6970_REG_13				0x13
#define SY6970_REG_14				0x14
#define SY6970_REG_NUM				21

/* Register 0x00 */
#define REG00_ENHIZ_MASK			0x80
#define REG00_ENHIZ_SHIFT			7
#define REG00_EN_ILIM_MASK			0x40
#define REG00_EN_ILIM_SHIFT			6
#define REG00_IINLIM_MASK			0x3f
#define REG00_IINLIM_SHIFT			0

/* Register 0x01*/
#define REG01_BHOT_MASK				0xc0
#define REG01_BHOT_SHIFT			6
#define REG01_BCOLD_MASK			0x20
#define REG01_BCOLD_SHIFT			5
#define REG01_VINDPM_OS_MASK			0x1f
#define REG01_VINDPM_OS_SHIFT			0

/* Register 0x02*/
#define REG02_CONV_START_MASK			0x80
#define REG02_CONV_START_SHIFT			7
#define REG02_CONV_RATE_MASK			0x40
#define REG02_CONV_RATE_SHIFT			6
#define REG02_BOOST_FREQ_MASK			0x20
#define REG02_BOOST_FREQ_SHIFT			5
#define REG02_ICO_EN_MASK			0x10
#define REG02_ICO_EN_SHIFT			4
#define REG02_HVDCP_EN_MASK			0x08
#define REG02_HVDCP_EN_SHIFT			3
#define REG02_MAXC_EN_MASK			0x04
#define REG02_MAXC_EN_SHIFT			2
#define REG02_FORCE_DPDM_MASK			0x02
#define REG02_FORCE_DPDM_SHIFT			1
#define REG02_AUTO_DPDM_EN_MASK			0x01
#define REG02_AUTO_DPDM_EN_SHIFT		0

/* Register 0x03 */
#define REG03_BAT_LOADEN_MASK			0x80
#define REG03_BAT_LOADEN_SHIFT			7
#define REG03_WDT_RESET_MASK			0x40
#define REG03_WDT_RESET_SHIFT			6
#define REG03_OTG_CONFIG_MASK			0x20
#define REG03_OTG_CONFIG_SHIFT			5
#define REG03_CHG_CONFIG_MASK			0x10
#define REG03_CHG_CONFIG_SHIFT			4
#define REG03_SYS_MINV_MASK			0x0e
#define REG03_SYS_MINV_SHIFT			1

/* Register 0x04*/
#define REG04_EN_PUMPX_MASK			0x80
#define REG04_EN_PUMPX_SHIFT			7
#define REG04_ICHG_MASK				0x7f
#define REG04_ICHG_SHIFT			0

/* Register 0x05*/
#define REG05_IPRECHG_MASK			0xf0
#define REG05_IPRECHG_SHIFT			4
#define REG05_ITERM_MASK			0x0f
#define REG05_ITERM_SHIFT			0

/* Register 0x06*/
#define REG06_VREG_MASK				0xfc
#define REG06_VREG_SHIFT			2
#define REG06_BATLOWV_MASK			0x02
#define REG06_BATLOWV_SHIFT			1
#define REG06_VRECHG_MASK			0x01
#define REG06_VRECHG_SHIFT			0

/* Register 0x07*/
#define REG07_EN_TERM_MASK			0x80
#define REG07_EN_TERM_SHIFT			7
#define REG07_STAT_DIS_MASK			0x40
#define REG07_STAT_DIS_SHIFT			6
#define REG07_WDT_MASK				0x30
#define REG07_WDT_SHIFT				4
#define REG07_EN_TIMER_MASK			0x08
#define REG07_EN_TIMER_SHIFT			3
#define REG07_CHG_TIMER_MASK			0x06
#define REG07_CHG_TIMER_SHIFT			1
#define REG07_JEITA_ISET_MASK			0x01
#define REG07_JEITA_ISET_SHIFT			0

/* Register 0x08*/
#define REG08_IR_COMP_MASK			0xe0
#define REG08_IR_COMP_SHIFT			5
#define REG08_VCLAMP_MASK			0x1c
#define REG08_VCLAMP_SHIFT			2
#define REG08_TREG_MASK				0x03
#define REG08_TREG_SHIFT			2

/* Register 0x09*/
#define REG09_FORCE_ICO_MASK			0x80
#define REG09_FORCE_ICO_SHIFT			7
#define REG09_TMR2X_EN_MASK			0x40
#define REG09_TMR2X_EN_SHIFT			6
#define REG09_BATFET_DIS_MASK			0x20
#define REG09_BATFET_DIS_SHIFT			5
#define REG09_JEITA_VSET_MASK			0x10
#define REG09_JEITA_VSET_SHIFT			4
#define REG09_BATFET_DLY_MASK			0x08
#define REG09_BATFET_DLY_SHIFT			3
#define REG09_BATFET_RST_EN_MASK		0x04
#define REG09_BATFET_RST_EN_SHIFT		2
#define REG09_PUMPX_UP_MASK			0x02
#define REG09_PUMPX_UP_SHIFT			1
#define REG09_PUMPX_DN_MASK			0x01
#define REG09_PUMPX_DN_SHIFT			0

/* Register 0x0A*/
#define REG0A_BOOSTV_MASK			0xf0
#define REG0A_BOOSTV_SHIFT			4
#define REG0A_BOOSTV_LIM_MASK			0x07
#define REG0A_BOOSTV_LIM_SHIFT			0

/* Register 0x0B*/
#define REG0B_VBUS_STAT_MASK			0xe0
#define REG0B_VBUS_STAT_SHIFT			5
#define REG0B_CHRG_STAT_MASK			0x18
#define REG0B_CHRG_STAT_SHIFT			3
#define REG0B_PG_STAT_MASK			0x04
#define REG0B_PG_STAT_SHIFT			2
#define REG0B_VSYS_STAT_MASK			0x01
#define REG0B_VSYS_STAT_SHIFT			0

/* Register 0x0C*/
#define REG0C_FAULT_WDT_MASK			0x80
#define REG0C_FAULT_WDT_SHIFT			7
#define REG0C_FAULT_BOOST_MASK			0x40
#define REG0C_FAULT_BOOST_SHIFT			6
#define REG0C_FAULT_CHRG_MASK			0x30
#define REG0C_FAULT_CHRG_SHIFT			4
#define REG0C_FAULT_BAT_MASK			0x08
#define REG0C_FAULT_BAT_SHIFT			3
#define REG0C_FAULT_NTC_MASK			0x07
#define REG0C_FAULT_NTC_SHIFT			0

/* Register 0x0D*/
#define REG0D_FORCE_VINDPM_MASK			0x80
#define REG0D_FORCE_VINDPM_SHIFT		7
#define REG0D_VINDPM_MASK			0x7f
#define REG0D_VINDPM_SHIFT			0

/* Register 0x0E*/
#define REG0E_THERM_STAT_MASK			0x80
#define REG0E_THERM_STAT_SHIFT			7
#define REG0E_BATV_MASK				0x7f
#define REG0E_BATV_SHIFT			0

/* Register 0x0F*/
#define REG0F_SYSV_MASK				0x7f
#define REG0F_SYSV_SHIFT			0

/* Register 0x10*/
#define REG10_TSPCT_MASK			0x7f
#define REG10_TSPCT_SHIFT			0

/* Register 0x11*/
#define REG11_VBUS_GD_MASK			0x80
#define REG11_VBUS_GD_SHIFT			7
#define REG11_VBUSV_MASK			0x7f
#define REG11_VBUSV_SHIFT			0

/* Register 0x12*/
#define REG12_ICHGR_MASK			0x7f
#define REG12_ICHGR_SHIFT			0

/* Register 0x13*/
#define REG13_VDPM_STAT_MASK			0x80
#define REG13_VDPM_STAT_SHIFT			7
#define REG13_IDPM_STAT_MASK			0x40
#define REG13_IDPM_STAT_SHIFT			6
#define REG13_IDPM_LIM_MASK			0x3f
#define REG13_IDPM_LIM_SHIFT			0

/* Register 0x14 */
#define REG14_REG_RESET_MASK			0x80
#define REG14_REG_RESET_SHIFT			7
#define REG14_REG_ICO_OP_MASK			0x40
#define REG14_REG_ICO_OP_SHIFT			6
#define REG14_PN_MASK				0x38
#define REG14_PN_SHIFT				3
#define REG14_TS_PROFILE_MASK			0x04
#define REG14_TS_PROFILE_SHIFT			2
#define REG14_DEV_REV_MASK			0x03
#define REG14_DEV_REV_SHIFT			0

#define REG00_HIZ_DISABLE			0
#define REG00_HIZ_ENABLE			1
#define REG00_EN_ILIM_DISABLE			0
#define REG00_EN_ILIM_ENABLE			1
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_STEP			50
#define REG00_IINLIM_MIN			100
#define REG00_IINLIM_MAX			3250

#define REG01_BHOT_VBHOT1			0
#define REG01_BHOT_VBHOT0			1
#define REG01_BHOT_VBHOT2			2
#define REG01_BHOT_DISABLE			3
#define REG01_BHOT_VBCOLD0			0
#define REG01_BHOT_VBCOLD1			1
#define REG01_VINDPM_OS_OFFSET			0
#define REG01_VINDPM_OS_STEP			100
#define REG01_VINDPM_OS_MIN			0
#define REG01_VINDPM_OS_MAX			3100

#define REG02_CONV_START_DISABLE		0
#define REG02_CONV_START_ENABLE			1
#define REG02_CONV_START_DISABLE		0
#define REG02_CONV_START_ENABLE			1
#define REG02_BOOST_FREQ_1p5M			0
#define REG02_BOOST_FREQ_500K			1
#define REG02_ICO_EN_DISABLE			0
#define REG02_ICO_EN_DENABLE			1
#define REG02_HVDCP_EN_DISABLE			0
#define REG02_HVDCP_EN_DENABLE			1
#define REG02_MAXC_EN_DISABLE			0
#define REG02_MAXC_EN_DENABLE			1
#define REG02_FORCE_DPDM_DISABLE		0
#define REG02_FORCE_DPDM_DENABLE		1
#define REG02_AUTO_DPDM_EN_DISABLE		0
#define REG02_AUTO_DPDM_EN_DENABLE		1

#define REG03_BAT_ENABLE			0
#define REG03_BAT_DISABLE			1
#define REG03_WDT_RESET				1
#define REG03_OTG_DISABLE			0
#define REG03_OTG_ENABLE			1
#define REG03_CHG_DISABLE			0
#define REG03_CHG_ENABLE			1
#define REG03_SYS_MINV_OFFSET			3000
#define REG03_SYS_MINV_STEP			100
#define REG03_SYS_MINV_MIN			3000
#define REG03_SYS_MINV_MAX			3700

#define REG04_EN_PUMPX_DISABLE			0
#define REG04_EN_PUMPX_ENABLE			1
#define REG04_ICHG_OFFSET			0
#define REG04_ICHG_STEP				64
#define REG04_ICHG_MIN				0
#define REG04_ICHG_MAX				5056

#define REG05_IPRECHG_OFFSET			64
#define REG05_IPRECHG_STEP			64
#define REG05_IPRECHG_MIN			64
#define REG05_IPRECHG_MAX			1024
#define REG05_ITERM_OFFSET			64
#define REG05_ITERM_STEP			64
#define REG05_ITERM_MIN				64
#define REG05_ITERM_MAX				1024

#define REG06_VREG_OFFSET			3840
#define REG06_VREG_STEP				16
#define REG06_VREG_MIN				3840
#define REG06_VREG_MAX				4608
#define REG06_BATLOWV_2p8v			0
#define REG06_BATLOWV_3v			1
#define REG06_VRECHG_100MV			0
#define REG06_VRECHG_200MV			1

#define REG07_TERM_DISABLE			0
#define REG07_TERM_ENABLE			1
#define REG07_STAT_DIS_DISABLE			1
#define REG07_STAT_DIS_ENABLE			0
#define REG07_WDT_DISABLE			0
#define REG07_WDT_40S				1
#define REG07_WDT_80S				2
#define REG07_WDT_160S				3
#define REG07_CHG_TIMER_DISABLE			0
#define REG07_CHG_TIMER_ENABLE			1
#define REG07_CHG_TIMER_5HOURS			0
#define REG07_CHG_TIMER_8HOURS			1
#define REG07_CHG_TIMER_12HOURS			2
#define REG07_CHG_TIMER_20HOURS			3
#define REG07_JEITA_ISET_50PCT			0
#define REG07_JEITA_ISET_20PCT			1

#define REG08_COMP_R_OFFSET			0
#define REG08_COMP_R_STEP			20
#define REG08_COMP_R_MIN			0
#define REG08_COMP_R_MAX			140
#define REG08_VCLAMP_OFFSET			0
#define REG08_VCLAMP_STEP			32
#define REG08_VCLAMP_MIN			0
#define REG08_VCLAMP_MAX			224
#define REG08_TREG_60				0
#define REG08_TREG_80				1
#define REG08_TREG_100				2
#define REG08_TREG_120				3

#define REG09_FORCE_ICO_DISABLE			0
#define REG09_FORCE_ICO_ENABLE			1
#define REG09_TMR2X_EN_DISABLE			0
#define REG09_TMR2X_EN_ENABLE			1
#define REG09_BATFET_DIS_DISABLE		0
#define REG09_BATFET_DIS_ENABLE			1
#define REG09_JEITA_VSET_DISABLE		0
#define REG09_JEITA_VSET_ENABLE			1
#define REG09_BATFET_DLY_DISABLE		0
#define REG09_BATFET_DLY_ENABLE			1
#define REG09_BATFET_RST_EN_DISABLE		0
#define REG09_BATFET_RST_EN_ENABLE		1
#define REG09_PUMPX_UP_DISABLE			0
#define REG09_PUMPX_UP_ENABLE			1
#define REG09_PUMPX_DN_DISABLE			0
#define REG09_PUMPX_DN_ENABLE			1

#define REG0A_BOOSTV_OFFSET			4550
#define REG0A_BOOSTV_STEP			64
#define REG0A_BOOSTV_MIN			4550
#define REG0A_BOOSTV_MAX			5510
#define REG0A_BOOSTV_LIM_500MA			0
#define REG0A_BOOSTV_LIM_750MA			1
#define REG0A_BOOSTV_LIM_1200MA			2
#define REG0A_BOOSTV_LIM_1400MA			3
#define REG0A_BOOSTV_LIM_1650MA			4
#define REG0A_BOOSTV_LIM_1875MA			5
#define REG0A_BOOSTV_LIM_2150MA			6
#define REG0A_BOOSTV_LIM_2450MA			7

#define REG0B_VBUS_TYPE_NONE			0
#define REG0B_VBUS_TYPE_USB_SDP			1
#define REG0B_VBUS_TYPE_USB_CDP			2
#define REG0B_VBUS_TYPE_USB_DCP			3
#define REG0B_VBUS_TYPE_DCP			4
#define REG0B_VBUS_TYPE_UNKNOWN			5
#define REG0B_VBUS_TYPE_ADAPTER			6
#define REG0B_VBUS_TYPE_OTG			7


#define REG0B_CHRG_STAT_IDLE			0
#define REG0B_CHRG_STAT_PRECHG			1
#define REG0B_CHRG_STAT_FASTCHG			2
#define REG0B_CHRG_STAT_CHGDONE			3
#define REG0B_POWER_NOT_GOOD			0
#define REG0B_POWER_GOOD			1
#define REG0B_NOT_IN_VSYS_STAT			0
#define REG0B_IN_VSYS_STAT			1

#define REG0C_FAULT_WDT				1
#define REG0C_FAULT_BOOST			1
#define REG0C_FAULT_CHRG_NORMAL			0
#define REG0C_FAULT_CHRG_INPUT			1
#define REG0C_FAULT_CHRG_THERMAL		2
#define REG0C_FAULT_CHRG_TIMER			3
#define REG0C_FAULT_BAT_OVP			1
#define REG0C_FAULT_NTC_NORMAL			0
#define REG0C_FAULT_NTC_WARM			2
#define REG0C_FAULT_NTC_COOL			3
#define REG0C_FAULT_NTC_COLD			5
#define REG0C_FAULT_NTC_HOT			6

#define REG0D_FORCE_VINDPM_DISABLE		0
#define REG0D_FORCE_VINDPM_ENABLE		1
#define REG0D_VINDPM_OFFSET			2600
#define REG0D_VINDPM_STEP			100
#define REG0D_VINDPM_MIN			3900
#define REG0D_VINDPM_MAX			15300

#define REG0E_THERM_STAT			1
#define REG0E_BATV_OFFSET			2304
#define REG0E_BATV_STEP				20
#define REG0E_BATV_MIN				2304
#define REG0E_BATV_MAX				4848

#define REG0F_SYSV_OFFSET			2304
#define REG0F_SYSV_STEP				20
#define REG0F_SYSV_MIN				2304
#define REG0F_SYSV_MAX				4848

#define REG10_TSPCT_OFFSET			21
#define REG10_TSPCT_STEP			0.465
#define REG10_TSPCT_MIN				21
#define REG10_TSPCT_MAX				80

#define REG11_VBUS_GD				1
#define REG11_VBUSV_OFFSET			2600
#define REG11_VBUSV_STEP			100
#define REG11_VBUSV_MIN				2600
#define REG11_VBUSV_MAX				15300

#define REG12_ICHGR_OFFSET			0
#define REG12_ICHGR_MIN				0
#define REG12_ICHGR_MAX				6350

#define REG13_VDPM_STAT				1
#define REG13_IDPM_STAT				1
#define REG13_IDPM_LIM_OFFSET			100
#define REG13_IDPM_LIM_STEP			50
#define REG13_IDPM_LIM_MIN			100
#define REG13_IDPM_LIM_MAX			3250

#define REG14_REG_RESET				1
#define REG14_REG_ICO_OP			1
/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
#define REG14_PN				1
/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
#define SY6970_REG_CHARGE_DONE_MASK		GENMASK(4, 3)
#define SY6970_REG_CHARGE_DONE_SHIFT		3
#define SY6970_CHARGE_DONE                 0x3

#define SY6970_REG_RECHG_MASK			GENMASK(0, 0)
#define SY6970_REG_RECHG_SHIFT			0
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
/* Other Realted Definition*/
#define SY6970_BATTERY_NAME			"sc27xx-fgu"

#define BIT_DP_DM_BC_ENB			BIT(0)

#define SY6970_WDT_VALID_MS			50

#define SY6970_OTG_ALARM_TIMER_MS		15000
#define SY6970_OTG_VALID_MS			500
#define SY6970_OTG_RETRY_TIMES			10

#define SY6970_DISABLE_PIN_MASK		BIT(0)
#define SY6970_DISABLE_PIN_MASK_2721		BIT(15)

#define SY6970_FAST_CHG_VOL_MAX		10500000
#define SY6970_NORMAL_CHG_VOL_MAX		6500000

#define SY6970_WAKE_UP_MS			2000
/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
#define SY6970_CURRENT_WORK_MS		msecs_to_jiffies(100)
/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end*/

struct sy6970_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sy6970_dump_reg;
	struct device_attribute attr_sy6970_lookup_reg;
	struct device_attribute attr_sy6970_sel_reg_id;
	struct device_attribute attr_sy6970_reg_val;
	struct attribute *attrs[5];

	struct sy6970_charger_info *info;
};

struct sy6970_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	struct notifier_block pd_swap_notify;
	struct notifier_block extcon_nb;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	struct delayed_work cur_work;
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 role;
	bool need_disable_Q1;
	int termination_cur;
	int vol_max_mv;
	u32 actual_limit_current;
	bool otg_enable;
	struct alarm otg_timer;
	struct sy6970_charger_sysfs *sysfs;
	int reg_id;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	bool is_sink;
	bool use_typec_extcon;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
};

struct sy6970_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sy6970_charger_reg_tab reg_tab[SY6970_REG_NUM + 1] = {
	{0, SY6970_REG_00, "Setting Input Limit Current reg"},
	{1, SY6970_REG_01, "Setting Vindpm_OS reg"},
	{2, SY6970_REG_02, "Related Function Enable reg"},
	{3, SY6970_REG_03, "Related Function Config reg"},
	{4, SY6970_REG_04, "Setting Charge Limit Current reg"},
	{5, SY6970_REG_05, "Setting Terminal Current reg"},
	{6, SY6970_REG_06, "Setting Charge Limit Voltage reg"},
	{7, SY6970_REG_07, "Related Function Config reg"},
	{8, SY6970_REG_08, "IR Compensation Resistor Setting reg"},
	{9, SY6970_REG_09, "Related Function Config reg"},
	{10, SY6970_REG_0A, "Boost Mode Related Setting reg"},
	{11, SY6970_REG_0B, "Status reg"},
	{12, SY6970_REG_0C, "Fault reg"},
	{13, SY6970_REG_0D, "Setting Vindpm reg"},
	{14, SY6970_REG_0E, "ADC Conversion of Battery Voltage reg"},
	{15, SY6970_REG_0F, "ADDC Conversion of System Voltage reg"},
	{16, SY6970_REG_10, "ADC Conversion of TS Voltage as Percentage of REGN reg"},
	{17, SY6970_REG_11, "ADC Conversion of VBUS voltage reg"},
	{18, SY6970_REG_12, "ICHGR Setting reg"},
	{19, SY6970_REG_13, "IDPM Limit Setting reg"},
	{20, SY6970_REG_14, "Related Function Config reg"},
	{21, 0, "null"},
};

static int sy6970_charger_set_limit_current(struct sy6970_charger_info *info,
					     u32 limit_cur);

static int sy6970_read(struct sy6970_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sy6970_write(struct sy6970_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sy6970_update_bits(struct sy6970_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sy6970_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sy6970_write(info, reg, v);
}

static void sy6970_charger_dump_register(struct sy6970_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SY6970_REG_NUM; i++) {
		ret = sy6970_read(info, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       reg_val);
			idx += len;
		}
	}

	dev_err(info->dev, "%s: %s", __func__, buf);
}

static bool sy6970_charger_is_bat_present(struct sy6970_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SY6970_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (!ret && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n", ret);

	return present;
}

static int sy6970_charger_is_fgu_present(struct sy6970_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SY6970_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
static int sy6970_charger_get_pn_value(struct sy6970_charger_info *info)
{
	u8 reg_val;
	int ret;

	ret = sy6970_read(info, SY6970_REG_14, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to get PN value, ret = %d\n", ret);
		return ret;
	}

	reg_val &= REG14_PN_MASK;
	reg_val >>= REG14_PN_SHIFT;
	if (reg_val != REG14_PN) {
		dev_err(info->dev, "The PN value is 0x%x\n", reg_val);
		return -EINVAL;
	}

	return 0;
}
/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
static int sy6970_charger_set_recharge(struct sy6970_charger_info *info)
{
	int ret = 0;

	ret = sy6970_update_bits(info, SY6970_REG_06,
				  SY6970_REG_RECHG_MASK,
				  0x1 << SY6970_REG_RECHG_SHIFT);

	return ret;
}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
static int sy6970_charger_set_vindpm(struct sy6970_charger_info *info, u32 vol)
{
	u8 reg_val;
	int ret;

	ret = sy6970_update_bits(info, SY6970_REG_0D, REG0D_FORCE_VINDPM_MASK,
				  REG0D_FORCE_VINDPM_ENABLE << REG0D_FORCE_VINDPM_SHIFT);
	if (ret) {
		dev_err(info->dev, "set force vindpm failed\n");
		return ret;
	}

	if (vol < REG0D_VINDPM_MIN)
		vol = REG0D_VINDPM_MIN;
	else if (vol > REG0D_VINDPM_MAX)
		vol = REG0D_VINDPM_MAX;
	reg_val = (vol - REG0D_VINDPM_OFFSET) / REG0D_VINDPM_STEP;
	/* Tab A8 code for AX6300DEV-1961 by qiaodan at 20211028 start */
	return sy6970_update_bits(info, SY6970_REG_0D,
				 REG0D_VINDPM_MASK, reg_val);
	/* Tab A8 code for AX6300DEV-1961 by qiaodan at 20211028 end */
}

static int sy6970_charger_set_termina_vol(struct sy6970_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < REG06_VREG_MIN)
		vol = REG06_VREG_MIN;
	else if (vol >= REG06_VREG_MAX)
		vol = REG06_VREG_MAX;
	reg_val = (vol - REG06_VREG_OFFSET) / REG06_VREG_STEP;

	return sy6970_update_bits(info, SY6970_REG_06, REG06_VREG_MASK,
				   reg_val << REG06_VREG_SHIFT);
}

static int sy6970_charger_set_termina_cur(struct sy6970_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= REG05_ITERM_MIN)
		cur = REG05_ITERM_MIN;
	else if (cur >= REG05_ITERM_MAX)
		cur = REG05_ITERM_MAX;
	reg_val = (cur - REG05_ITERM_OFFSET) / REG05_ITERM_STEP;

	return sy6970_update_bits(info, SY6970_REG_05, REG05_ITERM_MASK, reg_val);
}

/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
static int sy6970_charger_en_chg_timer(struct sy6970_charger_info *info, bool val)
{
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (val) {
		ret = sy6970_update_bits(info, SY6970_REG_07,
				  REG07_EN_TIMER_MASK,
				  0x1 << REG07_EN_TIMER_SHIFT);
		pr_info("sy6970 EN_TIMER is enabled\n");
	} else {
		ret = sy6970_update_bits(info, SY6970_REG_07,
				  REG07_EN_TIMER_MASK,
				  0x0 << REG07_EN_TIMER_SHIFT);
		pr_info("sy6970 EN_TIMER is disabled\n");
	}

	return ret;
}
/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */

static int sy6970_charger_hw_init(struct sy6970_charger_info *info)
{
	struct power_supply_battery_info bat_info;
	int ret;
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	int bat_id = 0;

	bat_id = battery_get_bat_id();
	ret = power_supply_get_battery_info(info->psy_usb, &bat_info, bat_id);
	// ret = power_supply_get_battery_info(info->psy_usb, &bat_info, 0);
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 1500000;
		info->cur.dcp_cur = 1500000;
		info->cur.cdp_limit = 1000000;
		info->cur.cdp_cur = 1000000;
		info->cur.unknown_limit = 500000;
		info->cur.unknown_cur = 500000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
		info->termination_cur = bat_info.charge_term_current_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = sy6970_update_bits(info, SY6970_REG_14, REG14_REG_RESET_MASK,
					  REG14_REG_RESET << REG14_REG_RESET_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset sy6970 failed\n");
			return ret;
		}

		ret = sy6970_charger_set_vindpm(info, info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sy6970 vindpm vol failed\n");
			return ret;
		}

		ret = sy6970_charger_set_termina_vol(info,
						      info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sy6970 terminal vol failed\n");
			return ret;
		}

		ret = sy6970_charger_set_termina_cur(info, info->termination_cur);
		if (ret) {
			dev_err(info->dev, "set sy6970 terminal cur failed\n");
			return ret;
		}
		/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
		ret = sy6970_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret) {
			dev_err(info->dev, "set sy6970 limit current failed\n");
			return ret;
		}

		sy6970_charger_set_recharge(info);
		if (ret) {
			dev_err(info->dev, "set sy6970 recharge failed\n");
			return ret;
		}
		/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
		/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
		ret = sy6970_charger_en_chg_timer(info, false);
		/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */
	}

	return ret;
}

/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 start */
static int sy6970_enter_hiz_mode(struct sy6970_charger_info *info)
{
	int ret;

	ret = sy6970_update_bits(info, SY6970_REG_00,
				  REG00_ENHIZ_MASK, REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "enter HIZ mode failed\n");

	return ret;
}

static int sy6970_exit_hiz_mode(struct sy6970_charger_info *info)
{
	int ret;

	ret = sy6970_update_bits(info, SY6970_REG_00,
				  REG00_ENHIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "exit HIZ mode failed\n");

	return ret;
}

static int sy6970_get_hiz_mode(struct sy6970_charger_info *info,u32 *value)
{
	u8 buf;
	int ret;

	ret = sy6970_read(info, SY6970_REG_00, &buf);
	*value = (buf & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	return ret;
}
/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 end */

static int sy6970_charger_get_charge_voltage(struct sy6970_charger_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SY6970_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SY6970_BATTERY_NAME\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static int sy6970_charger_start_charge(struct sy6970_charger_info *info)
{
	int ret;

	ret = sy6970_update_bits(info, SY6970_REG_00,
				  REG00_ENHIZ_MASK, REG00_HIZ_DISABLE);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	/* Tab A8 code for AX6300DEV-367 by wangjian at 20210916 start */
	ret = sy6970_update_bits(info, SY6970_REG_03,
				  REG03_CHG_CONFIG_MASK, REG03_CHG_ENABLE << REG03_CHG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable charge failed\n");
	}
	/* Tab A8 code for AX6300DEV-367 by wangjian at 20210916 end */

	ret = sy6970_update_bits(info, SY6970_REG_07, REG07_WDT_MASK,
				  REG07_WDT_40S << REG07_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sy6970 watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable sy6970 charge failed\n");
			return ret;
		}

	ret = sy6970_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sy6970_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sy6970 terminal cur failed\n");

	return ret;
}

static void sy6970_charger_stop_charge(struct sy6970_charger_info *info)
{
	int ret;
	bool present = sy6970_charger_is_bat_present(info);

	if (!present || info->need_disable_Q1) {
		ret = sy6970_update_bits(info, SY6970_REG_00, REG00_ENHIZ_MASK,
					  REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		info->need_disable_Q1 = false;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable sy6970 charge failed\n");

	ret = sy6970_update_bits(info, SY6970_REG_07, REG07_WDT_MASK,
				  REG07_WDT_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable sy6970 watchdog\n");

}

static int sy6970_charger_set_current(struct sy6970_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	int ret;

	cur = cur / 1000;
	if (cur <= REG04_ICHG_MIN)
		cur = REG04_ICHG_MIN;
	else if (cur >= REG04_ICHG_MAX)
		cur = REG04_ICHG_MAX;
	reg_val = cur / REG04_ICHG_STEP;

	ret = sy6970_update_bits(info, SY6970_REG_04, REG04_ICHG_MASK, reg_val);

	return ret;
}

static int sy6970_charger_get_current(struct sy6970_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sy6970_read(info, SY6970_REG_04, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG04_ICHG_MASK;
	*cur = reg_val * REG04_ICHG_STEP * 1000;

	return 0;
}

static int sy6970_charger_set_limit_current(struct sy6970_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sy6970_update_bits(info, SY6970_REG_00, REG00_EN_ILIM_MASK,
				  REG00_EN_ILIM_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable en_ilim failed\n");
		return ret;
	}

	limit_cur = limit_cur / 1000;
	if (limit_cur >= REG00_IINLIM_MAX)
		limit_cur = REG00_IINLIM_MAX;
	if (limit_cur <= REG00_IINLIM_MIN)
		limit_cur = REG00_IINLIM_MIN;

	info->last_limit_current = limit_cur * 1000;
	reg_val = (limit_cur - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;

	ret = sy6970_update_bits(info, SY6970_REG_00, REG00_IINLIM_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sy6970 limit cur failed\n");

	info->actual_limit_current =
		(reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET) * 1000;

	return ret;
}

static u32 sy6970_charger_get_limit_current(struct sy6970_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sy6970_read(info, SY6970_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	*limit_cur = reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET;
	if (*limit_cur >= REG00_IINLIM_MAX)
		*limit_cur = REG00_IINLIM_MAX * 1000;
	else
		*limit_cur = *limit_cur * 1000;

	return 0;
}

static inline int sy6970_charger_get_health(struct sy6970_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int sy6970_charger_get_online(struct sy6970_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int sy6970_charger_feed_watchdog(struct sy6970_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;

	ret = sy6970_update_bits(info, SY6970_REG_03, REG03_WDT_RESET_MASK,
				  REG03_WDT_RESET << REG03_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sy6970 failed\n");
		return ret;
	}

	ret = sy6970_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = sy6970_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}

static int sy6970_charger_set_fchg_current(struct sy6970_charger_info *info,
					    u32 val)
{
	int ret, limit_cur, cur;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		limit_cur = info->cur.fchg_limit;
		cur = info->cur.fchg_cur;
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		limit_cur = info->cur.dcp_limit;
		cur = info->cur.dcp_cur;
	} else {
		return 0;
	}

	ret = sy6970_charger_set_limit_current(info, limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg limit current\n");
		return ret;
	}

	ret = sy6970_charger_set_current(info, cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg current\n");
		return ret;
	}

	return 0;
}

static inline int sy6970_charger_get_status(struct sy6970_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
static int sy6970_charger_get_charge_done(struct sy6970_charger_info *info,
	union power_supply_propval *val)
{
	int ret = 0;
	u8 reg_val = 0;

	if (!info || !val) {
		dev_err(info->dev, "[%s]line=%d: info or val is NULL\n", __FUNCTION__, __LINE__);
		return ret;
	}

	ret = sy6970_read(info, SY6970_REG_0B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to get charge_done, ret = %d\n", ret);
		return ret;
	}

	reg_val &= SY6970_REG_CHARGE_DONE_MASK;
	reg_val >>= SY6970_REG_CHARGE_DONE_SHIFT;
	val->intval = (reg_val == SY6970_CHARGE_DONE);

	return 0;
}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end*/
static int sy6970_charger_set_status(struct sy6970_charger_info *info,
				      int val)
{
	int ret = 0;
	u32 input_vol;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		ret = sy6970_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge current\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		ret = sy6970_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 5V normal charge current\n");
			return ret;
		}
		ret = sy6970_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 9V charge voltage\n");
			return ret;
		}
		if (input_vol > SY6970_FAST_CHG_VOL_MAX)
			info->need_disable_Q1 = true;
	} else if (val == false) {
		ret = sy6970_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 5V charge voltage\n");
			return ret;
		}
		if (input_vol > SY6970_NORMAL_CHG_VOL_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sy6970_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sy6970_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void sy6970_charger_work(struct work_struct *data)
{
	struct sy6970_charger_info *info =
		container_of(data, struct sy6970_charger_info, work);
	bool present;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	int retry_cnt = 12;

	if (info->use_typec_extcon && info->limit) {
		/* if use typec extcon notify charger,
		 * wait for BC1.2 detect charger type.
		*/
		while (retry_cnt > 0) {
			if (info->usb_phy->chg_type != UNKNOWN_TYPE) {
				break;
			}
			retry_cnt--;
			msleep(50);
		}
		dev_info(info->dev, "retry_cnt = %d\n", retry_cnt);
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	present = sy6970_charger_is_bat_present(info);

	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static ssize_t sy6970_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sy6970_charger_sysfs *sy6970_sysfs =
		container_of(attr, struct sy6970_charger_sysfs,
			     attr_sy6970_reg_val);
	struct sy6970_charger_info *info = sy6970_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return sprintf(buf, "%s sy6970_sysfs->info is null\n", __func__);

	ret = sy6970_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get sy6970_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get sy6970_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "sy6970_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t sy6970_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sy6970_charger_sysfs *sy6970_sysfs =
		container_of(attr, struct sy6970_charger_sysfs,
			     attr_sy6970_reg_val);
	struct sy6970_charger_info *info = sy6970_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sy6970_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sy6970_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sy6970_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sy6970_charger_sysfs *sy6970_sysfs =
		container_of(attr, struct sy6970_charger_sysfs,
			     attr_sy6970_sel_reg_id);
	struct sy6970_charger_info *info = sy6970_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sy6970_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sy6970_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SY6970_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sy6970_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sy6970_sysfs->name, id);
	return count;
}

static ssize_t sy6970_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sy6970_charger_sysfs *sy6970_sysfs =
		container_of(attr, struct sy6970_charger_sysfs,
			     attr_sy6970_sel_reg_id);
	struct sy6970_charger_info *info = sy6970_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sy6970_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sy6970_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sy6970_charger_sysfs *sy6970_sysfs =
		container_of(attr, struct sy6970_charger_sysfs,
			     attr_sy6970_lookup_reg);
	struct sy6970_charger_info *info = sy6970_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return sprintf(buf, "%s sy6970_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SY6970_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sy6970_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sy6970_charger_sysfs *sy6970_sysfs =
		container_of(attr, struct sy6970_charger_sysfs,
			     attr_sy6970_dump_reg);
	struct sy6970_charger_info *info = sy6970_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sy6970_sysfs->info is null\n", __func__);

	sy6970_charger_dump_register(info);

	return sprintf(buf, "%s\n", sy6970_sysfs->name);
}

static int sy6970_register_sysfs(struct sy6970_charger_info *info)
{
	struct sy6970_charger_sysfs *sy6970_sysfs;
	int ret;

	sy6970_sysfs = devm_kzalloc(info->dev, sizeof(*sy6970_sysfs), GFP_KERNEL);
	if (!sy6970_sysfs)
		return -ENOMEM;

	info->sysfs = sy6970_sysfs;
	sy6970_sysfs->name = "sy6970_sysfs";
	sy6970_sysfs->info = info;
	sy6970_sysfs->attrs[0] = &sy6970_sysfs->attr_sy6970_dump_reg.attr;
	sy6970_sysfs->attrs[1] = &sy6970_sysfs->attr_sy6970_lookup_reg.attr;
	sy6970_sysfs->attrs[2] = &sy6970_sysfs->attr_sy6970_sel_reg_id.attr;
	sy6970_sysfs->attrs[3] = &sy6970_sysfs->attr_sy6970_reg_val.attr;
	sy6970_sysfs->attrs[4] = NULL;
	sy6970_sysfs->attr_g.name = "debug";
	sy6970_sysfs->attr_g.attrs = sy6970_sysfs->attrs;

	sysfs_attr_init(&sy6970_sysfs->attr_sy6970_dump_reg.attr);
	sy6970_sysfs->attr_sy6970_dump_reg.attr.name = "sy6970_dump_reg";
	sy6970_sysfs->attr_sy6970_dump_reg.attr.mode = 0444;
	sy6970_sysfs->attr_sy6970_dump_reg.show = sy6970_dump_reg_show;

	sysfs_attr_init(&sy6970_sysfs->attr_sy6970_lookup_reg.attr);
	sy6970_sysfs->attr_sy6970_lookup_reg.attr.name = "sy6970_lookup_reg";
	sy6970_sysfs->attr_sy6970_lookup_reg.attr.mode = 0444;
	sy6970_sysfs->attr_sy6970_lookup_reg.show = sy6970_reg_table_show;

	sysfs_attr_init(&sy6970_sysfs->attr_sy6970_sel_reg_id.attr);
	sy6970_sysfs->attr_sy6970_sel_reg_id.attr.name = "sy6970_sel_reg_id";
	sy6970_sysfs->attr_sy6970_sel_reg_id.attr.mode = 0644;
	sy6970_sysfs->attr_sy6970_sel_reg_id.show = sy6970_reg_id_show;
	sy6970_sysfs->attr_sy6970_sel_reg_id.store = sy6970_reg_id_store;

	sysfs_attr_init(&sy6970_sysfs->attr_sy6970_reg_val.attr);
	sy6970_sysfs->attr_sy6970_reg_val.attr.name = "sy6970_reg_val";
	sy6970_sysfs->attr_sy6970_reg_val.attr.mode = 0644;
	sy6970_sysfs->attr_sy6970_reg_val.show = sy6970_reg_val_show;
	sy6970_sysfs->attr_sy6970_reg_val.store = sy6970_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sy6970_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
static void sy6970_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sy6970_charger_info *info =
	 container_of(dwork, struct sy6970_charger_info, cur_work);
	u32 limit_cur = 0;
	int ret = 0;

	ret = sy6970_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return;
	}

	if (info->actual_limit_current == limit_cur)
		return;

	ret = sy6970_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return;
	}
}
/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
static int sy6970_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sy6970_charger_info *info =
		container_of(nb, struct sy6970_charger_info, usb_notify);

	info->limit = limit;

	pm_wakeup_event(info->dev, SY6970_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
static int sy6970_charger_extcon_event(struct notifier_block *nb,
				  unsigned long event, void *param)
{
	struct sy6970_charger_info *info =
		container_of(nb, struct sy6970_charger_info, extcon_nb);
	int state = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	state = extcon_get_state(info->edev, EXTCON_SINK);
	if (state < 0) {
		return NOTIFY_OK;
	}

	if (info->is_sink == state) {
		return NOTIFY_OK;
	}

	info->is_sink = state;

	if (info->is_sink) {
		info->limit = 500;
	} else {
		info->limit = 0;
	}

	pm_wakeup_event(info->dev, SY6970_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
static int sy6970_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sy6970_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info)
		return -ENOMEM;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = sy6970_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sy6970_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sy6970_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = sy6970_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sy6970_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev, "get sy6970 charge status failed\n");
			goto out;
		}

		val->intval = !enabled;
		break;
	/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 start */
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		ret = sy6970_get_hiz_mode(info, &enabled);
		val->intval = !enabled;
		break;
	case POWER_SUPPLY_PROP_DUMP_CHARGER_IC:
		sy6970_charger_dump_register(info);
		break;
	/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 end */
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		sy6970_charger_get_charge_done(info, val);
		break;
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sy6970_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sy6970_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info)
		return -ENOMEM;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sy6970_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sy6970_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
		schedule_delayed_work(&info->cur_work, SY6970_CURRENT_WORK_MS*5);
		/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = sy6970_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = sy6970_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sy6970_charger_set_termina_vol(info, val->intval / 1000);
/* Tab A8 code for AX6300DEV-1961 by qiaodan at 20211028 start */
#ifdef HQ_D85_BUILD
		if (ret < 0) {
			dev_err(info->dev, "failed to set terminate voltage\n");
		} else {
			ret = sy6970_charger_set_vindpm(info, 4300);
			if (ret < 0) {
				dev_err(info->dev, "failed to set vindpm\n");
			}
		}
#else
		if (ret < 0) {
			dev_err(info->dev, "failed to set terminate voltage\n");
		}
#endif
/* Tab A8 code for AX6300DEV-1961 by qiaodan at 20211028 end */
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (val->intval == true) {
			ret = sy6970_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sy6970_charger_stop_charge(info);
		}
		break;
	/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 start */
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		if(val->intval) {
			sy6970_exit_hiz_mode(info);
		} else {
			sy6970_enter_hiz_mode(info);
		}
		break;
	/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 end */
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
	case POWER_SUPPLY_PROP_EN_CHG_TIMER:
		if(val->intval)
			ret = sy6970_charger_en_chg_timer(info, true);
		else
			ret = sy6970_charger_en_chg_timer(info, false);
		break;
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sy6970_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
	case POWER_SUPPLY_PROP_EN_CHG_TIMER:
        case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sy6970_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property sy6970_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 start */
	POWER_SUPPLY_PROP_POWER_PATH_ENABLED,
	POWER_SUPPLY_PROP_DUMP_CHARGER_IC,
	/* Tab A8 code for SR-AX6300-01-102 by wenyaqi at 20210808 end */
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
	POWER_SUPPLY_PROP_CHARGE_DONE,
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
};

static const struct power_supply_desc sy6970_charger_desc = {
	.name			= "sy6970_charger",
	/* Tab A8 code for P211115-05893 by wenyaqi at 20211116 start */
	// .type			= POWER_SUPPLY_TYPE_USB,
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	/* Tab A8 code for P211115-05893 by wenyaqi at 20211116 end */
	.properties		= sy6970_usb_props,
	.num_properties		= ARRAY_SIZE(sy6970_usb_props),
	.get_property		= sy6970_charger_usb_get_property,
	.set_property		= sy6970_charger_usb_set_property,
	.property_is_writeable	= sy6970_charger_property_is_writeable,
	.usb_types		= sy6970_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sy6970_charger_usb_types),
};
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
static void sy6970_charger_detect_status(struct sy6970_charger_info *info)
{
	int state = 0;

	if (info->use_typec_extcon) {
		state = extcon_get_state(info->edev, EXTCON_SINK);
		if (state < 0) {
			return;
		}
		if (state == 0) {
			return;
		}
		info->is_sink = state;

		if (info->is_sink) {
			info->limit = 500;
		}

		schedule_work(&info->work);
	} else {
		unsigned int min, max;

		/*
		 * If the USB charger status has been USB_CHARGER_PRESENT before
		 * registering the notifier, we should start to charge with getting
		 * the charge current.
		 */
		if (info->usb_phy->chg_state != USB_CHARGER_PRESENT) {
			return;
		}

		usb_phy_get_charger_current(info->usb_phy, &min, &max);
		info->limit = min;

		/*
		 * slave no need to start charge when vbus change.
		 * due to charging in shut down will check each psy
		 * whether online or not, so let info->limit = min.
		 */
		schedule_work(&info->work);
	}
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
static void sy6970_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sy6970_charger_info *info = container_of(dwork,
							 struct sy6970_charger_info,
							 wdt_work);
	int ret;

	ret = sy6970_update_bits(info, SY6970_REG_03, REG03_WDT_RESET_MASK,
				  REG03_WDT_RESET << REG03_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sy6970 failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool sy6970_charger_check_otg_valid(struct sy6970_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sy6970_read(info, SY6970_REG_03, &value);
	if (ret) {
		dev_err(info->dev, "get sy6970 charger otg valid status failed\n");
		return status;
	}

	if (value & REG03_OTG_CONFIG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_3 = 0x%x\n", value);

	return status;
}

static bool sy6970_charger_check_otg_fault(struct sy6970_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sy6970_read(info, SY6970_REG_0C, &value);
	if (ret) {
		dev_err(info->dev, "get sy6970 charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG0C_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_0C = 0x%x\n",
			value);

	return status;
}

static void sy6970_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sy6970_charger_info *info = container_of(dwork,
			struct sy6970_charger_info, otg_work);
	bool otg_valid = sy6970_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = sy6970_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sy6970_update_bits(info, SY6970_REG_03,
						  REG03_OTG_CONFIG_MASK,
						  REG03_OTG_ENABLE << REG03_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart sy6970 charger otg failed\n");
		}

		otg_valid = sy6970_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SY6970_OTG_RETRY_TIMES);

	if (retry >= SY6970_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sy6970_charger_enable_otg(struct regulator_dev *dev)
{
	struct sy6970_charger_info *info = rdev_get_drvdata(dev);
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	int ret = 0;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
#if 0
	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		goto out;
	}
#endif
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	ret = sy6970_update_bits(info, SY6970_REG_03, REG03_OTG_CONFIG_MASK,
				  REG03_OTG_ENABLE << REG03_OTG_CONFIG_SHIFT);

	if (ret) {
		dev_err(info->dev, "enable sy6970 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		goto out;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SY6970_WDT_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SY6970_OTG_VALID_MS));
out:
	mutex_unlock(&info->lock);
	return 0;
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
}

static int sy6970_charger_disable_otg(struct regulator_dev *dev)
{
	struct sy6970_charger_info *info = rdev_get_drvdata(dev);
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	int ret = 0;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sy6970_update_bits(info, SY6970_REG_03,
				  REG03_OTG_CONFIG_MASK, REG03_OTG_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable sy6970 otg failed\n");
		goto out;
	}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
#if 0
	/* Enable charger detection function to identify the charger type */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
	if (ret)
		dev_err(info->dev, "enable BC1.2 failed\n");
#endif
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
out:
	mutex_unlock(&info->lock);
	return ret;
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
}

static int sy6970_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sy6970_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	mutex_lock(&info->lock);
	ret = sy6970_read(info, SY6970_REG_03, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sy6970 otg status\n");
		mutex_unlock(&info->lock);
		return ret;
	}

	val &= REG03_OTG_CONFIG_MASK;
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	dev_info(info->dev, "%s:line%d val = %d\n", __func__, __LINE__, val);
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */

	mutex_unlock(&info->lock);
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */
	return val;
}

static const struct regulator_ops sy6970_charger_vbus_ops = {
	.enable = sy6970_charger_enable_otg,
	.disable = sy6970_charger_disable_otg,
	.is_enabled = sy6970_charger_vbus_is_enabled,
};

static const struct regulator_desc sy6970_charger_vbus_desc = {
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	// .name = "otg-vbus",
	// .of_match = "otg-vbus",
	.name = "sy6970_otg_vbus",
	.of_match = "sy6970_otg_vbus",
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sy6970_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sy6970_charger_register_vbus_regulator(struct sy6970_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sy6970_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int sy6970_charger_register_vbus_regulator(struct sy6970_charger_info *info)
{
	return 0;
}
#endif

static int sy6970_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sy6970_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	client->addr = 0x6A;
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
	info->client = client;
	info->dev = dev;

	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	ret = sy6970_charger_get_pn_value(info);
	if (ret) {
		dev_err(dev, "failed to get pn value\n");
		return ret;
	}
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	info->use_typec_extcon =
		device_property_read_bool(dev, "use-typec-extcon");
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	INIT_WORK(&info->work, sy6970_charger_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	INIT_DELAYED_WORK(&info->cur_work, sy6970_current_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = sy6970_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	if (info->use_typec_extcon) {
		info->extcon_nb.notifier_call = sy6970_charger_extcon_event;
		ret = devm_extcon_register_notifier_all(dev,
							info->edev,
							&info->extcon_nb);
		if (ret) {
			dev_err(dev, "Can't register extcon\n");
			return ret;
		}
 	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SY6970_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = SY6970_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &sy6970_charger_desc,
						   &charger_cfg);

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	mutex_init(&info->lock);
	mutex_lock(&info->lock);
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}

	ret = sy6970_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sy6970_charger_hw_init\n");
		goto err_mutex_lock;
	}

	sy6970_charger_stop_charge(info);

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	ret = sy6970_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto err_psy_usb;
	}
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */

	device_init_wakeup(info->dev, true);
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	if (!info->use_typec_extcon) {
		info->usb_notify.notifier_call = sy6970_charger_usb_change;
		ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
		if (ret) {
			dev_err(dev, "failed to register notifier:%d\n", ret);
			goto err_psy_usb;
		}
 	}
	 /* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */

	ret = sy6970_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	sy6970_charger_detect_status(info);

	ret = sy6970_update_bits(info, SY6970_REG_07, REG07_WDT_MASK,
				  REG07_WDT_40S << REG07_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sy6970 watchdog\n");
		/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
		goto err_mutex_lock;
                /* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */
	}

	INIT_DELAYED_WORK(&info->otg_work, sy6970_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  sy6970_charger_feed_watchdog_work);
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 start */
	pr_info("[%s]line=%d: probe success\n", __FUNCTION__, __LINE__);
	/* Tab A8 code for SR-AX6300-01-104 by wangjian at 20210811 end */

        /* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	mutex_unlock(&info->lock);
	return 0;

err_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
err_mutex_lock:
	mutex_unlock(&info->lock);
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */
	mutex_destroy(&info->lock);

	return ret;
}

static void sy6970_charger_shutdown(struct i2c_client *client)
{
	struct sy6970_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	cancel_delayed_work_sync(&info->cur_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sy6970_update_bits(info, SY6970_REG_03,
					  REG03_OTG_CONFIG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable sy6970 otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}
}

static int sy6970_charger_remove(struct i2c_client *client)
{
	struct sy6970_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sy6970_charger_suspend(struct device *dev)
{
	struct sy6970_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = SY6970_OTG_ALARM_TIMER_MS;
	int ret;

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	cancel_delayed_work_sync(&info->cur_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */

	/* feed watchdog first before suspend */
	ret = sy6970_update_bits(info, SY6970_REG_03, REG03_WDT_RESET_MASK,
				  REG03_WDT_RESET << REG03_WDT_RESET_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sy6970 failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int sy6970_charger_resume(struct device *dev)
{
	struct sy6970_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = sy6970_update_bits(info, SY6970_REG_03, REG03_WDT_RESET_MASK,
				  REG03_WDT_RESET << REG03_WDT_RESET_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sy6970 failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	schedule_delayed_work(&info->cur_work, 0);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */

	return 0;
}
#endif

static const struct dev_pm_ops sy6970_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sy6970_charger_suspend,
				sy6970_charger_resume)
};

static const struct i2c_device_id sy6970_i2c_id[] = {
	{"sy6970_chg", 0},
	{}
};

static const struct of_device_id sy6970_charger_of_match[] = {
	{ .compatible = "sy,sy6970_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sy6970_charger_of_match);

static struct i2c_driver sy6970_charger_driver = {
	.driver = {
		.name = "sy6970_chg",
		.of_match_table = sy6970_charger_of_match,
		.pm = &sy6970_charger_pm_ops,
	},
	.probe = sy6970_charger_probe,
	.shutdown = sy6970_charger_shutdown,
	.remove = sy6970_charger_remove,
	.id_table = sy6970_i2c_id,
};

module_i2c_driver(sy6970_charger_driver);

MODULE_AUTHOR("Changhua Zhang <Changhua.Zhang@unisoc.com>");
MODULE_DESCRIPTION("SY6970 Charger Driver");
MODULE_LICENSE("GPL");

