#pragma once

#include <optional>
#include "Actor.h"

class FPlayer : public FActor
{
public:
	EActorType GetActorType() const override;

	void Update(float DeltaTime) override;
	void OnBeat(FStage& Stage) override;

	void QueueInput(EDirection InDirection);
	void ClearQueuedInput();
	bool HasQueuedInput() const;

private:
	std::optional<EDirection> QueuedInput;
};
