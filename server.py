#!/usr/bin/env python
from math import floor
from world import World, show_clouds, show_plants, show_trees
import atexit
import datetime
import random
import re
#import requests
import sqlite3
import sys
import threading
import time
import traceback
is_py2 = sys.version[0] == '2'
if is_py2:
    import Queue as queue
    import SocketServer as socketserver
else:
    import queue
    import socketserver

DEFAULT_HOST = '0.0.0.0'
DEFAULT_PORT = 4080

DB_PATH = 'my.piworld'
LOG_PATH = 'log.txt'

CHUNK_SIZE = 16
BUFFER_SIZE = 4096
COMMIT_INTERVAL = 5

MAX_LOCAL_PLAYERS = 4
MAX_SIGN_LENGTH = 256

AUTH_REQUIRED = False

DAY_LENGTH = 600
SPAWN_POINT = (0, 0, 0, 0, 0)
RATE_LIMIT = False
RECORD_HISTORY = False
INDESTRUCTIBLE_ITEMS = set([16])
ALLOWED_ITEMS = set([
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    17, 18, 19, 20, 21, 22, 23,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63])

ADD = 'F'
AUTHENTICATE = 'A'
BLOCK = 'B'
CHUNK = 'C'
DISCONNECT = 'D'
EVENT = 'v'
EXTRA = 'e'
GOTO = 'G'
KEY = 'K'
LIGHT = 'L'
NICK = 'N'
OPTION = 'O'
POSITION = 'P'
PQ = 'Q'
REDRAW = 'R'
REMOVE = 'X'
SHAPE = 's'
SIGN = 'S'
SPAWN = 'W'
TALK = 'T'
TIME = 'E'
TRANSFORM = 't'
VERSION = 'V'
YOU = 'U'

worldgen = ""

try:
    from config import *
except ImportError:
    pass

def log(*args):
    now = datetime.datetime.utcnow()
    line = ' '.join(map(str, (now,) + args))
    print(line)
    with open(LOG_PATH, 'a') as fp:
        fp.write('%s\n' % line)

def chunked(x):
    return int(floor(round(x) / CHUNK_SIZE))

def packet(*args):
    return '%s\n' % ','.join(map(str, args))

class RateLimiter(object):
    def __init__(self, rate, per):
        self.rate = float(rate)
        self.per = float(per)
        self.allowance = self.rate
        self.last_check = time.time()
    def tick(self):
        if not RATE_LIMIT:
            return False
        now = time.time()
        elapsed = now - self.last_check
        self.last_check = now
        self.allowance += elapsed * (self.rate / self.per)
        if self.allowance > self.rate:
            self.allowance = self.rate
        if self.allowance < 1:
            return True # too fast
        else:
            self.allowance -= 1
            return False # okay

class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True

class Player:
    def __init__(self, nick, position, pid):
        self.pid = pid
        self.nick = nick
        self.position = position
        self.is_active = False

class Handler(socketserver.BaseRequestHandler):
    def setup(self):
        self.position_limiter = RateLimiter(100, 5)
        self.limiter = RateLimiter(1000, 10)
        self.version = None
        self.client_id = None
        self.user_id = None
        self.queue = queue.Queue()
        self.running = True
        self.players = []
        self.start()
    def handle(self):
        model = self.server.model
        model.enqueue(model.on_connect, self)
        try:
            buf = []
            while True:
                if is_py2:
                    data = self.request.recv(BUFFER_SIZE)
                else:
                    data = str(self.request.recv(BUFFER_SIZE), 'utf-8')
                if not data:
                    break
                buf.extend(data.replace('\r\n', '\n'))
                while '\n' in buf:
                    index = buf.index('\n')
                    line = ''.join(buf[:index])
                    buf = buf[index + 1:]
                    if not line:
                        continue
                    if line[0] == POSITION:
                        if self.position_limiter.tick():
                            log('RATE', self.client_id)
                            self.stop()
                            return
                    else:
                        if self.limiter.tick():
                            log('RATE', self.client_id)
                            self.stop()
                            return
                    model.enqueue(model.on_data, self, line)
        finally:
            model.enqueue(model.on_disconnect, self)
    def finish(self):
        self.running = False
    def stop(self):
        self.request.close()
    def start(self):
        thread = threading.Thread(target=self.run)
        thread.setDaemon(True)
        thread.start()
    def run(self):
        while self.running:
            try:
                buf = []
                try:
                    buf.append(self.queue.get(timeout=5))
                    try:
                        while True:
                            buf.append(self.queue.get(False))
                    except queue.Empty:
                        pass
                except queue.Empty:
                    continue
                data = ''.join(buf)
                if is_py2:
                    self.request.sendall(data)
                else:
                    self.request.sendall(bytes(data, 'utf-8'))

            except Exception:
                self.request.close()
                raise
    def send_raw(self, data):
        if data:
            self.queue.put(data)
    def send(self, *args):
        self.send_raw(packet(*args))
    def active_players(self):
        return [x for x in self.players if x.is_active]

