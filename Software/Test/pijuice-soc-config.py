# Author: Milan Neskovic, Pi Supply, 2018

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# Usage:
# Use to write custom battery profile state of charge estimation parameters to PiJuice HAT.
# Before running this script, select/enable custom battery profile from PiJuice configuration GUI.
# Define profile parameters inside json structure and pass as parameter to SetCustomBatteryExtProfile().

from pijuice import PiJuice, PiJuiceInterface
import time

interface=PiJuiceInterface(1,0x14)
#pj=PiJuice(1,0x14)

BATTERY_EXT_PROFILE_CMD = 0x54
batteryChemisties = ['LIPO', 'LIFEPO4']
def GetBatteryExtProfile():
	ret = interface.ReadData(BATTERY_EXT_PROFILE_CMD, 17)
	if ret['error'] != 'NO_ERROR':
		return ret
	else:
		d = ret['data']
		if all(v == 0 for v in d):
			return {'data':'INVALID', 'error':'NO_ERROR'} 
		profile = {}
		if d[0] < len(batteryChemisties):
			profile['chemistry'] = batteryChemisties[d[0]]
		else:
			profile['chemistry'] = 'UNKNOWN'
		profile['ocv10'] = (d[2] << 8) | d[1]
		profile['ocv50'] = (d[4] << 8) | d[3]
		profile['ocv90'] = (d[6] << 8) | d[5]
		profile['r10'] = ((d[8] << 8) | d[7])/100.0
		profile['r50'] = ((d[10] << 8) | d[9])/100.0
		profile['r90'] = ((d[12] << 8) | d[11])/100.0
		return {'data':profile, 'error':'NO_ERROR'}

def SetCustomBatteryExtProfile(profile):
	d = [0x00] * 17
	try:
		chid = batteryChemisties.index(profile['chemistry'])
		d[0] = chid&0xFF
		v=int(profile['ocv10'])
		d[1] = v&0xFF
		d[2] = (v >> 8)&0xFF
		v=int(profile['ocv50'])
		d[3] = v&0xFF
		d[4] = (v >> 8)&0xFF
		v=int(profile['ocv90'])
		d[5] = v&0xFF
		d[6] = (v >> 8)&0xFF
		v=int(profile['r10']*100)
		d[7] = v&0xFF
		d[8] = (v >> 8)&0xFF
		v=int(profile['r50']*100)
		d[9] = v&0xFF
		d[10] = (v >> 8)&0xFF
		v=int(profile['r90']*100)
		d[11] = v&0xFF
		d[12] = (v >> 8)&0xFF
		d[13] = 0xFF
		d[14] = 0xFF
		d[15] = 0xFF
		d[16] = 0xFF
	except:
		return {'error':'BAD_ARGUMENT'}
	print(len(d))
	return interface.WriteDataVerify(BATTERY_EXT_PROFILE_CMD, d, 0.2)
	
extProfileBP7X_1820={
	'chemistry': 'LIPO',
	'ocv10': 3649, # Battery open circuit voltage when charge level is 10%.
	'ocv50': 3795, # Battery open circuit voltage when charge level is 50%, usually equal to declared nominal voltage.
	'ocv90': 4077, # Battery open circuit voltage when charge level is 90%.
	'r10': 159.0,  # Battery internal resistance when charge level is 10%.
	'r50': 156.3,  # Battery internal resistance when charge level is 50%.
	'r90': 155.5   # Battery internal resistance when charge level is 90%.
}

extProfile_SNN5843_2300={
	'chemistry': 'LIPO',
	'ocv10': 3649, 
	'ocv50': 3795,
	'ocv90': 4077, 
	'r10': 153.0, 
	'r50': 149.0,   
	'r90': 148.2
}

extProfile_18650LiFePO4_1100={
	'chemistry': 'LIFEPO4',
	'ocv10': 3131,
	'ocv50': 3263,
	'ocv90': 3303, 
	'r10': 91, 
	'r50': 83,   
	'r90': 76
}

#prf={'capacity': 1820, 'tempCold': 1, 'tempCool': 10, 'regulationVoltage': 4180, 'ntcB': 3380, 'ntcResistance': 10000, 'terminationCurrent': 50, 'tempWarm': 45, 'cutoffVoltage': 3000, 'tempHot': 59, 'chargeCurrent': 925}
#print(pj.config.SetCustomBatteryProfile(prf))
#print(pj.config.GetBatteryProfile())
#time.sleep(0.2)

print(SetCustomBatteryExtProfile(extProfileBP7X_1820))
time.sleep(0.2)
print(GetBatteryExtProfile())
