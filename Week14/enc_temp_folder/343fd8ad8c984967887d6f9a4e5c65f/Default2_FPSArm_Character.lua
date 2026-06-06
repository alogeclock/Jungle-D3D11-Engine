local arms   = nil
local camera = nil
local debugWidget = nil
local movement = nil
local DamagePostProcess = require("DamagePostProcess")
local InGamePause = require("InGamePause")
local WeaponHud = require("HUD/WeaponHud")
local ItemInspectSystem = require("Items/ItemInspectSystem")

-- 본 기반 머즐은 Arm_R 이 어깨 관절이라 카메라랑 거의 같은 위치 → 의미 없음.
-- 대부분 FPS 는 카메라에서 forward offset 으로 머즐을 근사함.
local MUZZLE_FWD_OFFSET = 0.4   -- 카메라 앞 0.4m 지점을 머즐로 간주

local MAX_RANGE        = 200.0
local FIRE_INTERVAL    = 0.12
local MAGAZINE_SIZE    = 50
local RELOAD_DURATION  = 1.1

local SPREAD_PER_SHOT       = 0.28
local SPREAD_DECAY_PER_SEC  = 1.15
local MAX_SPREAD            = 1.0
local MAX_SPREAD_ANGLE      = 0.075

local MAX_HITS_FOR_FULL_RED = 8
local MAX_HEALTH = 100.0
local DEBUG_Z_ORDER = 220
local DEBUG_SPEED_STEP = 0.25
local DEBUG_DAMAGE_STEP = 5.0
local DEBUG_HEALTH_STEP = 5.0

local fireCooldown = 0.0
local reloadTimer  = 0.0
local currentAmmo  = MAGAZINE_SIZE
local isReloading  = false
local hitCounts    = {}
local weaponSpread = 0.0
local currentHealth = MAX_HEALTH
local isInvincible = false
local debugSpeedMultiplier = 1.0
local debugDamageAmount = 10.0
local debugActiveTab = "player"
local debugWindowX = 40.0
local debugWindowY = 48.0
local debugDragging = false
local debugDragOffsetX = 0.0
local debugDragOffsetY = 0.0
local baseMaxWalkSpeed = nil
local baseSprintSpeedMultiplier = nil
local baseWallRunMaxSpeed = nil

local function px(value)
    return string.format("%.2fpx", value)
end

local function format_number(value, decimals)
    return string.format("%." .. tostring(decimals) .. "f", value)
end

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function get_health_ratio()
    if MAX_HEALTH <= 0.0 then return 0.0 end
    return clamp(currentHealth / MAX_HEALTH, 0.0, 1.0)
end

local function apply_player_damage(amount)
    amount = amount or 0.0
    if amount <= 0.0 then return end
    if isInvincible then return end

    currentHealth = clamp(currentHealth - amount, 0.0, MAX_HEALTH)
    local ratio = get_health_ratio()
    DamagePostProcess.TriggerHit(ratio)
    DamagePostProcess.SetHealthRatio(ratio)
end

local function set_player_health(value)
    currentHealth = clamp(value, 0.0, MAX_HEALTH)
    DamagePostProcess.SetHealthRatio(get_health_ratio())
end

local function cache_movement_defaults()
    if obj == nil or type(obj.GetCharacterMovement) ~= "function" then
        movement = nil
        return
    end

    movement = obj:GetCharacterMovement()
    if movement == nil then return end

    if baseMaxWalkSpeed == nil then
        baseMaxWalkSpeed = movement:GetMaxWalkSpeed()
        baseSprintSpeedMultiplier = movement:GetSprintSpeedMultiplier()
        baseWallRunMaxSpeed = movement:GetWallRunMaxSpeed()
    end
end

local function apply_debug_speed_multiplier()
    cache_movement_defaults()
    if movement == nil or baseMaxWalkSpeed == nil then return end

    movement:SetMaxWalkSpeed(baseMaxWalkSpeed * debugSpeedMultiplier)
    movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
    movement:SetWallRunMaxSpeed(baseWallRunMaxSpeed * debugSpeedMultiplier)
end

local function set_debug_text(id, text)
    if debugWidget == nil then return end
    debugWidget:SetText(id, text)
end

local function set_debug_property(id, property, value)
    if debugWidget == nil then return end
    debugWidget:SetProperty(id, property, value)
end

local function update_debug_values()
    if debugWidget == nil then return end

    set_debug_text("debug-speed-value", format_number(debugSpeedMultiplier, 2) .. "x")
    set_debug_text("debug-invincible-toggle", isInvincible and "ON" or "OFF")
    set_debug_text("debug-damage-value", tostring(math.floor(debugDamageAmount + 0.5)))
    set_debug_text("debug-health-value", tostring(math.floor(currentHealth + 0.5)))
    set_debug_text("debug-health-label", "Health " .. tostring(math.floor(get_health_ratio() * 100.0 + 0.5)) .. "%")
    set_debug_property("debug-invincible-toggle", "color", isInvincible and "#111111" or "#d8d8d8")
    set_debug_property("debug-invincible-toggle", "background-color", isInvincible and "#ffffff" or "#ffffff1F")