class Model(object):
    def __init__(self, seed):
        self.world = World(seed)
        self.clients = []
        self.queue = queue.Queue()
        self.commands = {
            ADD: self.on_add,
            AUTHENTICATE: self.on_authenticate,
            CHUNK: self.on_chunk,
            BLOCK: self.on_block,
            EVENT: self.on_control_callback,
            EXTRA: self.on_extra,
            GOTO: self.on_goto,
            LIGHT: self.on_light,
            NICK: self.on_nick,
            POSITION: self.on_position,
            PQ: self.on_pq,
            REMOVE: self.on_remove,
            TALK: self.on_talk,
            SHAPE: self.on_shape,
            SIGN: self.on_sign,
            SPAWN: self.on_spawn,
            TRANSFORM: self.on_transform,
            VERSION: self.on_version,
        }
        self.patterns = [
            (re.compile(r'^/help(?:\s+(\S+))?$'), self.on_help),
            (re.compile(r'^/list$'), self.on_list),
        ]
        self.running = True
    def finish(self):
        self.running = False
    def start(self):
        thread = threading.Thread(target=self.run)
        thread.setDaemon(True)
        thread.start()
        self.thread = thread
    def run(self):
        self.connection = sqlite3.connect(DB_PATH)
        self.create_tables()
        self.commit()
        query = (
            'select value from option where '
            'name = :name;'
        )
        rows = list(self.execute(query, dict(name="show-clouds")))
        if rows:
            show_clouds.value = int(rows[0][0])
        rows = list(self.execute(query, dict(name="show-plants")))
        if rows:
            show_plants.value = int(rows[0][0])
        rows = list(self.execute(query, dict(name="show-trees")))
        if rows:
            show_trees.value = int(rows[0][0])
        while self.running:
            try:
                if time.time() - self.last_commit > COMMIT_INTERVAL:
                    self.commit()
                self.dequeue()
            except Exception:
                traceback.print_exc()
        # Commit any pending changes before exiting the thread. This will
        # prevent sqlite leaving behind a journal file.
        self.commit()
    def enqueue(self, func, *args, **kwargs):
        self.queue.put((func, args, kwargs))
    def dequeue(self):
        try:
            func, args, kwargs = self.queue.get(timeout=5)
            func(*args, **kwargs)
        except queue.Empty:
            pass
    def execute(self, *args, **kwargs):
        return self.connection.execute(*args, **kwargs)
    def commit(self):
        self.last_commit = time.time()
        self.connection.commit()
    def create_tables(self):
        queries = [
            'create table if not exists block ('
            '    p int not null,'
            '    q int not null,'
            '    x int not null,'
            '    y int not null,'
            '    z int not null,'
            '    w int not null'
            ');',
            'create unique index if not exists block_pqxyz_idx on '
            '    block (p, q, x, y, z);',
            'create table if not exists extra ('
            '    p int not null,'
            '    q int not null,'
            '    x int not null,'
            '    y int not null,'
            '    z int not null,'
            '    w int not null'
            ');',
            'create unique index if not exists extra_pqxyz_idx on '
            '    extra (p, q, x, y, z);',
            'create table if not exists light ('
            '    p int not null,'
            '    q int not null,'
            '    x int not null,'
            '    y int not null,'
            '    z int not null,'
            '    w int not null'
            ');',
            'create unique index if not exists light_pqxyz_idx on '
            '    light (p, q, x, y, z);',
            'create table if not exists shape ('
            '    p int not null,'
            '    q int not null,'
            '    x int not null,'
            '    y int not null,'
            '    z int not null,'
            '    w int not null'
            ');',
            'create unique index if not exists shape_pqxyz_idx on '
            '    shape (p, q, x, y, z);',
            'create table if not exists transform ('
            '    p int not null,'
            '    q int not null,'
            '    x int not null,'
            '    y int not null,'
            '    z int not null,'
            '    w int not null'
            ');',
            'create unique index if not exists transform_pqxyz_idx on '
            '    transform (p, q, x, y, z);',
            'create table if not exists sign ('
            '    p int not null,'
            '    q int not null,'
            '    x int not null,'
            '    y int not null,'
            '    z int not null,'
            '    face int not null,'
            '    text text not null'
            ');',
            'create index if not exists sign_pq_idx on sign (p, q);',
            'create unique index if not exists sign_xyzface_idx on '
            '    sign (x, y, z, face);',
            'create table if not exists block_history ('
            '   timestamp real not null,'
            '   user_id int not null,'
            '   x int not null,'
            '   y int not null,'
            '   z int not null,'
            '   w int not null'
            ');',
            'create table if not exists option ('
            '    name text not null,'
            '    value text not null'
            ');',
            'create unique index if not exists option_idx on option (name);',
        ]
        for query in queries:
            self.execute(query)
    def get_default_block(self, x, y, z):
        p, q = chunked(x), chunked(z)
        chunk = self.world.get_chunk(p, q)
        return chunk.get((x, y, z), 0)
    def get_block(self, x, y, z):
        query = (
            'select w from block where '
            'p = :p and q = :q and x = :x and y = :y and z = :z;'
        )
        p, q = chunked(x), chunked(z)
        rows = list(self.execute(query, dict(p=p, q=q, x=x, y=y, z=z)))
        if rows:
            return rows[0][0]
        return self.get_default_block(x, y, z)
    def next_client_id(self):
        result = 1
        client_ids = set(x.client_id for x in self.clients)
        while result in client_ids:
            result += 1
        return result
    def on_connect(self, client):
        client.client_id = self.next_client_id()
        log('CONN', client.client_id, *client.client_address)
        client.players = []
        self.clients.append(client)
        client.send(TIME, time.time(), DAY_LENGTH)
        client.send(TALK, 'Welcome to PiWorld!')
        client.send(TALK, 'Type "/help" for a list of commands.')
        for i in range(MAX_LOCAL_PLAYERS):
            p = i + 1
            player = Player('guest%d-%d' % (client.client_id, p), SPAWN_POINT, p)
            client.players.append(player)
            client.send(YOU, client.client_id, p, *client.players[i].position)
            self.send_nick(client, p)
        for i in range(MAX_LOCAL_PLAYERS):
            self.send_positions(client, i + 1)
        self.send_nicks(client)
        self.send_options(client)
    def on_data(self, client, data):
        #log('RECV', client.client_id, data)
        args = data.split(',')
        command, args = args[0], args[1:]
        if command in self.commands:
            func = self.commands[command]
            func(client, *args)
    def on_disconnect(self, client):
        log('DISC', client.client_id, *client.client_address)
        self.clients.remove(client)
        self.send_disconnect(client)
        self.send_talk('%s has disconnected from the server.' % client.players[0].nick)
    def on_version(self, client, version):
        if client.version is not None:
            return
        version = int(version)
        if version != 2:
            client.stop()
            print("Unmatched client version:", version)
            return
        client.version = version
        # TODO: client.start() here
    def on_authenticate(self, client, username, access_token):
        user_id = None
        #if username and access_token:
        #    payload = {
        #        'username': username,
        #        'access_token': access_token,
        #    }
        #    response = requests.post(AUTH_URL, data=payload)
        #    if response.status_code == 200 and response.text.isdigit():
        #        user_id = int(response.text)
        client.user_id = user_id
        if user_id is None:
            client.nick = 'guest%d' % client.client_id
        else:
            client.nick = username
        self.send_nick(client, 1)
        # TODO: has left message if was already authenticated
        self.send_talk('%s has joined the game.' % client.players[0].nick)
    def on_chunk(self, client, p, q, key=0):
        packets = []
        p, q, key = map(int, (p, q, key))
        query = (
            'select rowid, x, y, z, w from block where '
            'p = :p and q = :q and rowid > :key;'
        )
        rows = self.execute(query, dict(p=p, q=q, key=key))
        max_rowid = 0
        blocks = 0
        for rowid, x, y, z, w in rows:
            blocks += 1
            packets.append(packet(BLOCK, p, q, x, y, z, w))
            max_rowid = max(max_rowid, rowid)
        query = (
            'select rowid, x, y, z, w from extra where '
            'p = :p and q = :q and rowid > :key;'
        )
        rows = self.execute(query, dict(p=p, q=q, key=key))
        extras = 0
        for rowid, x, y, z, w in rows:
            extras += 1
            packets.append(packet(EXTRA, p, q, x, y, z, w))
        query = (
            'select x, y, z, w from light where '
            'p = :p and q = :q;'
        )
        rows = self.execute(query, dict(p=p, q=q))
        lights = 0
        for x, y, z, w in rows:
            lights += 1
            packets.append(packet(LIGHT, p, q, x, y, z, w))
        query = (
            'select rowid, x, y, z, w from shape where '
            'p = :p and q = :q and rowid > :key;'
        )
        rows = self.execute(query, dict(p=p, q=q, key=key))
        shapes = 0
        for rowid, x, y, z, w in rows:
            shapes += 1
            packets.append(packet(SHAPE, p, q, x, y, z, w))
        query = (
            'select rowid, x, y, z, w from transform where '
            'p = :p and q = :q and rowid > :key;'
        )
        rows = self.execute(query, dict(p=p, q=q, key=key))
        transforms = 0
        for rowid, x, y, z, w in rows:
            transforms += 1
            packets.append(packet(TRANSFORM, p, q, x, y, z, w))
        query = (
            'select x, y, z, face, text from sign where '
            'p = :p and q = :q;'
        )
        rows = self.execute(query, dict(p=p, q=q))
        signs = 0
        for x, y, z, face, text in rows:
            signs += 1
            packets.append(packet(SIGN, p, q, x, y, z, face, text))
        if blocks:
            packets.append(packet(KEY, p, q, max_rowid))
        if blocks or extras or lights or shapes or signs or transforms:
            packets.append(packet(REDRAW, p, q))
        packets.append(packet(CHUNK, p, q))
        client.send_raw(''.join(packets))
    def on_block(self, client, x, y, z, w):
        x, y, z, w = map(int, (x, y, z, w))
        p, q = chunked(x), chunked(z)
        previous = self.get_block(x, y, z)
        message = None
        if AUTH_REQUIRED and client.user_id is None:
            message = 'Only logged in users are allowed to build.'
        elif y <= 0 or y > 255:
            message = 'Invalid block coordinates.'
        elif w not in ALLOWED_ITEMS:
            message = 'That item is not allowed.'
        elif w and previous:
            message = 'Cannot create blocks in a non-empty space.'
        elif not w and not previous:
            message = 'That space is already empty.'
        elif previous in INDESTRUCTIBLE_ITEMS:
            message = 'Cannot destroy that type of block.'
        if message is not None:
            client.send(BLOCK, p, q, x, y, z, previous)
            client.send(REDRAW, p, q)
            client.send(TALK, message)
            return
        query = (
            'insert into block_history (timestamp, user_id, x, y, z, w) '
            'values (:timestamp, :user_id, :x, :y, :z, :w);'
        )
        if RECORD_HISTORY:
            self.execute(query, dict(timestamp=time.time(),
                user_id=client.user_id, x=x, y=y, z=z, w=w))
        query = (
            'insert or replace into block (p, q, x, y, z, w) '
            'values (:p, :q, :x, :y, :z, :w);'
        )
        self.execute(query, dict(p=p, q=q, x=x, y=y, z=z, w=w))
        self.send_block(client, p, q, x, y, z, w)
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                if dx == 0 and dz == 0:
                    continue
                if dx and chunked(x + dx) == p:
                    continue
                if dz and chunked(z + dz) == q:
                    continue
                np, nq = p + dx, q + dz
                self.execute(query, dict(p=np, q=nq, x=x, y=y, z=z, w=-w))
                self.send_block(client, np, nq, x, y, z, -w)
        if w == 0:
            query = (
                'delete from sign where '
                'x = :x and y = :y and z = :z;'
            )
            self.execute(query, dict(x=x, y=y, z=z))
            query = (
                'update extra set w = 0 where '
                'x = :x and y = :y and z = :z;'
            )
            self.execute(query, dict(x=x, y=y, z=z))
            query = (
                'update light set w = 0 where '
                'x = :x and y = :y and z = :z;'
            )
            self.execute(query, dict(x=x, y=y, z=z))
            query = (
                'update shape set w = 0 where '
                'x = :x and y = :y and z = :z;'
            )
            self.execute(query, dict(x=x, y=y, z=z))
            query = (
                'update transform set w = 0 where '
                'x = :x and y = :y and z = :z;'
            )
            self.execute(query, dict(x=x, y=y, z=z))
    def on_extra(self, client, x, y, z, w):
        x, y, z, w = map(int, (x, y, z, w))
        p, q = chunked(x), chunked(z)
        block = self.get_block(x, y, z)
        message = None
        if AUTH_REQUIRED and client.user_id is None:
            message = 'Only logged in users are allowed to build.'
        elif y <= 0 or y > 255:
            message = 'Invalid block coordinates.'
        elif block == 0:
            message = 'Extras must be placed on a block.'
        if message is not None:
            # TODO: client.send(EXTRA, p, q, x, y, z, previous)
            client.send(REDRAW, p, q)
            client.send(TALK, message)
            return
        query = (
            'insert or replace into extra (p, q, x, y, z, w) '
            'values (:p, :q, :x, :y, :z, :w);'
        )
        self.execute(query, dict(p=p, q=q, x=x, y=y, z=z, w=w))
        self.send_extra(client, p, q, x, y, z, w)
    def on_light(self, client, x, y, z, w):
        x, y, z, w = map(int, (x, y, z, w))
        p, q = chunked(x), chunked(z)
        block = self.get_block(x, y, z)
        message = None
        if AUTH_REQUIRED and client.user_id is None:
            message = 'Only logged in users are allowed to build.'
        elif block == 0:
            message = 'Lights must be placed on a block.'
        elif w < 0 or w > 15:
            message = 'Invalid light value.'
        if message is not None:
            # TODO: client.send(LIGHT, p, q, x, y, z, previous)
            client.send(REDRAW, p, q)
            client.send(TALK, message)
            return
        query = (
            'insert or replace into light (p, q, x, y, z, w) '
            'values (:p, :q, :x, :y, :z, :w);'
        )
        self.execute(query, dict(p=p, q=q, x=x, y=y, z=z, w=w))
        self.send_light(client, p, q, x, y, z, w)
    def on_shape(self, client, x, y, z, w):
        x, y, z, w = map(int, (x, y, z, w))
        p, q = chunked(x), chunked(z)
        block = self.get_block(x, y, z)
        message = None
        if AUTH_REQUIRED and client.user_id is None:
            message = 'Only logged in users are allowed to build.'
        elif y <= 0 or y > 255:
            message = 'Invalid block coordinates.'
        elif block == 0:
            message = 'Shape must be placed on a block.'
        if message is not None:
            # TODO: client.send(SHAPE, p, q, x, y, z, previous)
            client.send(REDRAW, p, q)
            client.send(TALK, message)
            return
        query = (
            'insert or replace into shape (p, q, x, y, z, w) '
            'values (:p, :q, :x, :y, :z, :w);'
        )
        self.execute(query, dict(p=p, q=q, x=x, y=y, z=z, w=w))
        self.send_shape(client, p, q, x, y, z, w)
    def on_transform(self, client, x, y, z, w):
        x, y, z, w = map(int, (x, y, z, w))
        p, q = chunked(x), chunked(z)
        block = self.get_block(x, y, z)
        message = None
        if AUTH_REQUIRED and client.user_id is None:
            message = 'Only logged in users are allowed to build.'
        elif y <= 0 or y > 255:
            message = 'Invalid block coordinates.'
        elif block == 0:
            message = 'Transform must be placed on a block.'
        if message is not None:
            # TODO: client.send(TRANSFORM, p, q, x, y, z, previous)
            client.send(REDRAW, p, q)
            client.send(TALK, message)
            return
        query = (
            'insert or replace into transform (p, q, x, y, z, w) '
            'values (:p, :q, :x, :y, :z, :w);'
        )
        self.execute(query, dict(p=p, q=q, x=x, y=y, z=z, w=w))
        self.send_transform(client, p, q, x, y, z, w)
    def on_sign(self, client, x, y, z, face, *args):
        if AUTH_REQUIRED and client.user_id is None:
            client.send(TALK, 'Only logged in users are allowed to build.')
            return
        text = ','.join(args)
        x, y, z, face = map(int, (x, y, z, face))
        if y <= 0 or y > 255:
            return
        if face < 0 or face > 7:
            return
        if len(text) > MAX_SIGN_LENGTH:
            text = text[:MAX_SIGN_LENGTH-1]
            print("Truncating long sign text.")
        p, q = chunked(x), chunked(z)
        if text:
            query = (
                'insert or replace into sign (p, q, x, y, z, face, text) '
                'values (:p, :q, :x, :y, :z, :face, :text);'
            )
            self.execute(query,
                dict(p=p, q=q, x=x, y=y, z=z, face=face, text=text))
        else:
            query = (
                'delete from sign where '
                'x = :x and y = :y and z = :z and face = :face;'
            )
            self.execute(query, dict(x=x, y=y, z=z, face=face))
        self.send_sign(client, p, q, x, y, z, face, text)
    def on_position(self, client, player, x, y, z, rx, ry):
        player = int(player)
        x, y, z, rx, ry = map(float, (x, y, z, rx, ry))
        client.players[player - 1].position = (x, y, z, rx, ry)
        self.send_position(client, player)
    def on_add(self, client, player):
        player = int(player)
        client.players[player - 1].is_active = True
        self.send_add(client, player)
    def on_remove(self, client, player):
        player = int(player)
        client.players[player - 1].is_active = False
        self.send_remove(client, player)
    def on_talk(self, client, *args):
        text = ','.join(args)
        if text.startswith('/'):
            for pattern, func in self.patterns:
                match = pattern.match(text)
                if match:
                    func(client, *match.groups())
                    break
            else:
                client.send(TALK, 'Unrecognized command: "%s"' % text)
        elif text.startswith('@'):
            nick = text[1:].split(' ', 1)[0]
            for other in self.clients:
                if other.nick == nick:
                    client.send(TALK, '%s> %s' % (client.nick, text))
                    other.send(TALK, '%s> %s' % (client.nick, text))
                    break
            else:
                client.send(TALK, 'Unrecognized nick: "%s"' % nick)
        else:
            self.send_talk('%s> %s' % (client.nick, text))
    def on_nick(self, client, player, nick=None):
        player = int(player)
        if AUTH_REQUIRED:
            client.send(TALK, 'You cannot change your nick on this server.')
            return
        if nick is None:
            client.send(TALK, 'Your nickname is %s' % client.players[player - 1].nick)
        else:
            self.send_talk('%s is now known as %s' % (client.players[player - 1].nick, nick))
            client.players[player - 1].nick = nick
            self.send_nick(client, player)
    def on_spawn(self, client, player):
        player = int(player)
        client.players[player - 1].position = SPAWN_POINT
        client.send(YOU, client.client_id, player, *client.players[player - 1].position)
        self.send_position(client, player)
    def on_goto(self, client, player, nick=None):
        player = int(player)
        if nick in (None, ""):
            clients = [x for x in self.clients if (x != client and len(client.active_players()) > 0)]
            if len(client.active_players()) > 1:
                # Include own client if > 1 active players
                clients.append(client)
            other = random.choice(clients) if clients else None
            active_players = other.active_players()
            other_player = random.choice(active_players) if active_players else None
        else:
            nicks = {}
            for client in self.clients:
                nicks.update(dict((player.nick, (client, player)) for player in client.players))
            other = nicks.get(nick)[0]
            other_player = nicks.get(nick)[1]
        if other and other_player:
            client.players[player - 1].position = other_player.position
            client.send(YOU, client.client_id, player, *client.players[player - 1].position)
            self.send_position(client, player)
    def on_pq(self, client, player, p, q):
        player = int(player)
        p, q = map(int, (p, q))
        if abs(p) > 1000 or abs(q) > 1000:
            return
        client.players[player - 1].position = (p * CHUNK_SIZE, 0, q * CHUNK_SIZE, 0, 0)
        client.send(YOU, client.client_id, player, *client.players[player - 1].position)
        self.send_position(client, player)
    def on_help(self, client, topic=None):
        if topic is None:
            client.send(TALK, 'Type "t" to chat. Type "/" to type commands:')
            client.send(TALK, '/goto [NAME], /help [TOPIC], /list, /login NAME, /logout, /nick')
            client.send(TALK, '/offline [FILE], /online HOST [PORT], /pq P Q, /spawn, /view N')
            return
        topic = topic.lower().strip()
        if topic == 'goto':
            client.send(TALK, 'Help: /goto [NAME]')
            client.send(TALK, 'Teleport to another user.')
            client.send(TALK, 'If NAME is unspecified, a random user is chosen.')
        elif topic == 'list':
            client.send(TALK, 'Help: /list')
            client.send(TALK, 'Display a list of connected users.')
        elif topic == 'login':
            client.send(TALK, 'Help: /login NAME')
            client.send(TALK, 'Switch to another registered username.')
            client.send(TALK, 'The login server will be re-contacted. The username is case-sensitive.')
        elif topic == 'logout':
            client.send(TALK, 'Help: /logout')
            client.send(TALK, 'Unauthenticate and become a guest user.')
            client.send(TALK, 'Automatic logins will not occur again until the /login command is re-issued.')
        elif topic == 'offline':
            client.send(TALK, 'Help: /offline [FILE]')
            client.send(TALK, 'Switch to offline mode.')
            client.send(TALK, 'FILE specifies the save file to use and defaults to "craft".')
        elif topic == 'online':
            client.send(TALK, 'Help: /online HOST [PORT]')
            client.send(TALK, 'Connect to the specified server.')
        elif topic == 'nick':
            client.send(TALK, 'Help: /nick [NICK]')
            client.send(TALK, 'Get or set your nickname.')
        elif topic == 'pq':
            client.send(TALK, 'Help: /pq P Q')
            client.send(TALK, 'Teleport to the specified chunk.')
        elif topic == 'spawn':
            client.send(TALK, 'Help: /spawn')
            client.send(TALK, 'Teleport back to the spawn point.')
        elif topic == 'view':
            client.send(TALK, 'Help: /view N')
            client.send(TALK, 'Set viewing distance, 1 - 24.')
    def on_list(self, client):
        players = []
        for c in self.clients:
            players.extend(x.nick for x in c.active_players())
        client.send(TALK, 'Players: %s' % ', '.join(players))
    def on_control_callback(self, client, player, x, y, z, face):
        print("Control callback: ", player, x, y, z, face)
    def send_positions(self, client, player):
        for other in self.clients:
            other_player = other.players[player - 1]
            if other == client or not(other_player.is_active):
                continue
            client.send(POSITION, other.client_id, player, *other_player.position)
    def send_position(self, client, player):
        for other in self.clients:
            if other == client:
                continue
            other.send(POSITION, client.client_id, player, *client.players[player - 1].position)
    def send_add(self, client, player):
        for other in self.clients:
            if other == client:
                continue
            other.send(ADD, client.client_id, player)
        self.send_position(client, player)
        self.send_positions(client, player)
        for i in range(MAX_LOCAL_PLAYERS):
            self.send_nick(client, i + 1)
        self.send_nicks(client)
    def send_remove(self, client, player):
        for other in self.clients:
            if other == client:
                continue
            other.send(REMOVE, client.client_id, player)
    def send_nicks(self, client):
        for other in self.clients:
            if other == client:
                continue
            for i in range(MAX_LOCAL_PLAYERS):
                client.send(NICK, other.client_id, i+1, other.players[i].nick)
    def send_options(self, client):
        query = (
            'select name, value from option;'
        )
        rows = list(self.execute(query))
        for name, value in rows:
            client.send(OPTION, name, value)
        if worldgen:
            client.send(OPTION, "worldgen", worldgen)
    def send_nick(self, client, player_index):
        for other in self.clients:
            other.send(NICK, client.client_id, player_index,
                       client.players[player_index - 1].nick)
    def send_disconnect(self, client):
        for other in self.clients:
            if other == client:
                continue
            other.send(DISCONNECT, client.client_id)
    def send_block(self, client, p, q, x, y, z, w):
        for other in self.clients:
            if other == client:
                continue
            other.send(BLOCK, p, q, x, y, z, w)
            other.send(REDRAW, p, q)
    def send_extra(self, client, p, q, x, y, z, w):
        for other in self.clients:
            if other == client:
                continue
            other.send(EXTRA, p, q, x, y, z, w)
            other.send(REDRAW, p, q)
    def send_light(self, client, p, q, x, y, z, w):
        for other in self.clients:
            if other == client:
                continue
            other.send(LIGHT, p, q, x, y, z, w)
            other.send(REDRAW, p, q)
    def send_shape(self, client, p, q, x, y, z, w):
        for other in self.clients:
            if other == client:
                continue
            other.send(SHAPE, p, q, x, y, z, w)
            other.send(REDRAW, p, q)
    def send_transform(self, client, p, q, x, y, z, w):
        for other in self.clients:
            if other == client:
                continue
            other.send(TRANSFORM, p, q, x, y, z, w)
            other.send(REDRAW, p, q)
    def send_sign(self, client, p, q, x, y, z, face, text):
        for other in self.clients:
            if other == client:
                continue
            other.send(SIGN, p, q, x, y, z, face, text)
            other.send(REDRAW, p, q)
    def send_talk(self, text):
        log(text)
        for client in self.clients:
            client.send(TALK, text)

