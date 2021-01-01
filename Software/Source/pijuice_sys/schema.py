from marshmallow import (
    Schema,
    ValidationError,
    fields,
    validate,
    validates_schema,
)


class SystemTaskWatchdogSchema(Schema):
    enabled = fields.Boolean(missing=False)
    period = fields.Int(validate=validate.Range(min=1, max=65535))

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "period" in data:
            raise ValidationError("period must be provided")


class SystemTaskMinBatVoltageSchema(Schema):
    enabled = fields.Boolean(missing=False)
    threshold = fields.Int(validate=validate.Range(min=0, max=10))

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "threshold" in data:
            raise ValidationError("threshold must be provided")


class SystemTaskMinChargeSchema(Schema):
    enabled = fields.Boolean(missing=False)
    threshold = fields.Int(validate=validate.Range(min=0, max=100))

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "threshold" in data:
            raise ValidationError("threshold must be provided")


class SystemTaskWakeupOnChargeSchema(Schema):
    enabled = fields.Boolean(missing=False)
    trigger_level = fields.Int(validate=validate.Range(min=0, max=100))

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "trigger_level" in data:
            raise ValidationError("trigger_level must be provided")


class SystemTaskExtHaltPowerOff(Schema):
    enabled = fields.Boolean(missing=False)
    period = fields.Int(validate=validate.Range(min=10, max=65535))

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "period" in data:
            raise ValidationError("period must be provided")


class SystemTaskSchema(Schema):
    enabled = fields.Boolean(missing=False)
    watchdog = fields.Nested(
        SystemTaskWatchdogSchema,
        missing=SystemTaskWatchdogSchema().load({}),
    )
    min_bat_voltage = fields.Nested(
        SystemTaskMinBatVoltageSchema,
        missing=SystemTaskMinBatVoltageSchema().load({}),
    )
    min_charge = fields.Nested(
        SystemTaskMinChargeSchema,
        missing=SystemTaskMinChargeSchema().load({}),
    )
    wakeup_on_charge = fields.Nested(
        SystemTaskWakeupOnChargeSchema,
        missing=SystemTaskWakeupOnChargeSchema().load({}),
    )
    ext_halt_power_off = fields.Nested(
        SystemTaskExtHaltPowerOff,
        missing=SystemTaskExtHaltPowerOff().load({}),
    )


SystemEventsFunctionField = fields.Str(
    validate=validate.OneOf(
        [
            "NO_FUNC",
            "SYS_FUNC_HALT",
            "SYS_FUNC_HALT_POW_OFF",
            "SYS_FUNC_SYS_OFF_HALT",
            "SYS_FUNC_REBOOT",
            "USER_EVENT",
            "USER_FUNC1",
            "USER_FUNC2",
            "USER_FUNC3",
            "USER_FUNC4",
            "USER_FUNC5",
            "USER_FUNC6",
            "USER_FUNC7",
            "USER_FUNC8",
            "USER_FUNC9",
            "USER_FUNC10",
            "USER_FUNC11",
            "USER_FUNC12",
            "USER_FUNC13",
            "USER_FUNC14",
            "USER_FUNC15",
        ]
    )
)

SystemEventsUserFunctionField = fields.Str(
    validate=validate.OneOf(
        [
            "NO_FUNC",
            "USER_EVENT",
            "USER_FUNC1",
            "USER_FUNC2",
            "USER_FUNC3",
            "USER_FUNC4",
            "USER_FUNC5",
            "USER_FUNC6",
            "USER_FUNC7",
            "USER_FUNC8",
            "USER_FUNC9",
            "USER_FUNC10",
            "USER_FUNC11",
            "USER_FUNC12",
            "USER_FUNC13",
            "USER_FUNC14",
            "USER_FUNC15",
        ]
    )
)


class SystemEventsFunctionSchema(Schema):
    enabled = fields.Boolean(missing=False)
    function = SystemEventsFunctionField

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "function" in data:
            raise ValidationError("function must be provided")


class SystemEventsUserFunctionSchema(Schema):
    enabled = fields.Boolean(missing=False)
    function = SystemEventsUserFunctionField

    @validates_schema
    def validate_function(self, data, **kwargs):
        if data.get("enabled", False) and not "function" in data:
            raise ValidationError("function must be provided")


class SystemEventsSchema(Schema):
    no_power = fields.Nested(
        SystemEventsFunctionSchema,
        missing=SystemEventsFunctionSchema().load({}),
    )
    low_charge = fields.Nested(
        SystemEventsFunctionSchema,
        missing=SystemEventsFunctionSchema().load({}),
    )
    low_battery_voltage = fields.Nested(
        SystemEventsFunctionSchema,
        missing=SystemEventsFunctionSchema().load({}),
    )
    button_power_off = fields.Nested(
        SystemEventsUserFunctionSchema,
        missing=SystemEventsUserFunctionSchema().load({}),
    )
    forced_power_off = fields.Nested(
        SystemEventsUserFunctionSchema,
        missing=SystemEventsUserFunctionSchema().load({}),
    )
    forced_sys_power_off = fields.Nested(
        SystemEventsUserFunctionSchema,
        missing=SystemEventsUserFunctionSchema().load({}),
    )
    watchdog_reset = fields.Nested(
        SystemEventsUserFunctionSchema,
        missing=SystemEventsUserFunctionSchema().load({}),
    )


class UserFunctionsSchema(Schema):
    USER_FUNC1 = fields.Str()
    USER_FUNC2 = fields.Str()
    USER_FUNC3 = fields.Str()
    USER_FUNC4 = fields.Str()
    USER_FUNC5 = fields.Str()
    USER_FUNC6 = fields.Str()
    USER_FUNC7 = fields.Str()
    USER_FUNC8 = fields.Str()
    USER_FUNC9 = fields.Str()
    USER_FUNC10 = fields.Str()
    USER_FUNC11 = fields.Str()
    USER_FUNC12 = fields.Str()
    USER_FUNC13 = fields.Str()
    USER_FUNC14 = fields.Str()
    USER_FUNC15 = fields.Str()
    SYS_FUNC_HALT = fields.Str()
    SYS_FUNC_HALT_POW_OFF = fields.Str()
    SYS_FUNC_SYS_OFF_HALT = fields.Str()
    SYS_FUNC_REBOOT = fields.Str()


class ConfigSchema(Schema):
    system_task = fields.Nested(
        SystemTaskSchema,
        missing=SystemTaskSchema().load({}),
    )
    system_events = fields.Nested(
        SystemEventsSchema,
        missing=SystemEventsSchema().load({}),
    )
    user_functions = fields.Nested(UserFunctionsSchema)
