
import socket
from collections import namedtuple


MAX_PWPI_TEXT_LENGTH = 512
PWPI_PORT = 11760
PLAYER_HEIGHT = 2

Vec3 = namedtuple('Vec3', ('x', 'y', 'z'))
Vec2 = namedtuple('Vec2', ('x', 'y'))


class World:

    def __init__(self, port=PWPI_PORT):
        self.conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.conn.connect(('127.0.0.1', port))
        self._player= Player(self)

    def end(self):
        self._send('END')
        self._recv()
        self.conn.close()

    def _send(self, message):
        message += '\n'
        self.conn.sendall(message.encode())

    def _recv(self):
        return self.conn.recv(MAX_PWPI_TEXT_LENGTH)

    def echo(self, message):
        self._send('T,' + message)

    def get_player(self):
        self._player.update()
        return self._player

    def set_block(self, x, y, z, w):
        self._send('B,%d,%d,%d,%d' % (x, y, z, w))

    def set_blocks(self, blocks, w):
        key = lambda block: (block[1], block[0], block[2])
        for x, y, z in sorted(blocks, key=key):
            self.set_block(x, y, z, w)

    def get_block(self, x, y, z):
        self._send('Q,B,%d,%d,%d' % (x, y, z))
        data = int(self.conn.recv(MAX_PWPI_TEXT_LENGTH))
        return data

    def bitmap(self, sx, sy, sz, d1, d2, data, lookup):
        x, y, z = sx, sy, sz
        dx1, dy1, dz1 = d1
        dx2, dy2, dz2 = d2
        for row in data:
            x = sx if dx1 else x
            y = sy if dy1 else y
            z = sz if dz1 else z
            for c in row:
                w = lookup.get(c)
                if w is not None:
                    self.set_block(x, y, z, w)
                x, y, z = x + dx1, y + dy1, z + dz1
            x, y, z = x + dx2, y + dy2, z + dz2

    def offset_world(self, x, y, z):
        return OffsetWorld(self, x, y, z)


class Player:

    def __init__(self, world):
        self.world = world

    def update(self):
        self.world._send('Q,P')
        data = self.world._recv().split(b',')
        self._update(*data)

    def _update(self, x, y, z, rx, ry, cx, cy, cz, cw, item_in_hand):
        self.pos = Vec3(float(x), float(y), float(z))
        self.view_angle = Vec2(float(rx), float(ry))
        self.item_under_crosshair = int(cw)
        self.under_crosshair = Vec3(int(cx), int(cy), int(cz))
        self.item_in_hand = int(item_in_hand)
        self.under_foot = Vec3(int(round(self.pos.x)),
                               int(round(self.pos.y - PLAYER_HEIGHT)),
                               int(round(self.pos.z)))

    def set_pos(self, x, y, z):
        self.world._send('P,%f,%f,%f' % (x, y, z))

    def set_view_angle(self, x, y):
        self.world._send('P,%f,%f' % (x, y))

    def set_item_in_hand(self, item):
        self.world._send('H,%d' % item)


class OffsetWorld:

    def __init__(self, world, offset_x, offset_y, offset_z):
        self.world = world
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.offset_z = offset_z

    def get_player(self):
        return self.world.get_player()

    def set_block(self, x, y, z, w):
        self.world.set_block(x + self.offset_x, y + self.offset_y,
                             z + self.offset_z, w)

    def set_blocks(self, blocks, w):
        offset_blocks = map(lambda b: (b[0] + self.offset_x,
                                       b[1] + self.offset_y,
                                       b[2] + self.offset_z), blocks)
        self.world.set_blocks(offset_blocks, w)

    def get_block(self, x, y, z):
        return self.world.get_block(x, y, z)

    def bitmap(self, sx, sy, sz, d1, d2, data, lookup):
        self.world.bitmap(sx+self.offset_x, sy+self.offset_y, sz+self.offset_z,
                          d1, d2, data, lookup)

