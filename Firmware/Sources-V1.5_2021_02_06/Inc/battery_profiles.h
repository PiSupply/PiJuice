/*
 * battery_profiles.h
 *
 *  Created on: 31.03.21
 *      Author: jsteggall, milan
 */

#ifndef BATTERY_PROFILES_H_
#define BATTERY_PROFILES_H_

#define BATTERY_PROFILES_COUNT		((sizeof(m_batteryProfiles)/sizeof(BatteryProfile_T)))

const BatteryProfile_T m_batteryProfiles[] =
{
	{ 	// PiJuice Zero 1000mAh battery
		BAT_CHEMISTRY_LIPO,
		1000, // 1000mAh
		0x01, // 6250mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3743, 3933, 4057,
		13500, 13300, 13300,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// BP7X battery
		BAT_CHEMISTRY_LIPO,
		1820, // 1820mAh
		0x05, // 925mA, ~0.5C
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3649, 3800, 4077,
		20900, 20500, 20200,
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// SNN5843 battery
		BAT_CHEMISTRY_LIPO,
		2300, // 2300mAh
		0x08, // 1150mA, ~0.5C
		0x01, // 100mA
		0x22, // 4.18V
		150, // 3V
		3650, 3800, 4079,
		15300, 14900, 14820,
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice 12000mAh battery
		BAT_CHEMISTRY_LIPO,
		12000, // 12000mAh
		0x1A, // 2500mA
		0x06, // 350mA
		0x22, // 4.18V
		150, // 3V
		3488, 3824, 4061,
		11200, 10800, 10800,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice 5000mAh battery
		BAT_CHEMISTRY_LIPO,
		5000, // 5000mAh
		0x1A, // 2500mA
		0x04, // 250mA
		0x22, // 4.18V
		150, // 3V
		3506, 3870, 4056,
		11100, 10500, 10700,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice BP7X 1600mAh battery
		BAT_CHEMISTRY_LIPO,
		1600, // 1600mAh
		0x05, // 925mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3672, 3811, 4094,
		22200, 20800, 20500,
		0,
		2,
		50,
		70,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice SNN5843 1300mAh battery
		BAT_CHEMISTRY_LIPO,
		1300, // 1300mAh
		0x03, // 775mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3675, 3818, 4105,
		15600, 15100, 15100,
		0,
		2,
		50,
		70,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice 1200mAh battery
		BAT_CHEMISTRY_LIPO,
		1200, // 1200mAh
		0x02, // 700mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3514, 3859, 4045,
		19400, 17300, 16900,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// BP6X battery
		BAT_CHEMISTRY_LIPO,
		1400, // 1400mAh
		0x04, // 850mA, ~0.6C
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3649, 3800, 4077,
		20670, 20319, 20215,
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice 600mAh battery
		BAT_CHEMISTRY_LIPO,
		600, // 600mAh
		0x00, // 550mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3659, 3816, 4087,
		37200, 22100, 22300,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice 500mAh battery
		BAT_CHEMISTRY_LIPO,
		500, // 500mAh
		0x00, // 550mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3659, 3914, 4060,
		16600, 15600, 15600,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice 2500mAh battery
		BAT_CHEMISTRY_LIPO,
		2500, // 2500mAh
		0x0a, // 1300mA, ~0.5C
		0x01, // 100mA
		0x22, // 4.18V
		150, // 3V
		3650, 3800, 4079,
		15300, 14900, 14820,
		1,
		10,
		45,
		59,
		3450,
		1000, // 10K
	}
};

#endif /* BATTERY_PROFILES_H_ */
