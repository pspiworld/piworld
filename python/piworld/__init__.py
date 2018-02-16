
import atexit
import piworld.world


def get_world(port=piworld.world.PWPI_PORT):
    world = piworld.world.World(port)
    atexit.register(world.end)
    return world

