import asyncio
import grp
import importlib.resources
import json
import logging
import os
import pathlib
import pwd
import signal
import stat
import subprocess
import sys

import marshmallow

from pijuice import PiJuice
from pijuice_sys.schema import ConfigSchema
from pijuice_sys.utils import pidfile


logger = logging.getLogger(__name__)


class PiJuiceSysInterfaceError(Exception):
    pass


class PiJuiceSysConfigValidationError(Exception):
    pass


class PiJuiceSys:
    def __init__(self, i2c_bus, i2c_address, config_file_path):
        try:
            self.pijuice = PiJuice(bus=i2c_bus, address=i2c_address)
        except:
            raise PiJuiceSysInterfaceError()
        self.config_file_path = config_file_path
        self._daemon_task = None
        self.config = {}
        self._button_config = {}

    async def load_config(self):
        logger.debug(f"loading config: {self.config_file_path}")
        if self.config_file_path:
            try:
                with open(self.config_file_path, "r") as config_file:
                    config_data = json.load(config_file)
                self.config = ConfigSchema().load(config_data)
                logger.debug(self.config)
            except json.JSONDecodeError as e:
                raise PiJuiceSysConfigValidationError("failed to decode JSON")
            except marshmallow.ValidationError as e:
                raise PiJuiceSysConfigValidationError(e)

    async def load_button_config(self):
        logger.debug("loading button config")
        for button in self.pijuice.config.buttons:
            button_config = self.pijuice.config.GetButtonConfiguration(button)
            if button_config["error"] == "NO_ERROR":
                self._button_config[button] = button_config["data"]

    async def configure(self):
        logger.debug("configuring")
        watchdog_config = self.config["system_task"]["watchdog"]
        await self.configure_watchdog(
            enabled=watchdog_config["enabled"],
            period=watchdog_config.get("period", 0),
        )
        wakeup_on_charge_config = self.config["system_task"]["wakeup_on_charge"]
        await self.configure_wakeup_on_charge(
            enabled=wakeup_on_charge_config["enabled"],
            trigger_level=wakeup_on_charge_config.get("trigger_level", 0),
        )

    async def reconfigure(self, fail_on_error=False):
        logger.debug("reconfiguring")
        try:
            await self.load_config()
            await self.load_button_config()
            await self.configure()
        except PiJuiceSysConfigValidationError as e:
            if fail_on_error:
                logger.error(
                    f"config validation failed! exiting... validation error: {e}"
                )
                self._daemon_task.cancel()
                sys.exit(1)
            else:
                logger.warning(
                    f"config validation failed! ignoring... validation error: {e}"
                )

    async def configure_watchdog(self, enabled, period):
        logger.debug(f"configuring watchdog - enabled: {enabled} period: {period}")
        if not enabled:
            period = 0
        result = self.pijuice.power.SetWatchdog(period)
        if result["error"] != "NO_ERROR":
            await asyncio.sleep(0.05)
            self.pijuice.power.SetWatchdog(period)

    async def configure_wakeup_on_charge(self, enabled, trigger_level):
        logger.debug(
            "configuring wakeup-on-charge"
            f" - enabled: {enabled} trigger_level: {trigger_level}"
        )
        if enabled:
            self.pijuice.power.SetWakeUpOnCharge(trigger_level)
        else:
            self.pijuice.power.SetWakeUpOnCharge("DISABLED")

    async def _execute_event_function(
        self, event, function, param, fallback_to_daemon_user=True
    ):
        def demote(uid, gid):
            def result():
                logger.debug(f"Demoting to {uid}:{gid}")
                os.setgid(gid)
                os.setuid(uid)

            return result

        script_path = None
        execute_uid = None
        execute_gid = None
        logger.info(
            f"executing event function - event: {event} function: {function} param: {param}"
        )
        # resolve function to file
        user_function = self.config.get("user_functions", {}).get(function)
        if user_function:
            script_path = pathlib.Path(user_function)
        elif function.startswith("SYS_FUNC"):
            script_resource_path = importlib.resources.path(
                "pijuice_sys.scripts", f"{function}.sh"
            )
            with script_resource_path as p:
                script_path = p
        if not script_path:
            logger.warning(f"failed to resolve {function}-script")
            return
        logger.debug(f"resolved {function}-script to: {script_path}")
        # check if file exists
        if not script_path.is_file():
            logger.warning(f"file not found: {script_path}")
            return
        # check if script is owner-executable
        script_stat = script_path.stat()
        if script_stat.st_mode & stat.S_IXUSR == 0:
            logger.warning(f"file not executable: {script_path}")
            return
        # determine script-owner uid and gid
        script_user_id = script_stat.st_uid
        script_user_name = pwd.getpwuid(script_user_id).pw_name
        script_group_id = script_stat.st_gid
        # determine script-owner groups
        script_user_group_ids = [
            g.gr_gid for g in grp.getgrall() if script_user_name in g.gr_mem
        ]
        # determine daemon uid and gid
        daemon_user_id = os.geteuid()
        daemon_user_name = pwd.getpwuid(daemon_user_id).pw_name
        daemon_group_id = os.getegid()
        daemon_group_name = grp.getgrgid(daemon_group_id).gr_name
        # check if owned by root
        if script_user_name == "root":
            logger.warning(f"file owned by root. this is not allowed: {script_path}")
        elif (
            script_user_id == daemon_user_id
            or script_group_id == daemon_group_id
            or daemon_group_id in script_user_group_ids
        ):
            execute_uid = script_user_id
            execute_gid = script_group_id
        else:
            logger.warning(
                f"file is neither owned by '{daemon_user_name}'"
                f"nor is the owner '{script_user_name}' member of '{daemon_group_name}'-group"
            )
        if execute_uid is None and fallback_to_daemon_user:
            logger.warning(f"falling back to daemon-user: {daemon_user_name}")
            execute_uid = daemon_user_id
            execute_gid = daemon_group_id
        elif execute_uid is None:
            logger.warning("failed to resolve user to execute script")
            return
        subprocess.Popen(
            args=[str(script_path), str(event), str(param)],
            preexec_fn=demote(uid=execute_uid, gid=execute_gid),
        )

    async def _handle_system_event(self, event, param=None):
        logger.debug(f"handling event: {event} - param: {param}")
        if self.config["system_events"][event]["enabled"]:
            function = self.config["system_events"][event]["function"]
            logger.debug(f"resolved function: {function}")
            if function == "NO_FUNC":
                return
            await self._execute_event_function(event, function, param)

    async def _button_events_daemon(self, poll_interval):
        while True:
            await asyncio.sleep(poll_interval)
            result = self.pijuice.status.GetButtonEvents()
            logger.debug(f"button-events: {result}")
            if result["error"] != "NO_ERROR":
                continue
            button_events = result["data"]
            for button in self.pijuice.config.buttons:
                event = button_events[button]
                if event == "NO_EVENT":
                    continue
                function = self._button_config[button][event]["function"]
                if function == "USER_EVENT":
                    continue
                self.pijuice.status.AcceptButtonEvent(button)
                await self._execute_event_function(event, function, param=button)

    async def _charge_level_daemon(self, poll_interval):
        last_charge_level = None
        while True:
            await asyncio.sleep(poll_interval)
            if not self.config["system_task"]["min_charge"]["enabled"]:
                continue
            result = self.pijuice.status.GetChargeLevel()
            logger.debug(f"charge-level: {result}")
            if result["error"] != "NO_ERROR":
                continue
            charge_level = float(result["data"])
            logger.info(f"charge-level: {charge_level}%")
            threshold = self.config["system_task"]["min_charge"]["threshold"]
            if (
                charge_level == 0
                or (charge_level < threshold)
                and (last_charge_level is not None)
                and (0 <= (last_charge_level - charge_level) < 3)
            ):
                logger.info(f"charge-level hit threshold of {threshold}%")
                await self._handle_system_event(event="low_charge", param=charge_level)
            last_charge_level = charge_level

    async def _battery_voltage_daemon(self, poll_interval):
        while True:
            await asyncio.sleep(poll_interval)
            if not self.config["system_task"]["min_bat_voltage"]["enabled"]:
                continue
            result = self.pijuice.status.GetBatteryVoltage()
            logger.debug(f"battery-voltage: {result}")
            if result["error"] != "NO_ERROR":
                continue
            battery_voltage = float(result["data"]) / 1000
            logger.info(f"battery-voltage: {battery_voltage}V")
            threshold = self.config["system_task"]["min_bat_voltage"]["threshold"]
            if battery_voltage < threshold:
                logger.info(f"battery-voltage hit threshold of {threshold}V")
                await self._handle_system_event(
                    event="low_battery_voltage", param=battery_voltage
                )

    async def _power_inputs_daemon(self, poll_interval):
        NO_POWER_STATUSES = ["NOT_PRESENT", "BAD"]
        NO_POWER_THRESHOLD = 2
        no_power_count = 0
        while True:
            await asyncio.sleep(poll_interval)
            if not self.config["system_events"]["no_power"]["enabled"]:
                continue
            result = self.pijuice.status.GetStatus()
            logger.debug(f"power-inputs: {result}")
            if result["error"] != "NO_ERROR":
                continue
            status = result["data"]
            power_usb = status["powerInput"]
            power_io = status["powerInput5vIo"]
            logger.info(f"power_input - usb: {power_usb} io: {power_io}")
            if power_usb in NO_POWER_STATUSES and power_io in NO_POWER_STATUSES:
                no_power_count += 1
            else:
                no_power_count = 0
            if no_power_count >= NO_POWER_THRESHOLD:
                await self._handle_system_event("no_power")

    async def _fault_flags_daemon(self, poll_interval):
        while True:
            await asyncio.sleep(poll_interval)
            result = self.pijuice.status.GetFaultStatus()
            logger.debug(f"fault-flags: {result}")
            if result["error"] != "NO_ERROR":
                continue
            faults = result["data"]
            for fault in self.pijuice.status.faultEvents + self.pijuice.status.faults:
                if (
                    fault in faults
                    and self.config["system_events"][fault]["enabled"]
                    and self.config["system_events"][fault]["function"] != "USER_EVENT"
                ):
                    self.pijuice.status.ResetFaultFlags([fault])
                    await self._handle_system_event(event=fault, param=faults[fault])

    async def _run_daemon(self, poll_interval, button_poll_interval):
        logger.debug("installing SIGHUP handler")
        loop = asyncio.get_event_loop()
        loop.add_signal_handler(
            signal.SIGHUP, lambda: asyncio.create_task(pijuice_sys.reconfigure())
        )
        await self.reconfigure(fail_on_error=True)
        logger.debug("running daemons")
        await asyncio.gather(
            self._button_events_daemon(button_poll_interval),
            self._charge_level_daemon(poll_interval),
            self._battery_voltage_daemon(poll_interval),
            self._power_inputs_daemon(poll_interval),
            self._fault_flags_daemon(poll_interval),
        )

    async def run_daemon(self, pid_file_path, poll_interval, button_poll_interval):
        with pidfile(pid_file_path):
            self._daemon_task = asyncio.create_task(
                self._run_daemon(
                    poll_interval=poll_interval,
                    button_poll_interval=button_poll_interval,
                )
            )
            try:
                logger.debug("running daemon_task")
                await self._daemon_task
            except asyncio.CancelledError:
                pass

    async def execute_poweroff_command(self):
        logger.debug("executing poweroff command")
        await self.load_config()
        await self.configure_watchdog(enabled=False, period=0)
        if self.config["system_task"]["ext_halt_power_off"]["enabled"]:
            delay = self.config["system_task"]["ext_halt_power_off"]["period"]
            logger.debug(f"setting poweroff - delay: {delay}")
            self.pijuice.power.SetPowerOff(delay=delay)
