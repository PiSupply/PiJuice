#!/usr/bin/env python3
import argparse
import asyncio
import logging
import pathlib
import sys
import os

from pijuice_sys.daemon import (
    PiJuiceSys,
    PiJuiceSysInterfaceError,
    PiJuiceSysConfigValidationError,
)


logging.basicConfig(level=logging.WARN)
logger = logging.getLogger(__package__)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="PiJuice System Daemon")
    subparsers = parser.add_subparsers(
        help="sub-command help", dest="command", required=True
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Increase verbosity level",
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
        "-C",
        "--config-file",
        metavar="FILE",
        dest="config_file_path",
        type=pathlib.Path,
        default=os.environ.get(
            "PIJUICE_CONFIG_FILE", "/var/lib/pijuice/pijuice_config.JSON"
        ),
        help="Path to read Configuration file from (default: %(default)s)",
    )

    parser_daemon = subparsers.add_parser("daemon", help="run daemon")
    parser_daemon.add_argument(
        "--pid-file",
        metavar="FILE",
        dest="pid_file_path",
        type=pathlib.Path,
        default=os.environ.get("PIJUICE_PID_FILE", "/tmp/pijuice_sys.pid"),
        help="Path to create pid-file at (default: %(default)s)",
    )
    parser_daemon.add_argument(
        "--poll-interval",
        metavar="INTERVAL",
        type=float,
        default=os.environ.get("PIJUICE_POLL_INTERVAL", 5.0),
        help="Interval at which to poll status from pijuice (default: %(default)s)",
    )
    parser_daemon.add_argument(
        "--button-poll-interval",
        metavar="INTERVAL",
        type=float,
        default=os.environ.get("PIJUICE_BUTTON_POLL_INTERVAL", 1.0),
        help="Interval at which to poll button-events from pijuice (default: %(default)s)",
    )

    parser_poweroff = subparsers.add_parser("poweroff", help="execute poweroff command")
    return parser.parse_args()


async def run():
    args = parse_args()
    if args.verbose == 1:
        logger.setLevel(logging.INFO)
    elif args.verbose == 2:
        logger.setLevel(logging.DEBUG)

    try:
        pijuice_sys = PiJuiceSys(
            i2c_bus=args.i2c_bus,
            i2c_address=args.i2c_address,
            config_file_path=args.config_file_path,
        )
    except PiJuiceSysInterfaceError:
        logger.error("failed to initialize PiJuice interface")
        sys.exit(1)
    except PiJuiceSysConfigValidationError as e:
        logger.error(f"config validation failed: {e}")
        sys.exit(1)

    if args.command == "poweroff":
        await pijuice_sys.execute_poweroff_command()
    elif args.command == "daemon":
        logger.debug("starting daemon")
        await pijuice_sys.run_daemon(
            pid_file_path=args.pid_file_path,
            poll_interval=args.poll_interval,
            button_poll_interval=args.button_poll_interval,
        )


def main():
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
