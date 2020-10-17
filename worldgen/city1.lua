--[[
City 1 - a world generator script for piworld

City 1 is split into two zones types - each with a single building. One of
the zones contains the roads. The zones are laid out such that the roads
surround the buildings.

The zones containing roads surround the full-sized building zones, so that
roads will be interconnected.

To test this worldgen script:

  $ ./piworld ./city1-test.piworld --worldgen worldgen/city1.lua
--]]

-- General worldgen options
local height = 12
local zone_size = 32
local land = SAND

-- Zone 1 options - containing a multi-floor building
local wall_height1 = 20
local floor_color1 = LIGHT_STONE
local stair_texture = DARK_STONE
local ground_floor_color = COLOR_29
local floor_height = 5
local staircase_height = height + wall_height1 - floor_height
local step_width = 2
local staircase_x_length = floor_height * 2 - 2
local staircase_z_length = step_width * 2

-- Zone 2 options - containing a single floor building with roads
local flat_building_height = 3
local x_length2 = zone_size / 2
local z_length2 = zone_size / 2
local x_corner_offset2 = x_length2 / 2
local z_corner_offset2 = z_length2 / 2
local road = COLOR_09
local pavement = STONE
local central_divider_z = "\\8 \\w -"

function multi_floor_building(x, z, flag, x_corner, z_corner)
    -- A multi-floor building with central staircase, doors on ground floor,
    -- windows on every floor.

    -- Walls
    if (x == x_corner+1 or z == z_corner+1 or x == x_corner + zone_size - 1 or
        z == z_corner + zone_size - 1)
       then
        -- Outer wall
        for y = height, height + (wall_height1 - 1) do
            map_set(x, y, z, CEMENT * flag)
        end
        -- Doors
        if x == x_corner + (zone_size / 2) or z == z_corner + (zone_size / 2) then
            map_set(x, height, z, GLASS * flag)
            map_set_shape(x, height, z, LOWER_DOOR * flag)
            map_set(x, height + 1, z, GLASS * flag)
            map_set_shape(x, height + 1, z, UPPER_DOOR * flag)
            -- Transform door to match the direction of the wall it's embedded in.
            local side = DOOR_X_PLUS
            if x == x_corner + (zone_size / 2) and z == z_corner + 1 then
                side = DOOR_Z
            elseif x == x_corner + (zone_size / 2) and z == z_corner + zone_size - 1 then
                side = DOOR_Z_PLUS
            elseif x == x_corner + 1 and z == z_corner + (zone_size / 2) then
                side = DOOR_X
            end
            if side ~= 0 then
                map_set_transform(x, height, z, side * flag)
                map_set_transform(x, height + 1, z, side * flag)
            end
        -- Windows
        elseif (x >= x_corner + (zone_size / 4) and x <= x_corner + (zone_size / 4) + 3) or
               (x >= x_corner + (3 * zone_size / 4) - 3 and x <= x_corner + (3 * zone_size / 4)) or
               (z >= z_corner + (zone_size / 4) and z <= z_corner + (zone_size / 4) + 3) or
               (z >= z_corner + (3 * zone_size / 4) - 3 and z <= z_corner + (3 * zone_size / 4)) then
            for y = height + 1, height + (wall_height1 - 1), 5 do
                map_set(x, y, z, GLASS * flag)
            end
        end
    end

    -- Floors and stairs
    local staircase_x = x_corner + math.floor(zone_size/2) - math.floor(staircase_x_length/2)
    local staircase_z = z_corner + math.floor(zone_size/2) - math.floor(staircase_z_length/2)
    if x >= staircase_x and x <= staircase_x + staircase_x_length and
       z > staircase_z and z <= staircase_z + staircase_z_length then
        -- Stairwell in centre of building
        local y_offset
        if z > staircase_z and z <= staircase_z + step_width then
            -- -z side staircase: ground floor then every other floor
            y_offset = math.floor((x - staircase_x) / 2)
        else
            -- +z side staircase: 1st floor then every other floor
            y_offset = math.floor((staircase_x - x) / 2) + floor_height*2 - 1
        end
        for y = height, staircase_height, floor_height*2 do
            map_set(x, y + y_offset, z, stair_texture * flag)
            if (x % 2) == 0 then
                -- Alternate between cube and slab8 shapes for a climbable
                -- staircase.
                map_set_shape(x, y + y_offset, z, SLAB8 * flag)
            end
        end
    else
        -- Solid floor
        for y = height, height + wall_height1, floor_height do
            map_set(x, y - 1, z, floor_color1 * flag)
        end
    end

    -- Ground Floor
    map_set(x, height - 1, z, ground_floor_color * flag)
end

