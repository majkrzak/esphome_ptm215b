import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker, binary_sensor
from esphome.const import CONF_MAC_ADDRESS, CONF_ID, CONF_KEY

CODEOWNERS = ["@majkrzak"]
DEPENDENCIES = ["esp32_ble_tracker"]

ptm215b_ns = cg.esphome_ns.namespace("ptm215b")
PTM215B = ptm215b_ns.class_(
    "PTM215B", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONF_SECURITY_KEY = "security_key"


def security_key(value):
    value = cv.string_strict(value)
    parts = value.split(":")
    if len(parts) != 16:
        raise cv.Invalid("Security Key must consist of 16 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise cv.Invalid(
            "Security Key must be format XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX"
        )
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            # pylint: disable=raise-missing-from
            raise cv.Invalid(
                "Security Key parts must be hexadecimal values from 00 to FF"
            )
    return parts_int


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PTM215B),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SECURITY_KEY): security_key,
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

    cg.add(
        var.set_address(
            cg.MockObj("{" + ",".join(map(str, config[CONF_MAC_ADDRESS].parts)) + "}")
        )
    )

    if CONF_KEY in config:
        cg.add(
            var.set_key(
                cg.MockObj(
                    "{" + ",".join(map(str, config[CONF_KEY].to_bytes(16, "big"))) + "}"
                )
            )
        )

    for key, value in mapping.items():
        if key in config:
            sensor = await binary_sensor.new_binary_sensor(config[key])
            cg.add(value(sensor))
