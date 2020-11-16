#!/usr/bin/env python3
import argparse
import os
import sys

from pijuice import PiJuice


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="PiJuice Cmd")
    subparsers = parser.add_subparsers(
        help="sub-command help", dest="command", required=True
    )

    parser.add_argument(
        "-B",
        "--i2c-bus",
        metavar="BUS",
        type=int,
        default=os.environ.get("PIJUICE_I2C_BUS", 1),
        help="I2C Bus (default: %(default)s)",
    )
    parser.add_argument(
        "-A",
        "--i2c-address",
        metavar="ADDRESS",
        type=int,
        default=os.environ.get("PIJUICE_I2C_ADDRESS", 0x14),
        help="I2C Address (default: %(default)s)",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="Disable output",
    )

    parser_wakeup_on_charge = subparsers.add_parser(
        "wakeup-on-charge", help="configure wakeup-on-charge"
    )
    parser_wakeup_on_charge.add_argument(
        "-D",
        "--disabled",
        action="store_true",
        help="Whether to disable wakeup-on-charge",
    )
    parser_wakeup_on_charge.add_argument(
        "-T",
        "--trigger-level",
        metavar="TRIGGER_LEVEL",
        type=int,
        default=100,
        help="Battery-charge to wakeup at (default: %(default)s)",
    )

    parser_system_power_switch = subparsers.add_parser(
        "system-power-switch", help="configure system-power-switch"
    )
    parser_system_power_switch.add_argument(
        "state",
        type=int,
        choices=[0, 500, 2100],
        help="Current limit for VSYS pin (in mA)",
    )

    def validate_power_off_delay(value):
        try:
            value = int(value)
        except ValueError:
            raise argparse.ArgumentTypeError(f"invalid int value: '{value}'")
        if not 0 <= value <= 65535:
            raise argparse.ArgumentTypeError(f"not within range: {value}")
        return value

    parser_power_off = subparsers.add_parser("power-off", help="configure power-off")
    parser_power_off.add_argument(
        "-D",
        "--delay",
        metavar="DELAY",
        type=validate_power_off_delay,
        default=0,
        help="Delay before powering off (0 to 65535) (default: %(default)s)",
    )

    def validate_led_blink_count(value):
        try:
            value = int(value)
        except ValueError:
            raise argparse.ArgumentTypeError(f"invalid int value: '{value}'")
        if not 0 < value < 255:
            raise argparse.ArgumentTypeError(f"not within range: {value}")
        return value

    def validate_led_blink_rgb(value):
        values = value.split(",")
        if not len(values) == 3:
            raise argparse.ArgumentTypeError(f"value not in format 'R,G,B': '{value}'")
        try:
            values = [int(v) for v in values]
        except ValueError:
            raise argparse.ArgumentTypeError(f"invalid int values: '{value}'")
        return values

    def validate_led_blink_period(value):
        try:
            value = int(value)
        except ValueError:
            raise argparse.ArgumentTypeError(f"invalid int value: '{value}'")
        if not 10 <= value < 2550:
            raise argparse.ArgumentTypeError(f"not within range: {value}")
        return value

    parser_led_blink = subparsers.add_parser("led-blink", help="run led-blink pattern")
    parser_led_blink.add_argument(
        "-L",
        "--led",
        metavar="LED",
        type=str,
        choices=["D1", "D2"],
        default="D2",
        help="Led to blink (default: %(default)s)",
    )
    parser_led_blink.add_argument(
        "-C",
        "--count",
        metavar="COUNT",
        type=validate_led_blink_count,
        default=1,
        help="Amount of repetitions (default: %(default)s)",
    )
    parser_led_blink.add_argument(
        "--rgb1",
        metavar="R,G,B",
        type=validate_led_blink_rgb,
        default="150,0,0",
        help="RGB components of first blink (default: %(default)s)",
    )
    parser_led_blink.add_argument(
        "--period1",
        metavar="PERIOD",
        type=validate_led_blink_period,
        default=200,
        help="Duration of first blink in milliseconds (default: %(default)s)",
    )
    parser_led_blink.add_argument(
        "--rgb2",
        metavar="R,G,B",
        type=validate_led_blink_rgb,
        default="0,100,0",
        help="RGB components of second blink (default: %(default)s)",
    )
    parser_led_blink.add_argument(
        "--period2",
        metavar="PERIOD",
        type=validate_led_blink_period,
        default=200,
        help="Duration of second blink in milliseconds (default: %(default)s)",
    )

    return parser.parse_args()


def main():
    args = parse_args()
    try:
        pijuice = PiJuice(
            bus=args.i2c_bus,
            address=args.i2c_address,
        )
    except:
        print("Failed to connect to PiJuice")
        sys.exit(1)

    if args.command == "wakeup-on-charge":
        if args.disabled:
            pijuice.power.SetWakeUpOnCharge(arg="DISABLED")
            if not args.quiet:
                print("Disabled wakeup-on-charge")
        else:
            pijuice.power.SetWakeUpOnCharge(arg=args.trigger_level)
            if not args.quiet:
                print(
                    f"Enabled wakeup-on-charge with trigger-level: {args.trigger_level}"
                )
    elif args.command == "system-power-switch":
        pijuice.power.SetSystemPowerSwitch(state=args.state)
        if not args.quiet:
            print(f"Set System Power-Switch to: {args.state}mA")
    elif args.command == "power-off":
        pijuice.power.SetPowerOff(delay=args.delay)
        if not args.quiet:
            print(f"Set PowerOff with delay: {args.delay}s")
    elif args.command == "led-blink":
        pijuice.status.SetLedBlink(
            led=args.led,
            count=args.count,
            rgb1=args.rgb1,
            period1=args.period1,
            rgb2=args.rgb2,
            period2=args.period2,
        )


if __name__ == "__main__":
    main()