end

local function update_debug_tab_visuals()
    if debugWidget == nil then return end

    local tabs = { "player", "world", "ai", "render", "audio" }
    for _, tab in ipairs(tabs) do
        local selected = tab == debugActiveTab
        set_debug_property("debug-tab-" .. tab, "color", selected and "#ffffff" or "#808080")
        set_debug_property("debug-panel-" .. tab, "display", selected and "block" or "none")
    end
end

local function set_debug_tab(tab)
    debugActiveTab = tab
    update_debug_tab_visuals()
end

local function set_debug_window_position(x, y)
    debugWindowX = clamp(x, 0.0, 1600.0)
    debugWindowY = clamp(y, 0.0, 900.0)
    set_debug_property("debug-window", "left", px(debugWindowX))
    set_debug_property("debug-window", "top", px(debugWindowY))
end

local function bind_debug_events()
    if debugWidget == nil then return end

    debugWidget:bind_click("debug-close", function()
        if debugWidget ~= nil and debugWidget:IsInViewport() then
            debugWidget:RemoveFromParent()
            debugWidget:SetWantsMouse(false)
        end
    end)

    debugWidget:bind_click("debug-tab-player", function() set_debug_tab("player") end)
    debugWidget:bind_click("debug-tab-world", function() set_debug_tab("world") end)
    debugWidget:bind_click("debug-tab-ai", function() set_debug_tab("ai") end)
    debugWidget:bind_click("debug-tab-render", function() set_debug_tab("render") end)
    debugWidget:bind_click("debug-tab-audio", function() set_debug_tab("audio") end)

    debugWidget:bind_click("debug-speed-minus", function()
        debugSpeedMultiplier = clamp(debugSpeedMultiplier - DEBUG_SPEED_STEP, 0.25, 8.0)
        apply_debug_speed_multiplier()
        update_debug_values()
    end)
    debugWidget:bind_click("debug-speed-plus", function()
        debugSpeedMultiplier = clamp(debugSpeedMultiplier + DEBUG_SPEED_STEP, 0.25, 8.0)
        apply_debug_speed_multiplier()
        update_debug_values()
    end)
    debugWidget:bind_click("debug-invincible-toggle", function()
        isInvincible = not isInvincible
        update_debug_values()
    end)
    debugWidget:bind_click("debug-damage-minus", function()
        debugDamageAmount = clamp(debugDamageAmount - DEBUG_DAMAGE_STEP, 0.0, 100.0)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-damage-plus", function()
        debugDamageAmount = clamp(debugDamageAmount + DEBUG_DAMAGE_STEP, 0.0, 100.0)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-apply-damage", function()
        apply_player_damage(debugDamageAmount)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-health-minus", function()
        set_player_health(currentHealth - DEBUG_HEALTH_STEP)
        update_debug_values()
    end)
    debugWidget:bind_click("debug-health-plus", function()
        set_player_health(currentHealth + DEBUG_HEALTH_STEP)
        update_debug_values()
    end)

    debugWidget:bind_event("debug-titlebar", "mousedown", function(event)
        debugDragging = true
        debugDragOffsetX = event.mouse_x - debugWindowX
        debugDragOffsetY = event.mouse_y - debugWindowY
    end)
    debugWidget:bind_event("debug-titlebar", "mouseup", function()
        debugDragging = false
    end)
    debugWidget:bind_event("debug-window", "mouseup", function()
        debugDragging = false
    end)
    debugWidget:bind_event("debug-window", "mousemove", function(event)
        if not debugDragging then return end
        set_debug_window_position(event.mouse_x - debugDragOffsetX, event.mouse_y - debugDragOffsetY)
    end)
end

local function ensure_debug_widget()
    if debugWidget ~= nil then return end

    debugWidget = UI.CreateWidget("Content/UI/DebugUI/InGameDebug.rml")
    if debugWidget == nil then return end

    debugWidget:SetWantsMouse(false)
    bind_debug_events()
    update_debug_values()
    update_debug_tab_visuals()
    set_debug_window_position(debugWindowX, debugWindowY)
end

local function toggle_debug_widget()
    ensure_debug_widget()
    if debugWidget == nil then return end

    if debugWidget:IsInViewport() then
        debugWidget:RemoveFromParent()
        debugWidget:SetWantsMouse(false)
        debugDragging = false
    else
        debugWidget:AddToViewportZ(DEBUG_Z_ORDER)
        debugWidget:SetWantsMouse(true)
        update_debug_values()
        update_debug_tab_visuals()
        set_debug_window_position(debugWindowX, debugWindowY)
    end
