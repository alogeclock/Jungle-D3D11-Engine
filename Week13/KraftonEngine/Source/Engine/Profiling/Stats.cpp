#include "Profiling/Stats.h"

#include <algorithm>

uint32 FDrawCallStats::Count = 0;

#if STATS
uint32 FLODStats::LODCount[4] = { 0, 0, 0, 0 };
#endif

FStatManager::FStatManager()
{
	QueryPerformanceFrequency(&Frequency);
}

void FStatManager::RecordTime(const char* Name, double ElapsedSeconds, const char* Category)
{
	FStatKey Key{ Name, Category };
	FStatAccum& Accum = Stats[Key];
	if (!Accum.Name)
	{
		Accum.Name = Name;
		Accum.Category = Category;
	}
	Accum.FrameCallCount++;
	Accum.FrameTotal += ElapsedSeconds;
}

void FStatManager::TakeSnapshot()
{
	LARGE_INTEGER Now;
	QueryPerformanceCounter(&Now);
	const double CurrentTime = static_cast<double>(Now.QuadPart) / static_cast<double>(Frequency.QuadPart);
	const double CutoffTime = CurrentTime - STAT_AVG_WINDOW_SECONDS;

	Snapshot.clear();
	Snapshot.reserve(Stats.size());

	for (auto& [StatKey, Accum] : Stats)
	{
		// 현재 프레임 결과를 윈도우에 기록
		double FrameTime = Accum.FrameTotal;
		Accum.Window[Accum.WindowHead] = FrameTime;
		Accum.WindowTimes[Accum.WindowHead] = CurrentTime;
		Accum.WindowHead = (Accum.WindowHead + 1) % STAT_WINDOW_SIZE;
		if (Accum.WindowCount < STAT_WINDOW_SIZE)
			Accum.WindowCount++;

		// 최근 2초 이내 샘플만 사용해 Avg/Max/Min 계산
		double Sum = 0.0;
		double WMax = 0.0;
		double WMin = DBL_MAX;
		uint32 ValidCount = 0;
		for (uint32 i = 0; i < Accum.WindowCount; ++i)
		{
			if (Accum.WindowTimes[i] < CutoffTime)
				continue;
			double Val = Accum.Window[i];
			Sum += Val;
			WMax = (std::max)(WMax, Val);
			WMin = (std::min)(WMin, Val);
			++ValidCount;
		}

		FStatEntry Entry;
		Entry.Name = Accum.Name;
		Entry.Category = Accum.Category;
		Entry.CallCount = Accum.FrameCallCount;
		Entry.TotalTime = FrameTime;
		Entry.AvgTime = ValidCount > 0 ? Sum / ValidCount : 0.0;
		Entry.MaxTime = WMax;
		Entry.MinTime = WMin == DBL_MAX ? 0.0 : WMin;
		Entry.LastTime = FrameTime;
		Snapshot.push_back(Entry);

		// 현재 프레임 누적 리셋
		Accum.FrameCallCount = 0;
		Accum.FrameTotal = 0.0;
	}
}
