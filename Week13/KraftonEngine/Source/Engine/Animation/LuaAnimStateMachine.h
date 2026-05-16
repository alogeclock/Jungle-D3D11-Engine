#pragma once
#include "Animation/AnimationStateMachine.h"
#include <sol/sol.hpp>
#include <string>

class UAnimSequence;

class ULuaAnimStateMachine : public UAnimationStateMachine
{
public:
	DECLARE_CLASS(ULuaAnimStateMachine, UAnimationStateMachine)

	void Initialize(USkeletalMeshComponent* InOwner) override;

	// Lua 스크립트 파일을 로드하고 onInit() 호출
	void LoadScript(const std::string& ScriptPath);

	// AnimInstance 변수를 Lua 환경에 노출 — LoadScript() 이후에 호출
	// ex) BindProperty("Speed", &Speed);
	template<typename T>
	void BindProperty(const std::string& Name, T* Ptr)
	{
		ScriptEnv[Name] = Ptr;
	}

	// 매 프레임 호출 — Lua의 onUpdate(dt) 로 위임
	void ProcessState(float DeltaSeconds) override;

	// Lua에서 호출하는 전환 API
	void TransitionTo(const std::string& StateName);
	void SetBlendDuration(float Duration) { BlendDuration = Duration; }
	float GetBlendDuration() const { return BlendDuration; }
	std::string GetCurrentStateName() const { return CurrentStateName; }

	// Lua에서 AnimSequence를 이름으로 설정
	void SetSequenceByName(const std::string& SequenceName);

private:
	sol::environment ScriptEnv;
	sol::protected_function LuaOnUpdate;
	sol::protected_function LuaOnTransition;

	std::string CurrentStateName;
	std::string ScriptFilePath;

	bool bScriptLoaded = false;
};
