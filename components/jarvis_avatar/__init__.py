"""JARVIS Avatar: the ESPHome side."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

jarvis_avatar_ns = cg.esphome_ns.namespace("jarvis_avatar")
JarvisAvatar = jarvis_avatar_ns.class_("JarvisAvatar", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(JarvisAvatar)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