function flat_building(x, z, flag, x_corner, z_corner)
    -- A single-floor building with doors and windows on each wall. The doors
    -- and floor are set to a random colour.

    -- Walls
    if x == x_corner or z == z_corner or x == x_corner + x_length2 or
       z == z_corner + z_length2 then
        -- Outer wall
        for y = height, height + (flat_building_height - 1) do
            map_set(x, y, z, BRICK * flag)
        end

        -- Doors
        if x == x_corner + (x_length2 / 2) or z == z_corner + (z_length2 / 2) then
            math.randomseed(x * z)
            local door_color = math.random(COLOR_01, COLOR_31)
            map_set(x, height, z, door_color * flag)
            map_set_shape(x, height, z, LOWER_DOOR * flag)
            map_set(x, height + 1, z, door_color * flag)
            map_set_shape(x, height + 1, z, UPPER_DOOR * flag)
            -- Transform door to match the direction of the wall it's embedded in.
            local side = DOOR_X_PLUS
            if x == x_corner + (x_length2 / 2) and z == z_corner then
                side = DOOR_Z
            elseif x == x_corner + (x_length2 / 2) and z == z_corner + z_length2 then
                side = DOOR_Z_PLUS
            elseif x == x_corner and z == z_corner + (z_length2 / 2) then
                side = DOOR_X
            end
            if side ~= 0 then
                map_set_transform(x, height, z, side * flag)
                map_set_transform(x, height + 1, z, side * flag)
            end
        -- Windows
        elseif (x >= x_corner + (x_length2 / 4) and x <= x_corner + (x_length2 / 4) + 2) or
               (x >= x_corner + (3 * x_length2 / 4) - 2 and x <= x_corner + (3 * x_length2 / 4)) or
               (z >= z_corner + (z_length2 / 4) and z <= z_corner + (z_length2 / 4) + 2) or
               (z >= z_corner + (3 * z_length2 / 4) - 2 and z <= z_corner + (3 * z_length2 / 4)) then
            map_set(x, height + 1, z, GLASS * flag)
        end
    end

    -- Roof
    local y = height + flat_building_height
    map_set(x, y, z, CEMENT * flag)
    map_set_shape(x, y, z, SLAB8 * flag)

    -- Floor
    y = height - 1
    math.randomseed(x_corner * z_corner)
    local floor_color = math.random(COLOR_01, COLOR_31)
    map_set(x, y, z, floor_color * flag)
end

function zone1(x, z, flag, x_zone, z_zone)
    -- Zone 1 contains a single multi-floor building.
    local x_corner = math.floor(x / zone_size) * zone_size
    local z_corner = math.floor(z / zone_size) * zone_size
    if (x > x_corner and x < x_corner + zone_size) and
       (z > z_corner and z < z_corner + zone_size) then
        -- Building
        multi_floor_building(x, z, flag, x_corner, z_corner)
    else
        -- Fill in the little bit of pavement outside the building.
        local y = height - 1
        map_set(x, y, z, pavement * flag)
    end
end

function zone2(x, z, flag, x_zone, z_zone)
    -- Zone 2 contains a single flat building and 2 parallel roads.
    local x_corner = math.floor(x / zone_size) * zone_size
    local z_corner = math.floor(z / zone_size) * zone_size
    local building_x_corner = x_corner + x_corner_offset2
    local building_z_corner = z_corner + z_corner_offset2

    if x >= building_x_corner and x <= building_x_corner + x_length2 and
       z >= building_z_corner and z <= building_z_corner + z_length2 then
        -- Building
        flat_building(x, z, flag, building_x_corner, building_z_corner)
    elseif (z_zone % 2 == 0 or x_zone % 2 ~= 0) and
           ((x >= x_corner + zone_size - 6 and x <= x_corner + zone_size - 2) or
            (x >= x_corner + 2 and x <= x_corner + 6))
           then
        -- Road
        --   Tarmac
        map_set(x, height - 1, z, road * flag)
        --   Central divider
        if (x == x_corner + zone_size - 4 or x == x_corner + 4) then
            map_set_sign(x, height - 1, z, 5, central_divider_z)
        end
    else
        -- Pavement
        map_set(x, height - 1, z, pavement * flag)
    end
end

function worldgen(p, q)
    local pad = 1
    for dx = -pad, CHUNK_SIZE + pad do
        for dz = -pad, CHUNK_SIZE + pad do
            local flag = 1
            if dx < 0 or dz < 0 or dx >= CHUNK_SIZE or dz >= CHUNK_SIZE then
                flag = -1
            end
            local x = p * CHUNK_SIZE + dx
            local z = q * CHUNK_SIZE + dz
            map_set(x, 0, z, BEDROCK * flag)

            for y = 1, height-1 do
                map_set(x, y, z, land * flag)
            end

            -- Determine what zone xz is in and build on it
            local x_zone = math.floor(x / zone_size)
            local z_zone = math.floor(z / zone_size)
            if x_zone % 2 == 0 and z_zone % 2 == 0 then
                zone1(x, z, flag, x_zone, z_zone)
            else
                zone2(x, z, flag, x_zone, z_zone)
            end
        end
    end
end