def cleanup():
    world = World(None)
    conn = sqlite3.connect(DB_PATH)
    query = 'select x, y, z from block order by rowid desc limit 1;'
    last = list(conn.execute(query))[0]
    query = 'select distinct p, q from block;'
    chunks = list(conn.execute(query))
    count = 0
    total = 0
    delete_query = 'delete from block where x = %d and y = %d and z = %d;'
    print('begin;')
    for p, q in chunks:
        chunk = world.create_chunk(p, q)
        query = 'select x, y, z, w from block where p = :p and q = :q;'
        rows = conn.execute(query, {'p': p, 'q': q})
        for x, y, z, w in rows:
            if chunked(x) != p or chunked(z) != q:
                continue
            total += 1
            if (x, y, z) == last:
                continue
            original = chunk.get((x, y, z), 0)
            if w == original or original in INDESTRUCTIBLE_ITEMS:
                count += 1
                print(delete_query % (x, y, z))
    conn.close()
    print('commit;')
    print >> sys.stderr, '%d of %d blocks will be cleaned up' % (count, total)

def close_server(server):
    print("\nPlease wait for server to close (upto 5 seconds)...")
    server.server_close()
    server.model.finish()
    server.model.thread.join()

def main():
    if len(sys.argv) == 2 and sys.argv[1] == 'cleanup':
        cleanup()
        return
    host, port = DEFAULT_HOST, DEFAULT_PORT
    if '--worldgen' in sys.argv:
        global worldgen
        i = sys.argv.index('--worldgen')
        worldgen_value = sys.argv[i+1]
        worldgen = worldgen_value
        del sys.argv[i+1]
        del sys.argv[i]
    if len(sys.argv) > 1:
        host = sys.argv[1]
    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    log('SERV', host, port)
    model = Model(None)
    model.start()
    server = Server((host, port), Handler)
    server.model = model
    atexit.register(close_server, server)
    server.serve_forever()

if __name__ == '__main__':
    main()
