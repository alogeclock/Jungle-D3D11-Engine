# BeatMaze - 리듬 게임 기반 미로찾기

DirectX 11 + ImGui 기반의 리듬 액션 미로찾기 게임입니다.
BPM에 맞춰 입력하면 점수 보너스를 얻으며, 미로를 탈출하는 것이 목표입니다.

## 팀원

| 역할 | 이름 |
|------|------|
| 코어 + 파일 처리 | 김연하 |
| 렌더/UI | 이호진 |
| 게임플레이 | 전현길, 심우진 |

## 기술 스택

- **언어**: C++17
- **그래픽**: DirectX 11, HLSL
- **UI**: ImGui
- **빌드**: Visual Studio (MSVC)

## 프로젝트 구조

```
├── Core/               # 엔진 코어 (Application, Renderer, Input, Time, Texture)
├── Scene/              # 씬 관리 (TitleScene, PlayScene, SceneManager)
├── Gameplay/           # 게임 로직 (Stage, Player, Monster, BeatSystem, Camera2D)
├── UI/                 # UI 시스템 (UIManager, PopupManager, Widgets)
├── Data/               # 데이터 I/O (FileManager, MapLoader, ScoreRepository)
├── Resources/
│   ├── Maps/           # 맵 파일 (.map)
│   ├── Shaders/        # HLSL 셰이더
│   └── Sprites/        # 스프라이트 에셋 (player, monster, tile, wall)
├── imgui/              # ImGui 라이브러리
└── main.cpp            # 엔트리포인트
```

## 핵심 시스템

- **BeatSystem**: BPM 기반 박자 판정 (Perfect / Good / Miss)
- **Stage**: 맵 로딩, 액터 관리, 충돌 처리
- **Camera2D**: 플레이어 추적 카메라 (줌, 부드러운 이동)
- **ScoreSystem**: 콤보 기반 점수 시스템
- **Monster AI**: ChasePlayer, RandomMove, Patrol 행동 패턴

## 빌드 방법

1. `Jungle_Week1_Team2.sln`을 Visual Studio에서 열기
2. 구성: `Debug|x64` 또는 `Release|x64`
3. 빌드 및 실행 (F5)
