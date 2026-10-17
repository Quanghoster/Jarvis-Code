"""JARVIS Avatar: the ESPHome side.
A voice-pipeline-reactive avatar tile for Tessera screens.
Shows the Home Assistant voice assistant state as an animated face.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, smart_display, switch, text
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]
AUTO_LOAD = ["switch", "number", "text"]

CONF_DEMO = "demo"
CONF_POLL = "poll"
CONF_STATE = "state"

jarvis_ns = cg.esphome_ns.namespace("jarvis_avatar")
JarvisAvatar = jarvis_ns.class_("JarvisAvatar", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(JarvisAvatar),
        cv.Required(CONF_DEMO): cv.use_id(switch.Switch),
        cv.Required(CONF_POLL): cv.use_id(number.Number),
        cv.Required(CONF_STATE): cv.use_id(text.Text),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_demo_switch(await cg.get_variable(config[CONF_DEMO])))
    cg.add(var.set_poll_number(await cg.get_variable(config[CONF_POLL])))
    cg.add(var.set_state_text(await cg.get_variable(config[CONF_STATE])))
