--[[
Produces the same world as the default C implementation.

Handy for performance comparisons between C and Lua worldgens:

  $ time ./piworld --benchmark-create-chunks 256
  $ time ./piworld --benchmark-create-chunks 256 --worldgen worldgen/worldgen1.lua
--]]

function worldgen(p, q)
    local pad = 1
    for dx = -pad, CHUNK_SIZE-1 + pad do
        for dz = -pad, CHUNK_SIZE-1 + pad do
            local flag = 1
            if dx < 0 or dz < 0 or dx >= CHUNK_SIZE or dz >= CHUNK_SIZE then
                flag = -1
            end
            local x = p * CHUNK_SIZE + dx
            local z = q * CHUNK_SIZE + dz
            local f = simplex2(x * 0.01, z * 0.01, 4, 0.5, 2)
            local g = simplex2(-x * 0.01, -z * 0.01, 2, 0.9, 2)
            local mh = g * 32 + 16
            local h = f * mh
            local w = GRASS
            local t = 12
            if h < t+1 then
                h = t
                w = SAND
            end

            map_set(x, 0, z, BEDROCK * flag)

            -- sand and grass terrain
            for y = 1, h-1 do
                map_set(x, y, z, w * flag)
            end
            if w == GRASS then
                -- grass
                if simplex2(-x * 0.1, z * 0.1, 4, 0.8, 2) > 0.6 then
                    map_set(x, h, z, TALL_GRASS * flag)
                end
                -- flowers
                if simplex2(x * 0.05, -z * 0.05, 4, 0.8, 2) > 0.7 then
                    local w = 18 + simplex2(x * 0.1, z * 0.1, 4, 0.8, 2) * 7
                    map_set(x, h, z, w * flag)
                end

                -- trees
                local ok = true
                if dx - 4 < 0 or dz - 4 < 0 or dx + 4 >= CHUNK_SIZE or dz + 4 >= CHUNK_SIZE then
                    ok = false
                end
                if ok and simplex2(x, z, 6, 0.5, 2) > 0.84 then
                    -- leaves
                    for y = h + 3, h + 8 - 1 do
                        for ox = -3, 3 do
                            for oz = -3, 3 do
                                local d = (ox * ox) + (oz * oz) + (y - (h + 4)) * (y - (h + 4))
                                if d < 11 then
                                    map_set(x + ox, y, z + oz, LEAVES)
                                end
                            end
                        end
                    end
                    -- tree trunk
                    for y = h, h + 6 do
                        map_set(x, y, z, WOOD)
                    end
                end
            end

            -- Clouds
            for y = 64, 72-1 do
                if simplex3(x * 0.01, y * 0.1, z * 0.01, 8, 0.5, 2) > 0.75 then
                    map_set(x, y, z, 16 * flag)
                end
            end
        end
    end
end
