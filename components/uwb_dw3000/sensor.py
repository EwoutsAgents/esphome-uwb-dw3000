import esphome.config_validation as cv
from . import sensor_schema, sensor_to_code

CONFIG_SCHEMA = sensor_schema


async def to_code(config):
    await sensor_to_code(config)
