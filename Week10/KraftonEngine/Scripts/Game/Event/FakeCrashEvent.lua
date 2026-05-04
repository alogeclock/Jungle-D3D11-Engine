local crash_ui = nil
local ok_btn = nil
local is_crashed = false
local GameManager = require("Game.GameManager")

-- 컴포넌트를 여러 이름으로 시도해서 찾는 헬퍼 함수
local function get_component(...)
    local names = { ... }
    for index = 1, #names do
        local name = names[index]
        local component = obj:GetComponent(name)
        if component and component:IsValid() then
            return component
        end
    end
    return nil
end

function BeginPlay()
	-- 1. 배경 이미지 찾기 (사용자 지정 이름 또는 엔진 기본 이름들 시도)
	crash_ui = get_component("CrashAlertBox", "UUIImageComponent_0", "UUIImageComponent_1")
	
	-- 2. OK 버튼 찾기 (사용자 지정 이름 또는 엔진 기본 이름들 시도)
	ok_btn = get_component("CrashOKButton", "UIButtonComponent_0", "UIButtonComponent_1")

	if crash_ui then
		crash_ui:SetTexture("Asset/Content/Texture/CrashImage.png")
		crash_ui:SetVisible(false) 
	else
		warn("Crash 배경 이미지 컴포넌트를 찾을 수 없습니다.")
	end

	if ok_btn then
		ok_btn:SetTexture("Asset/Content/Texture/OK.png")
		ok_btn:SetVisible(false)
	else
		warn("Crash OK 버튼 컴포넌트를 찾을 수 없습니다.")
	end

	-- GameManager의 OnCrashDumpAnalyzed 이벤트 훅
	local original_analyzed = GameManager.OnCrashDumpAnalyzed
	GameManager.OnCrashDumpAnalyzed = function()
		if original_analyzed then original_analyzed() end
		TriggerFakeCrash()
	end
end

function TriggerFakeCrash()
	if is_crashed or not crash_ui then return end
	is_crashed = true

	StartCoroutine("RunFakeCrashSequence")
end

-- CrashOKButton의 OnClickAction에 "OnCrashOKClicked"를 연결하여 호출됨
function OnCrashOKClicked()
	if is_crashed then
		is_crashed = false
	end
end

function RunFakeCrashSequence()
	GameManager.Pause()
	stop_all_audio()
	show_cursor(true)
	
	wait_frames(5)
	-- 파일명에 점(.) 대신 대시(-)가 쓰인 경우를 고려하여 파일명 확인
	play_sfx("Sound.SFX.windows-98-error", false)

	crash_ui:SetVisible(true)
	crash_ui:SetTint(1.0, 1.0, 1.0, 1.0)
	
	if ok_btn then
		ok_btn:SetVisible(true)
		ok_btn:SetTint(1.0, 1.0, 1.0, 1.0)
	end

	while is_crashed do
		-- 버튼 클릭 외에도 ESC나 Enter로 넘길 수 있도록 처리
		if GetKeyDown("ESCAPE") or GetKeyDown("ENTER") then
			is_crashed = false
			break
		end
		wait_frames(1)
	end

	crash_ui:SetVisible(false)
	if ok_btn then
		ok_btn:SetVisible(false)
	end
	
	show_cursor(false)
	play_sfx("Sound.SFX.glitch_noise", false)
	stop_all_audio()
	
	GameManager.Resume()
end

function Tick(dt)
end