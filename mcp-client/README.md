# UnrealBlueprintMCP Client

AI 모델(Claude Code, Gemini CLI 등)이 언리얼 엔진의 블루프린트를 자동으로 생성하고 편집할 수 있도록 하는 Model Context Protocol (MCP) 클라이언트입니다.

## 📁 폴더 구조

```
mcp-client/
├── 📄 README.md              # 이 파일
├── 📄 requirements.txt       # Python 의존성
├── 📄 setup.py              # 패키지 설치 스크립트
├── 📁 src/                  # 핵심 소스 코드
│   └── mcp_client.py        # 메인 MCP 클라이언트 서버
├── 📁 config/               # 설정 파일들
│   └── config.json          # Claude Code MCP 설정
├── 📁 tools/                # 유틸리티 도구들
│   ├── diagnostic-check.sh  # 시스템 진단 스크립트
│   └── unreal-helper.py     # Gemini CLI 헬퍼
├── 📁 examples/             # 사용 예제들
│   └── quick-start-example.py # 빠른 시작 가이드
└── 📁 tests/                # 테스트 파일들
    └── test_mcp_api.py      # API 테스트 스위트
```

## 🚀 빠른 시작

### 1. 의존성 설치
```bash
cd mcp-client
pip install -r requirements.txt
```

### 2. 언리얼 엔진 플러그인 실행
- 언리얼 엔진에서 UnrealBlueprintMCP 플러그인 활성화
- 플러그인이 localhost:8080에서 자동 실행됨

### 3. Claude Code에서 사용
```bash
# Claude Code MCP 설정 추가
cp config/config.json ~/.config/claude-code/mcp-servers/
```

### 4. Gemini CLI에서 사용
```bash
# 헬퍼 스크립트로 언리얼 작업
echo "List all blueprints" | python tools/unreal-helper.py
```

## 🔧 주요 기능

### Resources (에셋 관리)
- `list_assets`: 에셋 목록 조회
- `get_asset`: 특정 에셋 정보 조회  
- `create_asset`: 새 에셋 생성

### Tools (블루프린트 편집)
- `create_blueprint`: 블루프린트 클래스 생성
- `add_variable`: 블루프린트 변수 추가
- `add_function`: 블루프린트 함수 추가
- `edit_graph`: 블루프린트 그래프 편집

### Prompts (개발 가이드)
- `blueprint_best_practices`: 블루프린트 모범 사례
- `performance_tips`: 성능 최적화 팁
- `node_reference`: 노드 참조 정보
- `troubleshooting`: 문제 해결 가이드

## 🛠️ 도구들

### 시스템 진단
```bash
./tools/diagnostic-check.sh
```

### API 테스트
```bash
python tests/test_mcp_api.py
```

### 빠른 예제 실행
```bash
python examples/quick-start-example.py
```

## 📚 상세 문서

프로젝트 루트의 다음 문서들을 참조하세요:
- `../README.md`: 전체 프로젝트 개요
- `../INTEGRATION_GUIDE.md`: Claude Code/Gemini CLI 통합 가이드
- `../TECHNICAL_GUIDE.md`: 개발자 기술 문서
- `../USER_GUIDE.md`: 사용자 가이드

## 🤝 기여하기

1. 이 저장소를 포크하세요
2. 기능 브랜치를 생성하세요 (`git checkout -b feature/AmazingFeature`)
3. 변경사항을 커밋하세요 (`git commit -m 'Add AmazingFeature'`)
4. 브랜치에 푸시하세요 (`git push origin feature/AmazingFeature`)
5. Pull Request를 생성하세요

## 📄 라이센스

이 프로젝트는 MIT 라이센스 하에 배포됩니다. 자세한 내용은 `LICENSE` 파일을 참조하세요.