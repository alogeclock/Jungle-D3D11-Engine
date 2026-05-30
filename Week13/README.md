# KraftonEngine

KraftonEngine은 DirectX 11 기반의 C++ 게임 엔진 프로젝트입니다. 렌더링, 에디터, 게임 런타임, PhysX 물리, NvCloth 클로스 시뮬레이션 연동을 포함합니다.

## 첫 실행

1. Visual Studio 2022에서 C++ 데스크톱 개발 도구가 설치되어 있는지 확인합니다.
2. 루트 폴더에서 아래 명령을 실행합니다.

```bat
GenerateProjectFiles.bat
```

3. 스크립트가 필요한 도구와 ThirdParty 라이브러리를 준비합니다.
   - Python/CMake가 없으면 자동으로 설치합니다.
   - PhysX와 NvCloth는 `KraftonEngine\Build`에서 빌드하고 `KraftonEngine\ThirdParty`로 stage합니다.
4. 생성된 `KraftonEngine.sln`을 Visual Studio 2022로 열고 `Debug|x64` 또는 `Release|x64`로 빌드/실행합니다.
