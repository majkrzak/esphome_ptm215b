import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import Lambda
from esphome.components import esp32_ble_tracker, binary_sensor
from esphome.const import CONF_MAC_ADDRESS, CONF_ID
from .config_validation import security_key
from .const import CONF_SECURITY_KEY

CODEOWNERS = ["@majkrzak"]
DEPENDENCIES = ["esp32_ble_tracker"]

ptm215b_ns = cg.esphome_ns.namespace("ptm215b")
PTM215B = ptm215b_ns.class_(
    "PTM215B", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)


BUTTONS = {
    "any": (None, None, None, None),
    "a0": (True, None, None, None),
    "a1": (None, True, None, None),
    "b0": (None, None, True, None),
    "b1": (None, None, None, True),
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PTM215B),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SECURITY_KEY): security_key,
            **{
                cv.Optional(button): binary_sensor.binary_sensor_schema()
                for button in BUTTONS
            },
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].parts))

    if CONF_SECURITY_KEY in config:
        cg.add(var.set_key(config[CONF_SECURITY_KEY]))

    for key, value in BUTTONS.items():
        if key in config:
            cg.add(
                var.set_button(
                    [
                        cg.optional.template(cg.bool_)(
                            switch
                            if switch is not None
                            else cg.global_ns.namespace("nullopt")
                        )
                        for switch in value
                    ],
                    await binary_sensor.new_binary_sensor(config[key]),
                )
            )
