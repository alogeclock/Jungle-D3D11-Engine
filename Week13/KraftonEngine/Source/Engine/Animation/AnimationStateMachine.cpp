#include "AnimationStateMachine.h"

UAnimSequence* UAnimationStateMachine::GetCurrentSequence() const
{
	return CurrentSequence;
}

float UAnimationStateMachine::GetCurrentStateTime() const
{
	return StateLocalTime;
}
