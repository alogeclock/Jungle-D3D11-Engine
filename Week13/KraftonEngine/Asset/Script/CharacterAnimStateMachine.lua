-- CharacterAnimStateMachine.lua
-- Lua로 구현하는 캐릭터 애니메이션 State Machine
-- 사용법: C++에서 ULuaAnimStateMachine::LoadScript("Asset/Script/CharacterAnimStateMachine.lua")

------------------------------------------------------------------------
-- 상태 정의
------------------------------------------------------------------------
local STATES = {
    Idle  = "Idle",
    Walk  = "Walk",
    Run   = "Run",
    Jump  = "Jump",
    Land  = "Land",
}

-- 상태별 재생할 애니메이션 시퀀스 이름
local STATE_SEQUENCE = {
    [STATES.Idle] = "Idle_Anim",
    [STATES.Walk] = "Walk_Anim",
    [STATES.Run]  = "Run_Anim",
    [STATES.Jump] = "Jump_Anim",
    [STATES.Land] = "Land_Anim",
}

------------------------------------------------------------------------
-- 컨텍스트 (Tick마다 C++ 쪽에서 갱신해줘야 할 값들)
-- 실제 프로젝트에서는 self:getOwnerSpeed() 같은 바인딩을 추가하거나
-- 공유 테이블로 주입하세요.
------------------------------------------------------------------------
local ctx = {
    speed     = 0.0,   -- 현재 이동 속도 (m/s)
    isJumping = false, -- 점프 중 여부
    isGrounded= true,  -- 지면 접촉 여부
}

------------------------------------------------------------------------
-- 전환 규칙 테이블
-- { from = "State", to = "State", condition = function(ctx) -> bool }
------------------------------------------------------------------------
local TRANSITIONS = {
    { from = STATES.Idle, to = STATES.Walk, condition = function(c) return c.speed > 0.5  end },
    { from = STATES.Walk, to = STATES.Run,  condition = function(c) return c.speed > 4.0  end },
    { from = STATES.Walk, to = STATES.Idle, condition = function(c) return c.speed <= 0.5 end },
    { from = STATES.Run,  to = STATES.Walk, condition = function(c) return c.speed <= 4.0 end },
    { from = STATES.Idle, to = STATES.Jump, condition = function(c) return c.isJumping    end },
    { from = STATES.Walk, to = STATES.Jump, condition = function(c) return c.isJumping    end },
    { from = STATES.Run,  to = STATES.Jump, condition = function(c) return c.isJumping    end },
    { from = STATES.Jump, to = STATES.Land, condition = function(c) return c.isGrounded and not c.isJumping end },
    { from = STATES.Land, to = STATES.Idle, condition = function(c) return true            end },
}

------------------------------------------------------------------------
-- 콜백: 스크립트 로드 직후 한 번 호출됨
------------------------------------------------------------------------
function onInit()
    print("[AnimSM] onInit — initial state: " .. self:getCurrentState())
    -- 첫 상태 세팅
    self:transitionTo(STATES.Idle)
end

------------------------------------------------------------------------
-- 콜백: 매 프레임 C++의 ProcessState(dt)에서 호출됨
------------------------------------------------------------------------
function onUpdate(dt)
    local cur = self:getCurrentState()

    for _, rule in ipairs(TRANSITIONS) do
        if rule.from == cur and rule.condition(ctx) then
            self.blendDuration = 0.15
            self:transitionTo(rule.to)
            break
        end
    end
end

------------------------------------------------------------------------
-- 콜백: TransitionTo() 가 실행될 때 호출됨
------------------------------------------------------------------------
function onTransition(newState)
    local seqName = STATE_SEQUENCE[newState]
    if seqName then
        self:setSequence(seqName)
    end
    print("[AnimSM] -> " .. newState)
end

------------------------------------------------------------------------
-- 외부(다른 Lua 스크립트 또는 C++)에서 컨텍스트를 주입하는 헬퍼
-- 예: require("CharacterAnimStateMachine").setContext({ speed = 5.0 })
------------------------------------------------------------------------
local M = {}

function M.setContext(newCtx)
    if newCtx.speed     ~= nil then ctx.speed     = newCtx.speed     end
    if newCtx.isJumping ~= nil then ctx.isJumping = newCtx.isJumping end
    if newCtx.isGrounded~= nil then ctx.isGrounded= newCtx.isGrounded end
end

return M
