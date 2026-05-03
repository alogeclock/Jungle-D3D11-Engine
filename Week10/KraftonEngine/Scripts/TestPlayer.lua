-- ============================================================
-- TestPlayer.lua
--   WASD: 평면 이동, Space: 위로, LCtrl: 아래로
--   Q/E:  Yaw 회전
--   F:    한 번 누르면 위치 초기화
--
-- 사용법: 에디터에서 임의의 가시(visible) 액터(예: Cube StaticMeshActor)를
--        선택 → Details에 UScriptComponent 추가 → ScriptPath를
--        "Scripts/TestPlayer.lua"로 설정 → 씬 저장 → 패키징.
-- ============================================================

DeclareProperties({
    MoveSpeed = { type = "float", default = 5.0,  min = 0.1, max = 50.0 },
    TurnSpeed = { type = "float", default = 90.0, min = 1.0, max = 360.0 },
})

local _start = vec3(0, 0, 0)

function BeginPlay()
    -- 시작 위치 기억 — F 키로 복귀
    _start = obj.Location
    print("[TestPlayer] BeginPlay at " .. tostring(_start.x) .. ", " .. tostring(_start.y) .. ", " .. tostring(_start.z))
end

function Tick(dt)
    local speed = property("MoveSpeed", 5.0)
    local turn  = property("TurnSpeed", 90.0)

    -- 평면 이동 (월드 축 기준)
    local mx, my, mz = 0, 0, 0
    if GetKey("W") then my = my + 1 end
    if GetKey("S") then my = my - 1 end
    if GetKey("D") then mx = mx + 1 end
    if GetKey("A") then mx = mx - 1 end
    if GetKey("Space")    then mz = mz + 1 end
    if GetKey("LControl") then mz = mz - 1 end

    if mx ~= 0 or my ~= 0 or mz ~= 0 then
        obj:AddWorldOffset(mx * speed * dt, my * speed * dt, mz * speed * dt)
    end

    -- Yaw 회전 (Z축)
    local r = obj.Rotation
    if GetKey("E") then r.z = r.z + turn * dt end
    if GetKey("Q") then r.z = r.z - turn * dt end
    obj.Rotation = r

    -- 위치 리셋
    if GetKeyDown("F") then
        obj.Location = _start
        print("[TestPlayer] reset to start")
    end
end

function EndPlay()
    print("[TestPlayer] EndPlay")
end
