from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import mqtt, uart
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE

AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@sairon"]
DEPENDENCIES = ["mqtt", "uart", "wifi", "esp32"]

nspanel_lovelace_ns = cg.esphome_ns.namespace("nspanel_lovelace")
NSPanelLovelace = nspanel_lovelace_ns.class_("NSPanelLovelace", cg.Component, uart.UARTDevice)


NSPanelLovelaceMsgIncomingTrigger = nspanel_lovelace_ns.class_(
    "NSPanelLovelaceMsgIncomingTrigger",
    automation.Trigger.template(cg.std_string)
)

CONF_MQTT_PARENT_ID = "mqtt_parent_id"
CONF_MQTT_RECV_TOPIC = "mqtt_recv_topic"
CONF_MQTT_SEND_TOPIC = "mqtt_send_topic"
CONF_INCOMING_MSG = "on_incoming_msg"
CONF_BERRY_DRIVER_VERSION = "berry_driver_version"
CONF_USE_MISSED_UPDATES_WORKAROUND = "use_missed_updates_workaround"
CONF_UPDATE_BAUD_RATE = "update_baud_rate"


def validate_config(config):
    if int(config[CONF_BERRY_DRIVER_VERSION]) > 0:
        if "CustomSend" not in config[CONF_MQTT_SEND_TOPIC]:
            # backend uses topic_send.replace("CustomSend", ...) for GetDriverVersion and FlashNextion
            raise cv.Invalid(f"{CONF_MQTT_SEND_TOPIC} must contain \"CustomSend\" for correct backend compatibility.\n"
                             f"Either change it or set {CONF_BERRY_DRIVER_VERSION} to 0.")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NSPanelLovelace),
            cv.GenerateID(CONF_MQTT_PARENT_ID): cv.use_id(mqtt.MQTTClientComponent),
            cv.Optional(CONF_MQTT_RECV_TOPIC, default="tele/nspanel/RESULT"): cv.string,
            cv.Optional(CONF_MQTT_SEND_TOPIC, default="cmnd/nspanel/CustomSend"): cv.string,
            cv.Optional(CONF_INCOMING_MSG): automation.validate_automation(
                cv.Schema(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NSPanelLovelaceMsgIncomingTrigger),
                    }
                )
            ),
            cv.Optional(CONF_BERRY_DRIVER_VERSION, default=999): cv.positive_int,
            cv.Optional(CONF_USE_MISSED_UPDATES_WORKAROUND, default=True): cv.boolean,
            cv.Optional(CONF_UPDATE_BAUD_RATE, default=921600): cv.positive_int,
        }
    )
        .extend(uart.UART_DEVICE_SCHEMA)
        .extend(cv.COMPONENT_SCHEMA),
    validate_config
        )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    mqtt_parent = await cg.get_variable(config[CONF_MQTT_PARENT_ID])
    cg.add(var.set_mqtt(mqtt_parent))
    cg.add(var.set_recv_topic(config[CONF_MQTT_RECV_TOPIC]))
    cg.add(var.set_send_topic(config[CONF_MQTT_SEND_TOPIC]))
    cg.add(var.set_berry_driver_version(config[CONF_BERRY_DRIVER_VERSION]))
    cg.add(var.set_missed_updates_workaround(config[CONF_USE_MISSED_UPDATES_WORKAROUND]))
    cg.add(var.set_update_baud_rate(config[CONF_UPDATE_BAUD_RATE]))

    for conf in config.get(CONF_INCOMING_MSG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)

    cg.add_define("USE_NSPANEL_LOVELACE")
