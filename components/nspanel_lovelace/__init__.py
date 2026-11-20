from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import esp32, mqtt, uart
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@sairon"]
DEPENDENCIES = ["uart", "wifi", "esp32"]


nspanel_lovelace_ns = cg.esphome_ns.namespace("nspanel_lovelace")
NSPanelLovelace = nspanel_lovelace_ns.class_("NSPanelLovelace", cg.Component, uart.UARTDevice)


NSPanelLovelaceMsgIncomingTrigger = nspanel_lovelace_ns.class_(
    "NSPanelLovelaceMsgIncomingTrigger",
    automation.Trigger.template(cg.std_string)
)

CONF_CONNECTION = "connection"
CONNECTION_MQTT = "mqtt"
CONNECTION_API = "api"
CONF_API_DEVICE_ID = "api_device_id"
CONF_MQTT_PARENT_ID = "mqtt_parent_id"
CONF_MQTT_RECV_TOPIC = "mqtt_recv_topic"
CONF_MQTT_SEND_TOPIC = "mqtt_send_topic"
CONF_INCOMING_MSG = "on_incoming_msg"
CONF_BERRY_DRIVER_VERSION = "berry_driver_version"
CONF_USE_MISSED_UPDATES_WORKAROUND = "use_missed_updates_workaround"
CONF_UPDATE_BAUD_RATE = "update_baud_rate"


def AUTO_LOAD(config):
    if config and config.get(CONF_CONNECTION) == CONNECTION_API:
        return ["text_sensor", "api"]
    return ["text_sensor", "mqtt"]


def validate_config(config):
    connection = config[CONF_CONNECTION]
    if int(config[CONF_BERRY_DRIVER_VERSION]) > 0:
        if connection == CONNECTION_MQTT and "CustomSend" not in config[CONF_MQTT_SEND_TOPIC]:
            raise cv.Invalid(f"{CONF_MQTT_SEND_TOPIC} must contain \"CustomSend\" for correct backend compatibility.\n"
                             f"Either change it or set {CONF_BERRY_DRIVER_VERSION} to 0.")
    return config


COMMON_SCHEMA = {
    cv.GenerateID(): cv.declare_id(NSPanelLovelace),
    cv.Optional(CONF_API_DEVICE_ID): cv.string,
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

MQTT_SCHEMA = cv.Schema({
    **COMMON_SCHEMA,
    cv.GenerateID(CONF_MQTT_PARENT_ID): cv.use_id(mqtt.MQTTClientComponent),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)

API_SCHEMA = cv.Schema(COMMON_SCHEMA).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            CONNECTION_MQTT: MQTT_SCHEMA,
            CONNECTION_API: API_SCHEMA,
        },
        key=CONF_CONNECTION,
        default_type=CONNECTION_MQTT,
    ),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_connection(config[CONF_CONNECTION] == CONNECTION_API))
    cg.add(var.set_berry_driver_version(config[CONF_BERRY_DRIVER_VERSION]))
    cg.add(var.set_missed_updates_workaround(config[CONF_USE_MISSED_UPDATES_WORKAROUND]))
    cg.add(var.set_update_baud_rate(config[CONF_UPDATE_BAUD_RATE]))

    if config[CONF_CONNECTION] == CONNECTION_MQTT:
        mqtt_parent = await cg.get_variable(config[CONF_MQTT_PARENT_ID])
        cg.add(var.set_mqtt(mqtt_parent))
        cg.add(var.set_recv_topic(config[CONF_MQTT_RECV_TOPIC]))
        cg.add(var.set_send_topic(config[CONF_MQTT_SEND_TOPIC]))
    else:
        api_device_id = config.get(CONF_API_DEVICE_ID, config[CONF_MQTT_RECV_TOPIC])
        cg.add(var.set_api_device_id(api_device_id))
        cg.add_define("USE_API_CUSTOM_SERVICES")
        cg.add_define("USE_API_USER_DEFINED_ACTIONS")
        cg.add_define("USE_API_HOMEASSISTANT_SERVICES")

    for conf in config.get(CONF_INCOMING_MSG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if hasattr(esp32, "request_http_client"):
        esp32.request_http_client()
    else:
        esp32.include_builtin_idf_component("esp_http_client")
    if hasattr(esp32, "allow_insecure_tls"):
        esp32.allow_insecure_tls()
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True)

    cg.add_define("USE_NSPANEL_LOVELACE")
