## PiWorld

A (Mine)Craft clone specifically for the Raspberry Pi. Network multiplayer
support is included using a Python-based server.

Forked from:

https://github.com/fogleman/Craft/

The primary hardware target for PiWorld is the Raspberry Pi Zero. Other
platforms and over-the-internet use are not the focus of this project.

To fit on the Pi Zero draw distances are low and anything that involves
shifting a large number of blocks will be painfully slow (the fully active
builder.py example script takes 30+ minutes on a Pi Zero to place just over
126,000 blocks). But for basic building with small amounts of scripting a Pi
Zero can be fast enough.

![Screenshot](https://github.com/pspiworld/piworld/wiki/images/piworld-basic.png)

[More screenshots](https://github.com/pspiworld/piworld/wiki/Screenshots)

### Features

* Simple but nice looking terrain generation using perlin / simplex noise.
* More than 10 types of blocks and more can be added easily.
* Supports plants (grass, flowers, trees, etc.) and transparency (glass).
* Simple clouds in the sky (they don't move).
* Day / night cycles and a textured sky dome.
* World changes persisted in a sqlite3 database.
* Multiplayer support!

### Install Dependencies

#### Linux (Raspbian) - dependencies to compile piworld

    sudo apt-get install cmake libsqlite3-dev xorg-dev

additional package required for Pi 4 (or the experimental Mesa driver for Pi 2
or 3):

    sudo apt-get install libgles2-mesa-dev

### Compile and Run

Once you have the dependencies (see above), run the following commands in your
terminal.

    git clone https://github.com/pspiworld/piworld.git
    cd piworld
    cmake -DRASPI=1 .
    make
    ./piworld

I recommend increasing the GPU memory on the Pi to a minimum of 128M.

#### Pi 4 (and experimental MESA driver on Pi 2 and 3)

To build for the Pi 4 or if you've enabled the experimental opengl driver (only
on the Pi 2 or 3) you can build PiWorld to use it:

    cmake -DMESA=1 -DRASPI=2 .
    make -j4

To reduce judder when playing you can start the game with:

    ./piworld --view=4 --show-plants=0

#### Making a release build

    cmake -DMESA=0 -DRASPI=1 -DRELEASE=1 .
    make

### Multiplayer

#### Client

You can connect to a server with command line arguments...

    ./piworld --server 192.168.1.64

Or, with the "/online" command in the game itself.

    /online 192.168.1.64

#### Server

The server is written in Python but requires a compiled DLL so it can perform
the terrain generation just like the client.

    gcc -DSERVER -std=c99 -O3 -fPIC -shared -o world -I src -I deps/noise deps/noise/noise.c src/world.c
    ./server.py [HOST [PORT]]

### Controls

- Esc to open the menu.
- WASD to move forward, left, backward, right.
- Space to jump.
- C to crouch.
- Left Click to destroy a block.
- Right Click or Ctrl + Left Click to create a block.
- Ctrl + Right Click to toggle a block as a light source.
- 1-9 to select the block type to create.
- I to open the block type menu.
- E to cycle through the block types.
- Tab to toggle between walking and flying.
- U to undo the removal of the last removed block.
- Z to zoom.
- F to show the scene in orthographic mode.
- O to observe players in the main view.
- P to observe players in the picture-in-picture view.
- T to type text into chat.
- Forward slash (/) to enter a command (see section: In Game Command Line).
- Backquote (`) to write text on any block (signs)
  (see section: Sign Text Markup).
- Dollar sign ($) to enter a Lua command.
- Arrow keys emulate mouse movement.
- Enter emulates mouse click.
- F8 to cycle current keyboard and mouse pair around local players.
- F11 to toggle fullscreen mode.

### In Game Command Line

Change your player name:

    /nick NAME

Set the number of local splitscreen players (1 to 4):

    /players N

Move your position to X Y Z:

    /position X Y Z

Set the time of day (N is a single number from 0 to 24):

    /time N

*Shape commands*

The `/cube` command uses the position of the last two edited blocks to form the
opposite corners of a cube, the `/sphere R` command uses the last edited block
as the spheres' centre and R as the radius. Both commands will insert blocks of
the current block type):

    /cube
    /sphere R

solid filled versions of the above commmands:

    /fcube
    /fsphere R

(**WARNING** there is no undo so be aware what is be in-between the area the
new shape will occupy before using any /shape command - you can destroy entire
worlds with these commands, **SECOND WARNING** any shape larger than the
current view area will freeze, for possibly a long time, the game until all
the blocks have been added and will create a large game save file)

Change the worldgen:

    /worldgen [checkerboard,city1,worldgen1]

Change worldgen to the default:

    /worldgen

### Startup Options

PiWorld can be started with or without any options. You can also add a path to
a game save file, if it does not exist a new one will be created. When no file
is specified the game file at `$HOME/.piworld/my.piworld` will be used.

    ./piworld [options] [file]

Start in fullscreen mode:

    --fullscreen

Set what size to use when in fullscreen mode (use `xrandr` to check what sizes
your screen supports):

    --fullscreen-size WxH

Set the number of local splitscreen players (1 to 4):

    --players N

Set the time of day (N is a single number from 0 to 24):

    --time N

Show more information:

    --verbose

Set view distance (default is 5, on older Pi models this will be reduced to fit
GPU memory size, higher numbers will reduce performance):

    --view N

Set the window size:

    --window-size WxH

Set the window title:

    --window-title TITLE

Set the window position:

    --window-xy XxY

worldgen:

    --worldgen city1

### Chat Commands

    /goto [NAME]

Teleport to another user.
If NAME is unspecified, a random user is chosen.

    /list

Display a list of connected users.

    /offline [FILE]

Switch to offline mode.
FILE specifies the save file to use and defaults to "my.piworld".

    /online HOST [PORT]

Connect to the specified server.

    /pq P Q

Teleport to the specified chunk.

    /spawn

Teleport back to the spawn point.

### Sign Text Markup

Signs in PiWorld can be styled with a simple form of markup.

To change the size of sign text enter `\N` at the start of the sign text, where
`N` is a number from 0.1 to 8 with 1 being the standard font size.

The colour of following text can be changed with `\a`, `\#FFF` or `\#FFFFFF`,
where `a` is a letter from the list of following colours and `F` is a
hexadecimal number.

letter | colour
------ | ------
r | red
g | green
b | blue
o | orange
p | purple
y | yellow
c | cyan
m | magenta
l | black
w | white
s | silver
e | grey

Example:

    `\2 \g ON \r OFF

this will set the font size to twice the standard height `\2`, then set the
text colour to green `\g`, add `ON` to the sign text, then set text colour to
red `\r` and finally add `OFF` to the sign. The two words on the sign will be
split over two lines due to automatic wrapping to fit the size of a block.

### Screenshot

![Screenshot](https://github.com/pspiworld/piworld/wiki/images/piworld-build.png)

### Implementation Details

#### Terrain Generation

The terrain is generated using Simplex noise - a deterministic noise function
seeded based on position. So the world will always be generated the same way in
a given location.

The world is split up into 16x16 block chunks in the XZ plane (Y is up). This
allows the world to be “infinite” (floating point precision is currently a
problem at large X or Z values) and also makes it easier to manage the data.
Only visible chunks need to be queried from the database.

#### Rendering

Only exposed faces are rendered. This is an important optimization as the vast
majority of blocks are either completely hidden or are only exposing one or two
faces. Each chunk records a one-block width overlap for each neighboring chunk
so it knows which blocks along its perimeter are exposed.

Only visible chunks are rendered. A naive frustum-culling approach is used to
test if a chunk is in the camera’s view. If it is not, it is not rendered. This
results in a pretty decent performance improvement as well.

Chunk buffers are completely regenerated when a block is changed in that chunk,
instead of trying to update the VBO.

Text is rendered using a bitmap atlas. Each character is rendered onto two
triangles forming a 2D rectangle.

“Modern” OpenGL is used - no deprecated, fixed-function pipeline functions are
used. Vertex buffer objects are used for position, normal and texture
coordinates. Vertex and fragment shaders are used for rendering. Matrix
manipulation functions are in matrix.c for translation, rotation, perspective,
orthographic, etc. matrices. The 3D models are made up of very simple
primitives - mostly cubes and rectangles. These models are generated in code in
cube.c.

Transparency in glass blocks and plants (plants don’t take up the full
rectangular shape of their triangle primitives) is implemented by discarding
magenta-colored pixels in the fragment shader.

#### Database

User changes to the world are stored in a sqlite database. Only the delta is
stored, so the default world is generated and then the user changes are applied
on top when loading.

The main database table is named “block” and has columns p, q, x, y, z, w. (p,
q) identifies the chunk, (x, y, z) identifies the block position and (w)
identifies the block type. 0 represents an empty block (air).

In game, the chunks store their blocks in a hash map. An (x, y, z) key maps to
a (w) value.

The y-position of blocks are limited to 0 <= y < 256. The upper limit is mainly
an artificial limitation to prevent users from building unnecessarily tall
structures. Users are not allowed to destroy blocks at y = 0 to avoid falling
underneath the world.

#### Multiplayer

Multiplayer mode is implemented using plain-old sockets. A simple, ASCII,
line-based protocol is used. Each line is made up of a command code and zero or
more comma-separated arguments. The client requests chunks from the server with
a simple command: C,p,q,key. “C” means “Chunk” and (p, q) identifies the chunk.
The key is used for caching - the server will only send block updates that have
been performed since the client last asked for that chunk. Block updates (in
realtime or as part of a chunk request) are sent to the client in the format:
B,p,q,x,y,z,w. After sending all of the blocks for a requested chunk, the
server will send an updated cache key in the format: K,p,q,key. The client will
store this key and use it the next time it needs to ask for that chunk. Player
positions are sent in the format: P,pid,x,y,z,rx,ry. The pid is the player ID
and the rx and ry values indicate the player’s rotation in two different axes.
The client interpolates player positions from the past two position updates for
smoother animation. The client sends its position to the server at most every
0.1 seconds (less if not moving).

Client-side caching to the sqlite database can be performance intensive when
connecting to a server for the first time. For this reason, sqlite writes are
performed on a background thread. All writes occur in a transaction for
performance. The transaction is committed every 5 seconds as opposed to some
logical amount of work completed. A ring / circular buffer is used as a queue
for what data is to be written to the database.

In multiplayer mode, players can observe one another in the main view or in a
picture-in-picture view. Implementation of the PnP was surprisingly simple -
just change the viewport and render the scene again from the other player’s
point of view.

#### Collision Testing

Hit testing (what block the user is pointing at) is implemented by scanning a
ray from the player’s position outward, following their sight vector. This is
not a precise method, so the step rate can be made smaller to be more accurate.

Collision testing simply adjusts the player’s position to remain a certain
distance away from any adjacent blocks that are obstacles. (Clouds and plants
are not marked as obstacles, so you pass right through them.)

#### Sky Dome

A textured sky dome is used for the sky. The X-coordinate of the texture
represents time of day. The Y-values map from the bottom of the sky sphere to
the top of the sky sphere. The player is always in the center of the sphere.
The fragment shaders for the blocks also sample the sky texture to determine
the appropriate fog color to blend with based on the block’s position relative
to the backing sky.

#### Ambient Occlusion

Ambient occlusion is implemented as described on this page:

http://0fps.wordpress.com/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/

#### Dependencies

* pg is used for GLES interfacing for VC4(Raspberry Pi) and X11 event handling.
* lodepng is used for loading PNG textures.
* sqlite3 is used for saving the blocks added / removed by the user.
* tinycthread is used for cross-platform threading.

