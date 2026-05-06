---@diagnostic disable: undefined-global, lowercase-global, unused-local

-- KraftonEngine Lua API 샘플
-- 최근 프로젝트에 존재하는 모든 Lua Reference 파일을 가져옴
-- It is safe to keep under Scripts/ as a reference file. By default it does not
-- run the mutating examples. Attach it to a ScriptComponent and turn RunDemo on
-- in Details only when you intentionally want to exercise the calls.

DeclareProperties({
    RunDemo = { type = "bool", default = false },
    SampleSpeed = { type = "float", default = 300.0, min = 0.0, max = 2000.0 },
    SampleInt = { type = "int", default = 3, min = 0, max = 10 },
    SampleName = { type = "string", default = "LuaSample" },
    SampleOffset = { type = "vector", default = vec3(10.0, 0.0, 0.0) },
    SampleFlag = { type = "bool", default = true },
})

local RUN_DEMO = property("RunDemo", false)
local SAMPLE_SPEED = property("SampleSpeed", 300.0)
local SAMPLE_INT = property("SampleInt", 3)
local SAMPLE_NAME = property("SampleName", "LuaSample")
local SAMPLE_OFFSET = property("SampleOffset", vec3(10.0, 0.0, 0.0))
local SAMPLE_FLAG = property("SampleFlag", true)

Sample = {}

local SAMPLE_SOUND = "Asset/Content/Sound/SFX/glitch_noise.wav"
local SAMPLE_BGM = "Asset/Content/Sound/BGM/main.mp3"
local SAMPLE_TEXTURE = "Asset/Content/Texture/OK.png"
local SAMPLE_MESH = "Asset/Content/Mesh/sample.obj"
local SAMPLE_JSON = "Saved/lua_sample_data.json"
local SAMPLE_SCENE = "Sample.Scene"

local function bool_text(value)
    return value and "true" or "false"
end

