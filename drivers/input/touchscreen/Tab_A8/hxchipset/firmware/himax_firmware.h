/*
 *
 * Himax TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/************************************************************************
*
* File Name: himax_firmware.h
*
* Author: gaozhengwei@huaqin.com
*
* Created: 2021-10-02
*
* Abstract: global configurations
*
* Version: v1.0
*
************************************************************************/


#ifndef __HIMAX_FIRMWARE_H
#define __HIMAX_FIRMWARE_H

static const uint8_t CTPM_FW_INX[] = {
    #include "INX_10.36_TESTING_ONLY_CID2F04_D00_C00_0728-015327.i"
};

static const uint8_t CTPM_FW_XY_HSD[] = {
    #include "A8_HQ_SEC_LV_XingYuan_CID420C_D00_C0E_20211115.i"
};

static const uint8_t CTPM_FW_LS_HSD[] = {
    #include "A8_HQ_SEC_LV_LCE_CID4307_D01_C09_20211115.i"
};

static const uint8_t CTPM_FW_XY_INX[] = {
    #include "A8_HQ_SEC_LV_XingY_INX_CID4503_D02_C08_20211116.i"
};

#endif
