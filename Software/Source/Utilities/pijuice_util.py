#!/usr/bin/env python3

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
from pprint import pprint
import json
import sys

def getDataOrError(d):
    rv = d.get('data', d['error'])
    return rv

if __name__ == '__main__':
    import pijuice
    from pijuice import pijuice_hard_functions, pijuice_sys_functions, pijuice_user_functions
    import argparse, logging

    parser = argparse.ArgumentParser(description='Command line utility for a PiJiuce')
    g = parser.add_mutually_exclusive_group()
    g.add_argument('--enable-wakeup', action='store_true', help='enable the RTC alarm wakeup flag')
    g.add_argument('--get-time', action='store_true', help='print the RTC time (UTC time)')
    g.add_argument('--get-alarm', action='store_true', help='print the RTC alarm settings')
    g.add_argument('--get-status', action='store_true', help='print the pijuice status')
    g.add_argument('--get-config', action='store_true', help='print the pijuice charging config, battery profile and firmware version')
    g.add_argument('--get-battery', action='store_true', help='print the pijuice battery status')
    g.add_argument('--get-input', action='store_true', help='print the pijuice input status')
    g.add_argument('--dump', action='store_true', help='print settings in JSON format to stdout')
    g.add_argument('--load', action='store_true', help='load settings in JSON format from stdin')

    parser.add_argument('--verbose', action='count', help='crank up logging')

    args = parser.parse_args()

    logging.basicConfig (level=logging.INFO, format='%(asctime)s %(levelname)s %(name)s %(message)s')
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # TODO does this need to be configurable
    pj = pijuice.PiJuice(1, 0x14)

    if args.enable_wakeup:
        rtc = pj.rtcAlarm
        ctr = rtc.GetControlStatus()
        logging.debug ('control status before %s', str(ctr))

        logging.debug ('disabling wakeup')
        rtc.SetWakeupEnabled(False)

        ctr = rtc.GetControlStatus()
        logging.debug ('control status after disable %s', str(ctr))

        logging.debug ('enabling wakeup')
        rtc.SetWakeupEnabled(True)
        logging.debug ('wakeup enabled')

        ctr = getDataOrError(rtc.GetControlStatus())
        print(ctr)

    if args.dump or args.load:
        RUN_PIN_VALUES = pj.config.runPinConfigs
        EEPROM_ADDRESSES = pj.config.idEepromAddresses
        INPUTS_PRECEDENCE = pj.config.powerInputs
        USB_CURRENT_LIMITS = pj.config.usbMicroCurrentLimits
        USB_MICRO_IN_DPMS = pj.config.usbMicroDPMs
        POWER_REGULATOR_MODES = pj.config.powerRegulatorModes

        config = {}
        config['general'] = {}
        config['general']['run_pin'] = RUN_PIN_VALUES.index(pj.config.GetRunPinConfig().get('data'))
        config['general']['i2c_addr'] = pj.config.GetAddress(1).get('data')
        config['general']['i2c_addr_rtc'] = pj.config.GetAddress(2).get('data')
        config['general']['eeprom_addr'] = EEPROM_ADDRESSES.index(pj.config.GetIdEepromAddress().get('data'))
        config['general']['eeprom_write_unprotected'] = not pj.config.GetIdEepromWriteProtect().get('data', False)

        result = pj.config.GetPowerInputsConfig()
        if result['error'] == 'NO_ERROR':
            pow_config = result['data']
            config['general']['precedence'] = INPUTS_PRECEDENCE.index(pow_config['precedence'])
            config['general']['gpio_in_enabled'] = pow_config['gpio_in_enabled']
            config['general']['usb_micro_current_limit'] = USB_CURRENT_LIMITS.index(pow_config['usb_micro_current_limit'])
            config['general']['usb_micro_dpm'] = USB_MICRO_IN_DPMS.index(pow_config['usb_micro_dpm'])
            config['general']['no_battery_turn_on'] = pow_config['no_battery_turn_on']

        config['general']['power_reg_mode'] = POWER_REGULATOR_MODES.index(pj.config.GetPowerRegulatorMode().get('data'))
        config['general']['charging_enabled'] = pj.config.GetChargingConfig().get('data', {}).get('charging_enabled')

        LED_FUNCTIONS_OPTIONS = pj.config.ledFunctionsOptions
        LED_NAMES = pj.config.leds

        config['led'] = []

        for i in range(len(LED_NAMES)):
            result = pj.config.GetLedConfiguration(LED_NAMES[i])
            led_config = {}
            try:
                led_config['function'] = result['data']['function']
            except ValueError:
                led_config['function'] = LED_FUNCTIONS_OPTIONS[0]
            led_config['color'] = [result['data']['parameter']['r'], result['data']['parameter']['g'], result['data']['parameter']['b']]
            config['led'].append(led_config)

        BUTTONS = pj.config.buttons

        config['button'] = {}

        got_error = False
        for button in BUTTONS:
            button_config = pj.config.GetButtonConfiguration(button)
            config['button'][button] = button_config.get('data')

    if args.dump:
        if args.verbose:
            pprint(config)
        print(json.dumps(config))

    if args.load:
        encoded_settings = ''
        for line in sys.stdin:
            encoded_settings = encoded_settings + line.rstrip()
        ns = json.loads(encoded_settings)

        # Address has to be set first
        for i, addr in enumerate(['i2c_addr', 'i2c_addr_rtc']):
            if config['general'][addr] != ns['general'][addr]:
                value = config['general'][addr]
                try:
                    new_value = int(str(ns['general'][addr]), 16)
                    if new_value >= 8 and new_value <= 0x77:
                        value = ns['general'][addr]
                except:
                    pass
                pj.config.SetAddress(i + 1, value)

        if config['general']['run_pin'] != ns['general']['run_pin']:
            pj.config.SetRunPinConfig(RUN_PIN_VALUES[ns['general']['run_pin']])
        if config['general']['eeprom_addr'] != ns['general']['eeprom_addr']:
            pj.config.SetIdEepromAddress(EEPROM_ADDRESSES[ns['general']['eeprom_addr']])
        power_config = {
            'precedence': INPUTS_PRECEDENCE[ns['general']['precedence']],
            'gpio_in_enabled': ns['general']['gpio_in_enabled'],
            'no_battery_turn_on': ns['general']['no_battery_turn_on'],
            'usb_micro_current_limit': USB_CURRENT_LIMITS[ns['general']['usb_micro_current_limit']],
            'usb_micro_dpm': USB_MICRO_IN_DPMS[ns['general']['usb_micro_dpm']],
        }
        pj.config.SetPowerInputsConfig(power_config, True);
        if config['general']['power_reg_mode'] != ns['general']['power_reg_mode']:
            pj.config.SetPowerRegulatorMode(POWER_REGULATOR_MODES[ns['general']['power_reg_mode']])
        if config['general']['charging_enabled'] != ns['general']['charging_enabled']:
            pj.config.SetChargingConfig({'charging_enabled': ns['general']['charging_enabled']}, True)

        for i in range(len(LED_NAMES)):
            led_config = {
                'function': ns['led'][i]['function'],
                'parameter': {
                    'r': ns['led'][i]['color'][0],
                    'g': ns['led'][i]['color'][1],
                    'b': ns['led'][i]['color'][2],
                }
            }
            pj.config.SetLedConfiguration(LED_NAMES[i], led_config)

        for button in BUTTONS:
            pj.config.SetButtonConfiguration(button, ns['button'][button])

    # primitives

    if args.get_status:
        print(pj.status.GetStatus())

    if args.get_time:
        rtc = pj.rtcAlarm
        print(getDataOrError(rtc.GetTime()))

    # composites

    if args.get_config:
        rv = {}
        config = pj.config
        # TODO either clean this up (making it like get_battery), else break it into several actions
        rv['chargingConfig'] = getDataOrError(config.GetChargingConfig())
        rv['batteryProfile'] = getDataOrError(config.GetBatteryProfile())
        rv['firmwareVersion'] = getDataOrError(config.GetFirmwareVersion())
        print('ChargingConfig: ', rv['chargingConfig'])
        print('Battery Profile: ', rv['batteryProfile'])
        print('Firmware Version: ', rv['firmwareVersion'])

    if args.get_alarm:
        rtc = pj.rtcAlarm
        rv = {}
        rv['alarm'] = getDataOrError(rtc.GetAlarm())
        rv['controlStatus'] = getDataOrError(rtc.GetControlStatus())
        print('Alarm: ', rv['alarm'])
        print('Controlstatus: ', rv['controlStatus'])

    if args.get_battery:
        v = {}
        status = pj.status
        v['batteryCurrent'] = getDataOrError(status.GetBatteryCurrent())
        v['batteryVoltage'] = getDataOrError(status.GetBatteryVoltage())
        v['chargeLevel'] = getDataOrError(status.GetChargeLevel())
        s = status.GetStatus().get('data',{ 'error': 'NO-STATUS-AVAILABLE'})
        v['batteryStatus']  = s.get('battery', 'BATTERY_STATUS-NOT-IN-STATUS')
        print(v)

    if args.get_input:
        v = {}
        status = pj.status

        # TODO should we camelCase here
        v['ioVoltage'] = getDataOrError(status.GetIoVoltage())
        v['ioCurrent'] = getDataOrError(status.GetIoCurrent())
        s = status.GetStatus().get('data',{ 'error': 'NO-STATUS-AVAILABLE'})
        # TODO is 'gpioPowerStatus' name good, or should it be powerInput5vIo?
        v['gpioPowerStatus'] = s.get('powerInput5vIo', 'GPIO_POWER_STATUS-NOT-IN-STATUS')
        v['usbPowerInput']  = s.get('powerInput', 'POWERINPUT-NOT-IN-STATUS')
        print(v)

