
local height = 11
local color1 = COLOR_16
local color2 = COLOR_29

function worldgen(p, q)
    local pad = 1
    local land = color1
    if (p % 2) == (q % 2) then
        land = color2
    end
    for dx = -pad, CHUNK_SIZE + pad do
        for dz = -pad, CHUNK_SIZE + pad do
            local flag = 1
            if dx < 0 or dz < 0 or dx >= CHUNK_SIZE or dz >= CHUNK_SIZE then
                flag = -1
            end
            local x = p * CHUNK_SIZE + dx
            local z = q * CHUNK_SIZE + dz
            map_set(x, 0, z, BEDROCK * flag)
            for y = 1, height do
                map_set(x, y, z, land * flag)
            end
        end
    end
end