end

local function get_anim_requests()
    if obj == nil then return nil end

    _G.FPSArmAnimRequests = _G.FPSArmAnimRequests or {}

    local ownerId = obj.UUID
    local requests = _G.FPSArmAnimRequests[ownerId]
    if requests == nil then
        requests = {}
        _G.FPSArmAnimRequests[ownerId] = requests
    end

    return requests
end

local function request_arm_animation(name)
    local requests = get_anim_requests()
    if requests == nil then return end

    requests[name] = true
end

local function update_weapon_hud()
    WeaponHud.SetAmmo(currentAmmo, MAGAZINE_SIZE)
end

local function is_target_actor(actor)
    return actor ~= nil and actor:GetSkeletalMesh() ~= nil
end

local function finish_reload()
    currentAmmo = MAGAZINE_SIZE
    reloadTimer = 0.0
    isReloading = false
    update_weapon_hud()
end

local function start_reload()
    if arms == nil or isReloading or currentAmmo >= MAGAZINE_SIZE then return end

    isReloading = true
    reloadTimer = RELOAD_DURATION
    request_arm_animation("reload")
end

local function lerp_red(t)
    if t > 1.0 then t = 1.0 end
    -- 옅은 빨강 (255,180,180) → 진한 빨강 (180,0,0)
    local r = math.floor(255 - (255 - 180) * t)
    local g = math.floor(180 * (1 - t))
    local b = math.floor(180 * (1 - t))
    return r, g, b
end

local function register_hit(hitActor, hitLoc)
    local id = hitActor.UUID
    local n  = (hitCounts[id] or 0) + 1
    hitCounts[id] = n
    local r, g, b = lerp_red(n / MAX_HITS_FOR_FULL_RED)
    Debug.DrawSphere(hitLoc, 0.08, r, g, b, 9999.0, 10)

    if n == MAX_HITS_FOR_FULL_RED then
        WeaponHud.TriggerKillMarker()
    elseif n < MAX_HITS_FOR_FULL_RED then
        WeaponHud.TriggerHitMarker()
    else
        return
    end
end

local function get_fire_direction(camFwd)
    if Input.GetKey(Key.MouseRight) or weaponSpread <= 0.0 then
        return camFwd
    end

    local camRight = camera.Right
    local camUp = camera.Up
    local angle = math.random() * math.pi * 2.0
    local radius = math.sqrt(math.random()) * weaponSpread * MAX_SPREAD_ANGLE
    local spreadDir = camFwd + camRight * (math.cos(angle) * radius) + camUp * (math.sin(angle) * radius)
    spreadDir:Normalize()
    return spreadDir
end

local function try_shoot()
    if camera == nil or arms == nil then return end
    if isReloading then return end
    if currentAmmo <= 0 then
        start_reload()
        return
    end

    currentAmmo = currentAmmo - 1
    update_weapon_hud()

    request_arm_animation("shoot")

    local camPos = camera:GetWorldLocation()
    local camFwd = camera.Forward
    local fireDir = get_fire_direction(camFwd)

    -- 카메라 + forward offset 을 머즐로 간주 (시안색 시각화).
    local muzzleWorld = Vector.new(
        camPos.X + camFwd.X * MUZZLE_FWD_OFFSET,
        camPos.Y + camFwd.Y * MUZZLE_FWD_OFFSET,
        camPos.Z + camFwd.Z * MUZZLE_FWD_OFFSET)
    Debug.DrawSphere(muzzleWorld, 0.04, 0, 200, 255, 1.0, 8)

    -- 카메라 → 머즐 사이에 벽이 있으면 (= 총구가 벽 안으로 박힘) 시작점 클램프.
    local rayStart = muzzleWorld
    local obstrHit = World.RaycastWorldStatic(camPos, fireDir, MUZZLE_FWD_OFFSET, obj)
    if obstrHit ~= nil then
        rayStart = obstrHit.WorldHitLocation
        Debug.DrawSphere(rayStart, 0.05, 255, 230, 0, 1.0, 8)
    end

    -- 벽 우선: 카메라 forward 방향으로 WorldStatic 트레이스 → 닿는 곳까지만 SkeletalMesh 트레이스.
    local effRange = MAX_RANGE
    local wallHit  = World.RaycastWorldStatic(rayStart, fireDir, MAX_RANGE, obj)
    if wallHit ~= nil then
        effRange = wallHit.Distance
    end

    local hit = World.RaycastSkeletalMesh(rayStart, fireDir, effRange, obj)

    local endPos
    if hit ~= nil then
        endPos = hit.WorldHitLocation
    elseif wallHit ~= nil then
        endPos = wallHit.WorldHitLocation
    else
        endPos = Vector.new(
            rayStart.X + fireDir.X * MAX_RANGE,
            rayStart.Y + fireDir.Y * MAX_RANGE,
            rayStart.Z + fireDir.Z * MAX_RANGE)
    end
    Debug.DrawLine(rayStart, endPos, 0, 255, 0, 1.5)

    if hit ~= nil and is_target_actor(hit.HitActor) then
        register_hit(hit.HitActor, hit.WorldHitLocation)
    end

    if not Input.GetKey(Key.MouseRight) then
        weaponSpread = weaponSpread + SPREAD_PER_SHOT
        if weaponSpread > MAX_SPREAD then weaponSpread = MAX_SPREAD end
        WeaponHud.SetSpread(weaponSpread)
    end