function Sample.OpenLibraryExamples()
    -- base library: assert, type, tostring, tonumber, pairs, ipairs, pcall, select.
    local as_number = tonumber("42") or 0
    local as_text = tostring(as_number)
    local value_type = type(as_text)
    assert(value_type == "string", "base library assert/type/tostring failed")

    local map_sum = 0
    for _, value in pairs({ a = 1, b = 2, c = 3 }) do
        map_sum = map_sum + value
    end

    local array_sum = 0
    for _, value in ipairs({ 4, 5, 6 }) do
        array_sum = array_sum + value
    end

    local argc = select("#", "a", "b", "c")
    local require_ok, config_or_error = pcall(require, "Game.Config")

    -- math library.
    local angle = math.pi * 0.25
    local math_value = (
        math.max(1, math.min(10, math.floor(math.sqrt(81))))
        + math.abs(math.sin(angle))
        + math.abs(math.cos(angle))
    )

    -- string library.
    local formatted = string.format("name=%s int=%d flag=%s", SAMPLE_NAME, SAMPLE_INT, bool_text(SAMPLE_FLAG))
    local lowered = string.lower(formatted)
    local uppered = string.upper(lowered)
    local replaced = string.gsub(uppered, "LUA", "Lua")
    local matched = string.match(replaced, "name=([%w_]+)") or "none"
    local prefix = string.sub(replaced, 1, math.min(12, string.len(replaced)))

    -- table library.
    local list = { "delta", "alpha" }
    table.insert(list, "charlie")
    table.insert(list, "bravo")
    table.sort(list)
    local removed = table.remove(list, #list)
    local joined = table.concat(list, ",")

    -- coroutine library. This is a plain Lua coroutine, not the engine scheduler.
    local co = coroutine.create(function()
        coroutine.yield("yielded")
        return "done"
    end)
    local first_ok, first_value = coroutine.resume(co)
    local status_after_yield = coroutine.status(co)
    local second_ok, second_value = coroutine.resume(co)
    local status_after_done = coroutine.status(co)

    -- package library.
    local package_path = package.path
    local config_loaded = require_ok and config_or_error ~= nil
    local cached_config = package.loaded["Game.Config"]

    print("[lua_sample] libraries", as_text, map_sum, array_sum, argc, math_value, matched, prefix, joined, removed)
    debug_log("[lua_sample] coroutine", first_ok, first_value, status_after_yield, second_ok, second_value, status_after_done)
    log("[lua_sample] package.path", package_path)
    warn("[lua_sample] require Game.Config ok=" .. bool_text(config_loaded) .. " cached=" .. bool_text(cached_config ~= nil))
end

function Sample.MathTypeExamples()
    local a = vec3(1.0, 2.0, 3.0)
    local b = Vector3(4.0, 5.0, 6.0)
    local c = Vector.new(7.0, 8.0, 9.0)
    local vector_result = ((a + b) - c) * 2.0 / 2.0
    vector_result.x = vector_result.x + SAMPLE_OFFSET.x
    vector_result.y = vector_result.y + SAMPLE_OFFSET.y
    vector_result.z = vector_result.z + SAMPLE_OFFSET.z

    local r1 = rotator(10.0, 20.0, 30.0)
    local r2 = Rotator.new(1.0, 2.0, 3.0)
    local rotation_result = (r1 + r2 - rotator(1.0, 1.0, 1.0)) * 0.5
    rotation_result.pitch = rotation_result.pitch + 1.0
    rotation_result.yaw = rotation_result.yaw + 1.0
    rotation_result.roll = rotation_result.roll + 1.0

    return vector_result, rotation_result
end

---@param actor ActorProxy
function Sample.ActorProxyExamples(actor)
    if not actor or not actor:IsValid() then
        warn("[lua_sample] ActorProxyExamples skipped: invalid actor")
        return
    end

    local name = actor.Name
    local uuid = actor.UUID
    local original_tag = actor.Tag
    local original_location = actor.Location
    local original_rotation = actor.Rotation
    local original_scale = actor.Scale
    local original_velocity = actor.Velocity

    actor.Tag = SAMPLE_NAME
    local has_tag = actor:HasTag(SAMPLE_NAME)

    local world_location = actor:GetWorldLocation()
    actor:SetWorldLocation(world_location)
    actor:SetWorldLocationXYZ(world_location.x, world_location.y, world_location.z)

    local world_rotation = actor:GetWorldRotation()
    actor:SetWorldRotation(world_rotation)
    actor:SetWorldRotationXYZ(world_rotation.pitch, world_rotation.yaw, world_rotation.roll)

    local world_scale = actor:GetWorldScale()
    actor:SetWorldScale(world_scale)
    actor:SetWorldScaleXYZ(world_scale.x, world_scale.y, world_scale.z)

    actor.Velocity = vec3(1.0, 0.0, 0.0)
    local velocity = actor.Velocity

    actor:AddWorldOffset(vec3(0.0, 0.0, 0.0))
    actor:AddWorldOffset(0.0, 0.0, 0.0)

    local forward = actor:GetForwardVector()
    local right = actor:GetRightVector()
    local up = actor:GetUpVector()

    local ground = actor:FindGround(500.0, 2.0)
    if ground.hit then
        log("[lua_sample] ground", ground.ground_z, ground.distance, ground.actor.Name, ground.component.Name)
    end

    local by_name = actor:GetComponent("StaticMeshComponent")
    local by_type = actor:GetComponentByType("StaticMeshComponent")
    local by_class = actor:FindComponentByClass("CameraComponent")
    local script_component = actor:GetScriptComponent()
    local static_mesh_component = actor:GetStaticMeshComponent()

    actor:SetMoveSpeed(SAMPLE_SPEED)
    local move_speed = actor:GetMoveSpeed()
    actor:MoveTo(world_location)
    actor:MoveTo(world_location.x, world_location.y)
    actor:MoveTo(world_location.x, world_location.y, world_location.z)
    actor:MoveBy(vec3(0.0, 0.0, 0.0))
    actor:MoveBy(0.0, 0.0)
    actor:MoveBy(0.0, 0.0, 0.0)
    actor:MoveToActor(actor)
    actor:StopMove()
    local move_done = actor:IsMoveDone()

    local damage = actor:GetDamage()
    local damage_set = actor:SetDamage(damage)
    actor:PrintLocation()

    actor.Tag = original_tag
    actor.Location = original_location
    actor.Rotation = original_rotation
    actor.Scale = original_scale
    actor.Velocity = original_velocity

    log("[lua_sample] actor", name, uuid, has_tag, velocity.x, forward.x, right.y, up.z, move_speed, move_done, damage_set)
end

---@param component ComponentProxy
---@param other_actor ActorProxy
function Sample.ComponentProxyExamples(component, other_actor)
    if not component then
        warn("[lua_sample] ComponentProxyExamples skipped: nil component")
        return
    end

    local component_name = component.Name
    local owner = component.Owner
    local type_name_field = component.TypeName
    local is_valid = component:IsValid()
    local type_name = component:GetTypeName()

    local was_active = component:IsActive()
    component:SetActive(was_active)

    local was_visible = component:IsVisible()
    component:SetVisible(was_visible)

    local world_location = component:GetWorldLocation() or vec3(0.0, 0.0, 0.0)
    component:SetWorldLocation(world_location)
    component:SetWorldLocationXYZ(world_location.x, world_location.y, world_location.z)

    local local_location = component:GetLocalLocation() or vec3(0.0, 0.0, 0.0)
    component:SetLocalLocation(local_location)
    component:SetLocalLocationXYZ(local_location.x, local_location.y, local_location.z)

    component:AddWorldOffset(vec3(0.0, 0.0, 0.0))
    component:AddWorldOffsetXYZ(0.0, 0.0, 0.0)
    component:AddLocalOffset(vec3(0.0, 0.0, 0.0))
    component:AddLocalOffsetXYZ(0.0, 0.0, 0.0)

    local world_rotation = component:GetWorldRotation() or rotator(0.0, 0.0, 0.0)
    component:SetWorldRotation(world_rotation)
    component:SetWorldRotationXYZ(world_rotation.pitch, world_rotation.yaw, world_rotation.roll)

    local local_rotation = component:GetLocalRotation() or rotator(0.0, 0.0, 0.0)
    component:SetLocalRotation(local_rotation)
    component:SetLocalRotationXYZ(local_rotation.pitch, local_rotation.yaw, local_rotation.roll)

    local world_scale = component:GetWorldScale() or vec3(1.0, 1.0, 1.0)
    component:SetWorldScale(world_scale)
    component:SetWorldScaleXYZ(world_scale.x, world_scale.y, world_scale.z)

    local local_scale = component:GetLocalScale() or vec3(1.0, 1.0, 1.0)
    component:SetLocalScale(local_scale)
    component:SetLocalScaleXYZ(local_scale.x, local_scale.y, local_scale.z)

    local forward = component:GetForwardVector()
    local right = component:GetRightVector()
    local up = component:GetUpVector()

    component:SetCollisionEnabled(true)
    component:SetGenerateOverlapEvents(true)
    local is_overlapping = component:IsOverlappingActor(other_actor)

    local shape_type = component:GetShapeType()
    local half_height = component:GetShapeHalfHeight()
    if half_height then
        component:SetShapeHalfHeight(half_height)
    end

    local radius = component:GetShapeRadius()
    if radius then
        component:SetShapeRadius(radius)
    end

    local shape_extent = component:GetShapeExtent()
    if shape_extent then
        component:SetShapeExtent(shape_extent)
    end

    local box_extent = component:GetBoxExtent()
    if box_extent then
        component:SetBoxExtent(box_extent)
        component:SetBoxExtent(box_extent.x, box_extent.y, box_extent.z)
    end

    component:SetStaticMesh(SAMPLE_MESH)

    component:SetText("Lua sample text")
    local text = component:GetText()

    local screen_position = component:GetScreenPosition() or vec3(0.0, 0.0, 0.0)
    component:SetScreenPosition(screen_position)
    component:SetScreenPositionXYZ(screen_position.x, screen_position.y, screen_position.z)

    local screen_size = component:GetScreenSize() or vec3(100.0, 100.0, 0.0)
    component:SetScreenSize(screen_size)
    component:SetScreenSizeXYZ(screen_size.x, screen_size.y, screen_size.z)

    component:SetTexture(SAMPLE_TEXTURE)
    local texture_path = component:GetTexturePath()

    component:SetTint(vec3(1.0, 1.0, 1.0))
    component:SetTint(1.0, 1.0, 1.0, 1.0)

    component:SetLabel("Lua Sample")
    local label = component:GetLabel()
    local hovered = component:IsHovered()
    local pressed = component:IsPressed()
    local clicked = component:WasClicked()

    component:SetAudioPath(SAMPLE_SOUND)
    local audio_path = component:GetAudioPath()
    component:SetAudioCategory("sfx")
    local audio_category = component:GetAudioCategory()
    component:SetAudioLooping(false)
    local looping = component:IsAudioLooping()
    component:PlayAudio()
    component:PlayAudio(SAMPLE_SOUND)
    component:PauseAudio()
    component:ResumeAudio()
    local audio_playing = component:IsAudioPlaying()
    component:StopAudio()

    component:SetSpeed(SAMPLE_SPEED)
    local speed = component:GetSpeed()
    component:MoveTo(world_location)
    component:MoveTo(world_location.x, world_location.y, world_location.z)
    component:MoveBy(vec3(0.0, 0.0, 0.0))
    component:MoveBy(0.0, 0.0, 0.0)
    component:StopMove()
    local component_move_done = component:IsMoveDone()

    component:StartCameraShake(0.2, 0.1)
    component:AddHitEffect(0.2, 0.1)

    component:SetActive(was_active)
    component:SetVisible(was_visible)
    component:SetWorldLocation(world_location)
    component:SetLocalLocation(local_location)
    component:SetWorldRotation(world_rotation)
    component:SetLocalRotation(local_rotation)
    component:SetWorldScale(world_scale)
    component:SetLocalScale(local_scale)

    log("[lua_sample] component", component_name, type_name_field, type_name, is_valid, owner.Name, shape_type, is_overlapping)
    debug_log("[lua_sample] component values", text, texture_path, label, hovered, pressed, clicked, audio_path, audio_category, looping, audio_playing, speed, component_move_done)
end

function Sample.InputExamples()
    local key_w = GetKey("W")
    local key_space_down = GetKeyDown("SPACE")
    local key_space_up = GetKeyUp("SPACE")

    local input_w = Input.GetKey("W")
    local input_down = Input.GetKeyDown("SPACE")
    local input_up = Input.GetKeyUp("SPACE")

    local mouse_dx = GetMouseDeltaX()
    local mouse_dy = GetMouseDeltaY()
    local wheel = GetMouseWheel()
    local moved = MouseMoved()

    local dragging = IsDragging("LBUTTON")
    local drag_dx = GetDragDeltaX("LBUTTON")
    local drag_dy = GetDragDeltaY("LBUTTON")
    local drag_distance = GetDragDistance("LBUTTON")

    log("[lua_sample] input", key_w, key_space_down, key_space_up, input_w, input_down, input_up, mouse_dx, mouse_dy, wheel, moved, dragging, drag_dx, drag_dy, drag_distance)
end

function Sample.AudioWorldDataUiExamples()
    local sfx_handle = play_sfx(SAMPLE_SOUND, false)
    if sfx_handle and sfx_handle ~= "" then
        is_audio_playing_by_handle(sfx_handle)
        pause_audio_by_handle(sfx_handle)
        resume_audio_by_handle(sfx_handle)
        stop_audio_by_handle(sfx_handle)
    end

    local bgm_handle = play_bgm(SAMPLE_BGM, true)
    if bgm_handle and bgm_handle ~= "" then
        is_audio_playing_by_handle(bgm_handle)
        pause_audio_by_handle(bgm_handle)
        resume_audio_by_handle(bgm_handle)
        stop_audio_by_handle(bgm_handle)
    end

    is_bgm_playing()
    pause_bgm()
    resume_bgm()
    stop_bgm()
    stop_all_audio()

    local spawned = spawn_actor("AActor", obj:GetWorldLocation())
    if spawned and spawned:IsValid() then
        destroy_actor(spawned)
    end

    local spawned_for_method = spawn_actor("AActor", obj:GetWorldLocation())
    if spawned_for_method and spawned_for_method:IsValid() then
        spawned_for_method:Destroy()
    end

    local found_by_name = find_actor(obj.Name)
    local found_by_uuid = find_actor_by_uuid(obj.UUID)
    local found_by_tag = find_actor_by_tag(obj.Tag)
    local tagged_actors = find_actors_by_tag(obj.Tag)
    for index, actor in ipairs(tagged_actors) do
        log("[lua_sample] tagged actor", index, actor.Name)
    end

    save_json_file(SAMPLE_JSON, {
        name = SAMPLE_NAME,
        speed = SAMPLE_SPEED,
        flag = SAMPLE_FLAG,
        offset = { SAMPLE_OFFSET.x, SAMPLE_OFFSET.y, SAMPLE_OFFSET.z },
    })
    local loaded_json = load_json_file(SAMPLE_JSON)

    open_score_save_popup(12345)
    local nickname = consume_score_save_popup_result()
    open_message_popup("Lua sample message")
    local confirmed = consume_message_popup_ok()
    open_scoreboard_popup(SAMPLE_JSON)
    open_title_options_popup()

    -- Dangerous lifecycle-changing calls are shown but intentionally disabled.
    if false then
        load_scene(SAMPLE_SCENE)
        request_exit_game()
    end

    log("[lua_sample] world/data/ui", found_by_name.Name, found_by_uuid.UUID, found_by_tag.Name, nickname, confirmed, loaded_json ~= nil)
end

function Sample.CoroutineExample()
    log("[lua_sample] coroutine start")
    wait(0.1)
    Wait(0.1)
    wait_frames(1)

    if obj and obj:IsValid() then
        obj:SetMoveSpeed(SAMPLE_SPEED)
        obj:MoveTo(obj:GetWorldLocation())
        wait_until_move_done()
    end

    wait_key_down("SPACE")
    log("[lua_sample] coroutine end")
end

function BeginPlay()
    print("[lua_sample] BeginPlay", SAMPLE_NAME)
    Sample.OpenLibraryExamples()
    Sample.MathTypeExamples()

    if not RUN_DEMO then
        warn("[lua_sample] RunDemo is false. Mutating binding examples are defined but not executed.")
        return
    end

    if obj and obj:IsValid() then
        Sample.ActorProxyExamples(obj)
        Sample.ComponentProxyExamples(obj:GetComponentByType("SceneComponent"), obj)
        Sample.ComponentProxyExamples(obj:GetComponentByType("StaticMeshComponent"), obj)
        Sample.ComponentProxyExamples(obj:GetComponentByType("CameraComponent"), obj)
        Sample.ComponentProxyExamples(obj:GetComponentByType("UIButtonComponent"), obj)
        Sample.ComponentProxyExamples(obj:GetComponentByType("UUIImageComponent"), obj)
        Sample.ComponentProxyExamples(obj:GetComponentByType("USoundComponent"), obj)
        Sample.InputExamples()
        Sample.AudioWorldDataUiExamples()
        StartCoroutine("Sample.CoroutineExample")
    end
end

function Tick(dt)
    local elapsed = time()
    local frame_dt = delta_time()

    if RUN_DEMO and GetKeyDown("F9") then
        Sample.InputExamples()
        log("[lua_sample] Tick trigger", dt, elapsed, frame_dt)
    end
end

function EndPlay()
    print("[lua_sample] EndPlay")
end

function OnBeginOverlap(otherActor, otherComp, selfComp)
    log("[lua_sample] OnBeginOverlap", otherActor.Name, otherComp.Name, selfComp.Name)
    if RUN_DEMO then
        Sample.ComponentProxyExamples(selfComp, otherActor)
    end
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    log("[lua_sample] OnEndOverlap", otherActor.Name, otherComp.Name, selfComp.Name)
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    log("[lua_sample] OnHit", otherActor.Name, otherComp.Name, selfComp.Name, impactLocation.x, impactLocation.y, impactLocation.z, impactNormal.x, impactNormal.y, impactNormal.z)
end

function OnInputAction(actionName, value, scalar)
    log("[lua_sample] OnInputAction", actionName, value.x, value.y, value.z, scalar)
end
