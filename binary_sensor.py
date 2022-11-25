import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker, binary_sensor
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_ID,
)

CODEOWNERS = ["@majkrzak"]
DEPENDENCIES = ["esp32_ble_tracker"]

ptm215b_ns = cg.esphome_ns.namespace("ptm215b")
PTM215B = ptm215b_ns.class_(
    "PTM215B", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PTM215B),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional("bar"): binary_sensor.binary_sensor_schema(),
            cv.Optional("a0"): binary_sensor.binary_sensor_schema(),
            cv.Optional("a1"): binary_sensor.binary_sensor_schema(),
            cv.Optional("b0"): binary_sensor.binary_sensor_schema(),
            cv.Optional("b1"): binary_sensor.binary_sensor_schema(),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    mapping = {
        "bar": var.set_bar_sensor,
        "a0": var.set_a0_sensor,
        "a1": var.set_a1_sensor,
        "b0": var.set_b0_sensor,
        "b1": var.set_b1_sensor,
    }

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    for key, value in mapping.items():
        if key in config:
            sensor = await binary_sensor.new_binary_sensor(config[key])
            cg.add(value(sensor))