end

function BeginPlay()
    arms   = obj:GetSkeletalMesh()
    camera = obj:GetCamera()
    movement = nil
    baseMaxWalkSpeed = nil
    baseSprintSpeedMultiplier = nil
    baseWallRunMaxSpeed = nil
    cache_movement_defaults()
    currentAmmo = MAGAZINE_SIZE
    currentHealth = MAX_HEALTH
    isInvincible = false
    debugSpeedMultiplier = 1.0
    debugDamageAmount = 10.0
    reloadTimer = 0.0
    isReloading = false
    fireCooldown = 0.0
    local requests = get_anim_requests()
    if requests ~= nil then
        requests.shoot = false
        requests.reload = false
    end
    weaponSpread = 0.0
    DamagePostProcess.Initialize()
    DamagePostProcess.Reset()
    DamagePostProcess.SetHealthRatio(get_health_ratio())
    InGamePause.Initialize()

    _G.ApplyPlayerDamage = apply_player_damage
    _G.GetPlayerHealthRatio = get_health_ratio
    Engine.SetOnEscape(function()
        InGamePause.Toggle()
    end)

    WeaponHud.Initialize({
        camera = camera,
        owner = obj,
        maxRange = MAX_RANGE,
        currentAmmo = currentAmmo,
        magazineSize = MAGAZINE_SIZE,
        weaponSpread = weaponSpread,
        zOrder = 80,
    })
    ItemInspectSystem.Initialize()
end

function EndPlay()
    if obj ~= nil and _G.FPSArmAnimRequests ~= nil then
        _G.FPSArmAnimRequests[obj.UUID] = nil
    end

    WeaponHud.Shutdown()
    ItemInspectSystem.Shutdown()
    DamagePostProcess.Shutdown()
    InGamePause.Shutdown()
    Engine.SetOnEscape(function() end)
    if _G.ApplyPlayerDamage == apply_player_damage then
        _G.ApplyPlayerDamage = nil
    end
    if _G.GetPlayerHealthRatio == get_health_ratio then
        _G.GetPlayerHealthRatio = nil
    end
    if debugWidget ~= nil and debugWidget:IsInViewport() then
        debugWidget:RemoveFromParent()
    end
    debugWidget = nil
    if movement ~= nil and baseMaxWalkSpeed ~= nil then
        movement:SetMaxWalkSpeed(baseMaxWalkSpeed)
        movement:SetSprintSpeedMultiplier(baseSprintSpeedMultiplier)
        movement:SetWallRunMaxSpeed(baseWallRunMaxSpeed)
    end
    movement = nil
end

function Tick(dt)
    if arms == nil then return end

    if Input.GetKeyDown(Key.F10) then
        toggle_debug_widget()
    end

    if fireCooldown > 0 then
        fireCooldown = fireCooldown - dt
    end

    if weaponSpread > 0.0 then
        weaponSpread = weaponSpread - SPREAD_DECAY_PER_SEC * dt
        if weaponSpread < 0.0 then weaponSpread = 0.0 end
    end

    if Input.GetKey(Key.MouseRight) then
        weaponSpread = 0.0
    end

    if isReloading then
        reloadTimer = reloadTimer - dt
        if reloadTimer <= 0 then
            finish_reload()
        end
    end

    DamagePostProcess.Tick(dt)
    InGamePause.Tick()
    WeaponHud.Tick(dt, weaponSpread)
    apply_debug_speed_multiplier()
    if debugWidget ~= nil and debugWidget:IsInViewport() then
        update_debug_values()
    end

    if InGamePause.IsOpen() then
        return
    end

    ItemInspectSystem.Tick(camera, obj)
    if ItemInspectSystem.IsOpen() then
        return
    end

    if Input.GetKeyDown(Key.R) then
        start_reload()
    end

    if Input.GetKey(Key.MouseLeft) and fireCooldown <= 0 then
        try_shoot()
        fireCooldown = FIRE_INTERVAL
    end

end
