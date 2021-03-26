#ifndef CHARGER_CONF_H_
#define CHARGER_CONF_H_

#define CHARGER_REGISTER_COUNT			8u
#define CHARGER_LAST_REGISTER			(CHARGER_REGISTER_COUNT - 1u)

#define CHG_REG_SUPPLY_STATUS			0u
#define CHG_REG_BATTERY_STATUS			1u
#define CHG_REG_CONTROL					2u
#define CHG_REG_CONTROL_BATTERY			3u
#define CHG_REG_VENDOR					4u
#define CHG_REG_TERMI_FASTCHARGEI		5u
#define CHG_REG_DPPM_STATUS				6u
#define CHG_REG_SAFETY_NTC				7u

/* Bit definitions for Status/Control Register */
#define CHGR_SC_TMR_RST_bm				(1u << 7u)
#define CHGR_SC_STAT_Pos				(4u)
#define CHGR_SC_STAT_Msk				(7u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_NO_SOURCE			(0u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_IN_READY			(1u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_USB_READY			(2u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_IN_CHARGING		(3u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_USB_CHARGING		(4u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_CHARGE_DONE		(5u << CHGREG_SC_STAT_Pos)
#define CHGR_SC_STAT_FAULT				(7u << CHGREG_SC_STAT_Pos)
#define CHGR_SUPPLY_SEL_bm				(1u << 3u)
#define CHGR_SC_FLT_Msk					(7u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_Pos					(0u)
#define CHGR_SC_FLT_NONE				(0u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_THERMAL_SHUTDOWN	(1u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_BATTERY_TEMP		(2u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_WATCHDOGT_EXPIRED	(3u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_SAFETYT_EXPIRED		(4u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_IN_SUPPLY			(5u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_USB_SUPPLY			(6u << CHGREG_SC_FLT_Pos)
#define CHGR_SC_FLT_BATTERY				(7u << CHGREG_SC_FLT_Pos)

#define CHGR_SC_FLT_SUPPLY_PREF_USB		(CHGR_SUPPLY_SEL_bm)

/* Bit definitions for Battery/Supply Status Register */
#define CHGR_BS_INSTAT_Pos				(6u)
#define CHGR_BS_INSTAT_Msk				(3u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_INSTAT_NORMAL			(0u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_INSTAT_OVP				(1u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_INSTAT_WEAK				(2u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_INSTAT_UVP				(3u << CHGR_BS_INSTAT_Pos)

#define CHGR_BS_USBSTAT_Pos				(4u)
#define CHGR_BS_USBSTAT_Msk				(3u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_USBSTAT_NORMAL			(0u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_USBSTAT_OVP				(1u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_USBSTAT_WEAK			(2u << CHGR_BS_INSTAT_Pos)
#define CHGR_BS_USBSTAT_UVP				(3u << CHGR_BS_INSTAT_Pos)

#define CHGR_BS_OTG_LOCK_bm				(1u << 3u)
#define CHGR_BS_BATSTAT_Pos				(1u)
#define CHGR_BS_BATSTAT_Msk				(3u << CHGR_BS_BATSTAT_Pos)
#define CHGR_BS_BATSTAT_NORMAL			(0u << CHGR_BS_BATSTAT_Pos)
#define CHGR_BS_BATSTAT_OVP				(1u << CHGR_BS_BATSTAT_Pos)
#define CHGR_BS_BATSTAT_NOT_PRESENT		(2u << CHGR_BS_BATSTAT_Pos)

#define CHGR_BS_EN_NOBAT_OP				(1u)


/* Bit definitions for Control Register */
#define CHGR_CTRL_RESET					(1u << 7u)
#define CHGR_CTRL_IUSB_LIMIT_Pos		(6u)
#define CHGR_CTRL_IUSB_LIMIT_Msk		(7u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_IUSB_LIMIT_100MA		(0u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_IUSB_LIMIT_150MA		(1u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_IUSB_LIMIT_500MA		(2u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_IUSB_LIMIT_800MA		(3u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_IUSB_LIMIT_900MA		(4u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_IUSB_LIMIT_1500MA		(5u << CHGR_CTRL_IUSB_LIMIT_Pos)
#define CHGR_CTRL_EN_STAT				(1u << 3u)
#define CHGR_CTRL_TE					(1u << 2u)
#define CHGR_CTRL_CHG_DISABLE			(1u << 1u)
#define CHGR_CTRL_HZ_MODE				(1u << 0u)


#define CHGR_ST_NTC_2XTMR_EN_bm			(1u << 7u)
#define CHGR_ST_NTC_TMR_Pos				(5u)
#define CHGR_ST_NTC_TMR_Msk				(3u << CHGR_ST_NTC_TMR_Pos)
#define CHGR_ST_NTC_TS_EN_bm			(1u << 3u)
#define CHGR_ST_NTC_FAULT_Pos			(1u)
#define CHGR_ST_NTC_FAULT_Msk			(3u << CHGR_ST_NTC_FAULT_Pos)
#define CHGR_ST_NTC_LOW_CHARGE_bm		(1u)


#define CHGR_ST_NTC_SFTMR_DISABLE		(CHGR_ST_NTC_TMR_Msk)
#define CHGR_ST_NTC_SFTMR_9HOUR			(2u << CHGR_ST_NTC_TMR_Pos)
#define CHGR_ST_NTC_SFTMR_6HOUR			(1u << CHGR_ST_NTC_TMR_Pos)
#define CHGR_ST_NTC_SFTMR_27MIN			(0u << CHGR_ST_NTC_TMR_Pos)

#define CHGR_ST_NTC_TS_FAULT_NONE		(0u << CHGR_ST_NTC_FAULT_Pos)
#define CHGR_ST_NTC_TS_FAULT_SUSPEND	(1u << CHGR_ST_NTC_FAULT_Pos)
#define CHGR_ST_NTC_TS_FAULT_COLD		(2u << CHGR_ST_NTC_FAULT_Pos)
#define CHGR_ST_NTC_TS_FAULT_WARM		(3u << CHGR_ST_NTC_FAULT_Pos)

#endif /* CHARGER_CONF_H_ */
