script_name('RouteLine')
script_properties('work-in-pause')
local ffi = require 'ffi'
local imgui = require 'mimgui'
local jni = require 'android.jnienv-util'
local env = require 'android.jnienv'
ffi.cdef[[ void rl_enable(int e); int rl_nodes_load(const char* p); int rl_nav(float px,float py,float pz,float tx,float ty,float tz); void rl_route_clear(); void rl_route_style(float w,int r,int g,int b); int rl_route_n(); int rl_route_pos(); const float* rl_route_ptr(); ]]

local SO_URL = 'https://github.com/brruham-arch/radarroute/releases/latest/download/libroute2d.so'
local SO_SIZE = 3477264
local WD = getWorkingDirectory()
local SRC = WD .. '/libroute2d.so'
local TMP = WD .. '/libroute2d.tmp'
local parts = {}
for p in string.gmatch(WD, '([^/\\]+)') do parts[#parts + 1] = p end
local DST = '/data/data/' .. parts[#parts - 1] .. '/libroute2d.so'
local L
local RC, W, on = { 0, 207, 255 }, 4, true
local A = { t = 0, has = false, x = 0, y = 0 }
local got, lastQ = 0, 0

local function msg(t) sampAddChatMessage('[RL] ' .. t, 0x00FF88) end
local function progress()
    local q = math.floor(got / SO_SIZE * 4)
    if q > lastQ and q < 4 then lastQ = q; msg('unduh ' .. q * 25 .. '%') end
end
local function validSo(path)
    local f = io.open(path, 'rb'); if not f then return false end
    local h = f:read(4); local sz = f:seek('end') or 0; f:close()
    return h == '\127ELF' and sz > 100000
end

-- unduh: curl kalau ada, fallback JNI
local CURL_BIN
for _, p in ipairs({ '/system/bin/curl', '/system/xbin/curl' }) do
    local f = io.open(p, 'r'); if f then CURL_BIN = p; f:close(); break end
end
local function execShellAsync(cmd)
    local rt = jni.CallStaticObjectMethod('java/lang/Runtime', 'getRuntime', '()Ljava/lang/Runtime;')
    local arr = env.NewObjectArray(3, jni.FindClass('java/lang/String'), nil)
    env.SetObjectArrayElement(arr, 0, env.NewStringUTF('sh'))
    env.SetObjectArrayElement(arr, 1, env.NewStringUTF('-c'))
    env.SetObjectArrayElement(arr, 2, env.NewStringUTF(cmd))
    jni.CallObjectMethod(rt, 'exec', '([Ljava/lang/String;)Ljava/lang/Process;', arr)
end
local function curlFetch(url, out)
    local done = out .. '.done'; os.remove(done); os.remove(out)
    execShellAsync(string.format("%s -s -L -f -o '%s' '%s'; echo $? > '%s'", CURL_BIN, out, url, done))
    local waited = 0
    while waited < 120000 do
        wait(200); waited = waited + 200
        local sf = io.open(out, 'rb'); if sf then got = sf:seek('end') or 0; sf:close(); progress() end
        local df = io.open(done, 'r')
        if df then
            local code = (df:read('*a') or ''):match('^%s*(.-)%s*$'); df:close(); os.remove(done)
            if code ~= '0' then return nil, 'curl exit ' .. code end
            return true
        end
    end
    return nil, 'timeout'
end
local function jniDownload(url)
    local u = jni.CallConstructor('java/net/URL', '(Ljava/lang/String;)V', env.NewStringUTF(url))
    local conn = jni.CallObjectMethod(u, 'openConnection', '()Ljava/net/URLConnection;')
    jni.CallVoidMethod(conn, 'setConnectTimeout', '(I)V', 8000)
    jni.CallVoidMethod(conn, 'setReadTimeout', '(I)V', 15000)
    local code = jni.CallIntMethod(conn, 'getResponseCode', '()I')
    if code ~= 200 then return nil, 'HTTP ' .. tostring(code) end
    local stream = jni.CallObjectMethod(conn, 'getInputStream', '()Ljava/io/InputStream;')
    local CH = 8192
    local jbuf, native, chunks = env.NewByteArray(CH), ffi.new('uint8_t[?]', CH), {}
    while true do
        local n = jni.CallIntMethod(stream, 'read', '([B)I', jbuf)
        if n == nil or n < 0 then break end
        if n > 0 then env.GetByteArrayRegion(jbuf, 0, n, native); chunks[#chunks + 1] = ffi.string(native, n); got = got + n; progress() end
        wait(0)
    end
    jni.CallVoidMethod(stream, 'close', '()V')
    local d = table.concat(chunks); if #d == 0 then return nil, '0 byte diterima' end
    return d
end
local function fetchBytes(url)
    got, lastQ = 0, 0
    if CURL_BIN then
        local ok, err = curlFetch(url, TMP); if not ok then return nil, err end
        local f = io.open(TMP, 'rb'); local d = f and f:read('*a'); if f then f:close() end; os.remove(TMP)
        if not d or #d == 0 then return nil, '0 byte diterima' end
        return d
    end
    local ok, d, err = pcall(jniDownload, url); if not ok then return nil, tostring(d) end
    return d, err
end

local function ensureSo()
    if validSo(SRC) then return true end
    msg('libroute2d.so belum ada, mengunduh...')
    local d, err = fetchBytes(SO_URL)
    if not d then return nil, err end
    if d:sub(1, 4) ~= '\127ELF' then return nil, 'bukan file .so (cek repo publik / release v1)' end
    local o = io.open(SRC, 'wb'); if not o then return nil, 'gagal tulis ' .. SRC end
    o:write(d); o:close()
    msg('unduh selesai ' .. #d .. ' byte')
    return true
end
local function loadLib()
    local f = io.open(SRC, 'rb'); if not f then return nil, 'file hilang' end
    local d = f:read('*a'); f:close()
    local o = io.open(DST, 'wb'); if o then o:write(d); o:close() end
    local ok, lib = pcall(ffi.load, DST)
    if not ok then return nil, tostring(lib) end
    return lib
end

-- pause map
pcall(ffi.cdef, 'void* dlopen(const char*,int); void* dlsym(void*,const char*);')
local GM
do
    local ok, p = pcall(function() return ffi.C.dlsym(ffi.C.dlopen('libGTASA.so', 2), 'gMobileMenu') end)
    if ok and p ~= nil then GM = ffi.cast('uint8_t*', p) end
end
local function fullMap() return GM ~= nil and ffi.cast('uint32_t*', GM + 0x24)[0] == 1 end

imgui.OnFrame(function() return on and L ~= nil and fullMap() and L.rl_route_n() > 0 end, function(self)
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
    wait(1000)
    local ok, err = ensureSo()
    if not ok then msg('gagal unduh: ' .. tostring(err)); return end
    local lib, lerr = loadLib()
    if not lib then msg('gagal load .so: ' .. tostring(lerr)); return end
    L = lib
    pcall(function() L.rl_nodes_load('') end)
    L.rl_route_style(W, RC[1], RC[2], RC[3])
    sampRegisterChatCommand('rl', function()
        on = not on; L.rl_enable(on and 1 or 0)
        if not on then L.rl_route_clear(); A.has = false end
        msg(on and 'ON' or 'OFF')
    end)
    sampRegisterChatCommand('rlcol', function(p)
        local r, g, b = (p or ''):match('(%d+)%s+(%d+)%s+(%d+)')
        if r then RC = { tonumber(r), tonumber(g), tonumber(b) }; L.rl_route_style(W, RC[1], RC[2], RC[3]) end
    end)
    sampRegisterChatCommand('rlw', function(p)
        local w = tonumber(p or ''); if w then W = w; L.rl_route_style(W, RC[1], RC[2], RC[3]) end
    end)
    sampRegisterChatCommand('rlupdate', function()
        os.remove(SRC); msg('.so lama dihapus, restart game untuk unduh versi terbaru')
    end)
    msg('/rl  /rlcol r g b  /rlw lebar  /rlupdate')
    while true do
        wait(0)
        if on and os.clock() - A.t > 0.5 then
            A.t = os.clock()
            local ex, tx, ty, tz = getTargetBlipCoordinates()
            if ex then
                if not A.has or math.abs(tx - A.x) > 1 or math.abs(ty - A.y) > 1 then
                    A.has, A.x, A.y = true, tx, ty
                    local px, py, pz = getCharCoordinates(PLAYER_PED)
                    L.rl_nav(px, py, pz, tx, ty, tz)
                end
            elseif A.has then
                A.has = false; L.rl_route_clear()
            end
        end
    end
end
