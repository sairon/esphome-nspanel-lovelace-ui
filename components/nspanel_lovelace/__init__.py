from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@joBr99"]
DEPENDENCIES = ["uart", "wifi", "esp32"]

CONF_ENABLE_UPLOAD = "enable_upload"

nspanel_lovelace_ns = cg.esphome_ns.namespace("nspanel_lovelace")
NSPanelLovelace = nspanel_lovelace_ns.class_("NSPanelLovelace", cg.Component, uart.UARTDevice)


NSPanelLovelaceMsgIncomingTrigger = nspanel_lovelace_ns.class_(
    "NSPanelLovelaceMsgIncomingTrigger",
    automation.Trigger.template(cg.std_string)
)

CONF_INCOMING_MSG = 'on_incoming_msg'


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NSPanelLovelace),
            cv.Optional(CONF_ENABLE_UPLOAD, default=False): cv.All(cv.boolean, cv.only_with_arduino),
            cv.Required(CONF_INCOMING_MSG): automation.validate_automation(
                cv.Schema(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NSPanelLovelaceMsgIncomingTrigger),
                    }
                )
            ),
        }
    )
        .extend(uart.UART_DEVICE_SCHEMA)
        .extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
        )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for conf in config.get(CONF_INCOMING_MSG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CONF_ENABLE_UPLOAD in config:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)
        cg.add_define("USE_NSPANEL_LOVELACE_UPLOAD")

    cg.add_define("USE_NSPANEL_LOVELACE")
