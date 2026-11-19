script_name('RouteLine')
script_properties('work-in-pause')
local ffi = require 'ffi'
local imgui = require 'mimgui'
ffi.cdef[[ void rl_enable(int e); int rl_nodes_load(const char* p); int rl_nav(float px,float py,float pz,float tx,float ty,float tz); void rl_route_clear(); void rl_route_style(float w,int r,int g,int b); int rl_route_n(); int rl_route_pos(); const float* rl_route_ptr(); ]]
local WD = getWorkingDirectory()
local parts = {}
for p in string.gmatch(WD, '([^/\\]+)') do parts[#parts + 1] = p end
local dst = '/data/data/' .. parts[#parts - 1] .. '/libroute2d.so'
local f = io.open(WD .. '/libroute2d.so', 'rb')
if f then local d = f:read('*a'); f:close(); local o = io.open(dst, 'wb'); if o then o:write(d); o:close() end end
local L = ffi.load(dst)
local NODES = WD .. '/lib/nodes.bin'
local RC, W, on = { 0, 207, 255 }, 4, true
local A = { t = 0, has = false, x = 0, y = 0 }

pcall(ffi.cdef, 'void* dlopen(const char*,int); void* dlsym(void*,const char*);')
local GM
do
    local ok, p = pcall(function() return ffi.C.dlsym(ffi.C.dlopen('libGTASA.so', 2), 'gMobileMenu') end)
    if ok and p ~= nil then GM = ffi.cast('uint8_t*', p) end
end
local function fullMap() return GM ~= nil and ffi.cast('uint32_t*', GM + 0x24)[0] == 1 end

imgui.OnFrame(function() return on and fullMap() and L.rl_route_n() > 0 end, function(self)
    self.HideCursor = true
    local dl = imgui.GetBackgroundDrawList()
    local ax, ay = 2.4, 2.4
    local z = ffi.cast('float*', GM + 0x58)[0]
    local ox = ffi.cast('float*', GM + 0x5c)[0]
    local oy = ffi.cast('float*', GM + 0x60)[0]
    local n, c, p = L.rl_route_n(), L.rl_route_pos(), L.rl_route_ptr()
    local rc = 0xFF000000 + RC[3] * 65536 + RC[2] * 256 + RC[1]
    local px, py
    for i = c, n - 1 do
        local sx = (ox + p[2 * i] / 3000 * z) * ax
        local sy = (oy - p[2 * i + 1] / 3000 * z) * ay
        if px then dl:AddLine(imgui.ImVec2(px, py), imgui.ImVec2(sx, sy), rc, 5) end
        px, py = sx, sy
    end
    if px then dl:AddCircleFilled(imgui.ImVec2(px, py), 6, 0xFF2828FF) end
end)

function main()
    repeat wait(0) until isSampAvailable()
    pcall(function() L.rl_nodes_load(NODES) end)
    L.rl_route_style(W, RC[1], RC[2], RC[3])
    sampRegisterChatCommand('rl', function()
        on = not on; L.rl_enable(on and 1 or 0)
        if not on then L.rl_route_clear(); A.has = false end
        sampAddChatMessage('[RL] ' .. (on and 'ON' or 'OFF'), 0x00FF88)
    end)
    sampRegisterChatCommand('rlcol', function(p)
        local r, g, b = (p or ''):match('(%d+)%s+(%d+)%s+(%d+)')
        if r then RC = { tonumber(r), tonumber(g), tonumber(b) }; L.rl_route_style(W, RC[1], RC[2], RC[3]) end
    end)
    sampRegisterChatCommand('rlw', function(p)
        local w = tonumber(p or ''); if w then W = w; L.rl_route_style(W, RC[1], RC[2], RC[3]) end
    end)
    sampAddChatMessage('[RL] /rl  /rlcol r g b  /rlw lebar', 0x00FF88)
    while true do
        wait(0)
        if on and os.clock() - A.t > 0.5 then
            A.t = os.clock()
            local ex, tx, ty, tz = getTargetBlipCoordinates()
            if ex then
                if not A.has or math.abs(tx - A.x) > 1 or math.abs(ty - A.y) > 1 then
                    A.has, A.x, A.y = true, tx, ty
                    L.rl_nodes_load(NODES)
                    local px, py, pz = getCharCoordinates(PLAYER_PED)
                    L.rl_nav(px, py, pz, tx, ty, tz)
                end
            elseif A.has then
                A.has = false; L.rl_route_clear()
            end
        end
    end
end
