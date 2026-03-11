#include "pch.h"
#include "Monster.h"
#include "Player.h"
#include "Stage.h"
#include <cstdlib>
#include <queue>

EActorType FMonster::GetActorType() const { return EActorType::Monster; }

void FMonster::Update(float DeltaTime, FGameContext &Context)
{
    FActor::Update(DeltaTime, Context);
}

void FMonster::OnBeat(FStage &Stage)
{
    MoveFrequencyOffset++;
    if (MoveFrequencyOffset == MoveFrequency)
    {
        // 비트에 따라 각 몬스터들의 다음 이동 방향 결정
        EDirection Dir = DecideNextMove(Stage);
        TryMove(Stage, Dir);
        MoveFrequencyOffset = 0;
    }
}

// 이동 주기 설정 (1: 매 비트, 2: 2비트마다 등)
void FMonster::SetMoveFrequency(int InFreq) { MoveFrequency = InFreq; }

int FMonster::GetMoveFrequency() const { return MoveFrequency; }

void FMonster::SetAiType(EMonsterAIType InAiType) { AiType = InAiType; }

EMonsterAIType FMonster::GetAiType() const { return AiType; }

void FMonster::SetMonsterType(int InMonType) { MonsterType = InMonType; }

int FMonster::GetMonsterType() const { return MonsterType; }

void FMonster::SetSearchRange(int InSearchRange) { SearchRange = InSearchRange; }

int FMonster::GetSearchRange() const { return SearchRange; }

EDirection FMonster::DecideNextMove(const FStage &Stage) const
{
    // 1. BFS 탐색을 통해 플레이어를 추적하는 최단 경로 방향을 찾음
    auto ChasingDir = SearchPlayer(Stage);
    if (ChasingDir.has_value())
    {
        return ChasingDir.value();
    }

    // 2. 탐색 범위(5스텝) 내에 플레이어가 없으면 기존 랜덤 이동 수행
    int Random = std::rand() % 4;
    switch (Random)
    {
    case 0:
        return EDirection::Up;
    case 1:
        return EDirection::Down;
    case 2:
        return EDirection::Left;
    default:
        return EDirection::Right;
    }
}

std::optional<EDirection> FMonster::SearchPlayer(const FStage &Stage) const
{
    int StartX = GetTileX();
    int StartY = GetTileY();

    const FPlayer &Player = Stage.GetPlayer();
    int            TargetX = Player.GetTileX();
    int            TargetY = Player.GetTileY();

    // 이미 플레이어와 같은 위치라면 탐색 종료
    if (StartX == TargetX && StartY == TargetY)
        return std::nullopt;

    // BFS를 위한 노드 구조체 (현재 위치, 누적 거리, 첫 번째 이동 방향)
    struct FNode
    {
        int        X, Y, Distance;
        EDirection FirstStep;
    };

    std::queue<FNode> Queue;

    // 방문 여부 확인을 위한 맵 정보 가져오기
    const FStageData &Map = Stage.GetMap();
    int               MapWidth = Map.GetWidth();
    int               MapHeight = Map.GetHeight();

    // 1차원 인덱스로 방문 체크 (중복 방문 방지)
    std::vector<bool> Visited(MapWidth * MapHeight, false);
    Visited[StartY * MapWidth + StartX] = true;

    // 이동 가능한 4방향 정의
    EDirection Dirs[] = {EDirection::Up, EDirection::Down, EDirection::Left, EDirection::Right};

    // 몬스터의 현재 위치에서 갈 수 있는 첫 번째 스텝들을 큐에 삽입
    for (auto Dir : Dirs)
    {
        int nextX = StartX;
        int nextY = StartY;

        if (Dir == EDirection::Up)
            nextY--;
        else if (Dir == EDirection::Down)
            nextY++;
        else if (Dir == EDirection::Left)
            nextX--;
        else if (Dir == EDirection::Right)
            nextX++;

        if (!Map.IsInside(nextX, nextY))
            continue;

        // 바로 옆에 플레이어가 있는 경우
        if (nextX == TargetX && nextY == TargetY)
            return Dir;

        // 벽이 아니거나 다른 오브젝트가 없는 곳으로만 탐색 시작
        if (Stage.CanMoveTo(nextX, nextY))
        {
            Visited[nextY * MapWidth + nextX] = true;
            Queue.push({nextX, nextY, 1, Dir});
        }
    }

    // BFS 탐색 시작
    while (!Queue.empty())
    {
        FNode Current = Queue.front();
        Queue.pop();

        // 설정된 탐색 범위(5스텝)를 초과하면 해당 경로는 무시
        if (Current.Distance >= SearchRange)
            continue;

        for (auto Dir : Dirs)
        {
            int nextX = Current.X;
            int nextY = Current.Y;

            if (Dir == EDirection::Up)
                nextY--;
            else if (Dir == EDirection::Down)
                nextY++;
            else if (Dir == EDirection::Left)
                nextX--;
            else if (Dir == EDirection::Right)
                nextX++;

            if (!Map.IsInside(nextX, nextY) || Visited[nextY * MapWidth + nextX])
                continue;

            // 플레이어 위치에 도달하면 이 경로의 시작 방향을 반환
            if (nextX == TargetX && nextY == TargetY)
                return Current.FirstStep;

            // 이동 가능한 타일인 경우에만 큐에 추가
            if (Stage.CanMoveTo(nextX, nextY))
            {
                Visited[nextY * MapWidth + nextX] = true;
                Queue.push({nextX, nextY, Current.Distance + 1, Current.FirstStep});
            }
        }
    }

    return std::nullopt; // 20스텝 이내에 플레이어를 찾지 못함
}
