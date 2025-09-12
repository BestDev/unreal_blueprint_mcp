# UnrealBlueprintMCP 개발 로드맵 (Phase 2 & 3)

## 📋 문서 개요

이 문서는 UnrealBlueprintMCP 플러그인의 Phase 2 (아키텍처 개선) 및 Phase 3 (고급 기능) 개발 계획을 상세히 기술합니다.

**현재 상태**: Phase 1 완료 (Critical → Stable)
- ✅ 무한 재귀 크래시 해결
- ✅ 보안 취약점 해결 (localhost 바인딩, CORS 제거)
- ✅ 스레드 안전성 확보 (Game Thread 마샬링)
- ✅ 기본 에러 처리 구현

---

## 🛠️ Phase 2: 아키텍처 개선 (1-2주)

### 목표
확장 가능하고 유지보수 가능한 구조로 전환하여 장기적 개발 기반 마련

### 1. 모듈형 서비스 핸들러 시스템

#### 현재 문제점
```cpp
// 현재: 하드코딩된 if-else 체인 (확장 불가)
if (Method == TEXT("ping")) {
    Result = HandlePing(Params);
} else if (Method == TEXT("getBlueprints")) {
    Result = HandleGetBlueprints(Params);
} // ... 20개 이상의 조건문
```

#### 개선 방향 (단순하고 명확한 구조)
```cpp
// 간단한 핸들러 베이스 클래스
class UNREALBLUEPRINTMCP_API FMCPHandler
{
public:
    virtual ~FMCPHandler() = default;
    virtual bool CanHandle(const FString& Method) const = 0;
    virtual TSharedPtr<FJsonObject> HandleRequest(const FString& Method, TSharedPtr<FJsonObject> Params) = 0;
    virtual FString GetHandlerName() const = 0;
};

// 리소스 핸들러 예시 (단순한 구조)
class UNREALBLUEPRINTMCP_API FMCPResourcesHandler : public FMCPHandler
{
public:
    virtual bool CanHandle(const FString& Method) const override
    {
        return Method.StartsWith(TEXT("resources/"));
    }
    
    virtual TSharedPtr<FJsonObject> HandleRequest(const FString& Method, TSharedPtr<FJsonObject> Params) override
    {
        if (Method == TEXT("resources/list")) return HandleList(Params);
        if (Method == TEXT("resources/get")) return HandleGet(Params);
        if (Method == TEXT("resources/create")) return HandleCreate(Params);
        return CreateErrorResponse(TEXT("Method not found"));
    }
    
    virtual FString GetHandlerName() const override { return TEXT("Resources"); }

private:
    TSharedPtr<FJsonObject> HandleList(TSharedPtr<FJsonObject> Params);
    TSharedPtr<FJsonObject> HandleGet(TSharedPtr<FJsonObject> Params);
    TSharedPtr<FJsonObject> HandleCreate(TSharedPtr<FJsonObject> Params);
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
};

// 메인 서버에서 단순하게 등록
void FMCPJsonRpcServer::RegisterHandlers()
{
    Handlers.Add(MakeShared<FMCPResourcesHandler>());
    Handlers.Add(MakeShared<FMCPToolsHandler>());
    Handlers.Add(MakeShared<FMCPPromptsHandler>());
}
```

### 2. 설정 시스템 (UE5 표준 방식)

```cpp
// UE5의 표준 설정 클래스 활용 (복잡한 싱글톤 패턴 등 사용 안함)
UCLASS(config=Editor, defaultconfig)
class UNREALBLUEPRINTMCP_API UMCPServerSettings : public UObject
{
    GENERATED_BODY()

public:
    /** 서버 포트 번호 */
    UPROPERTY(config, EditAnywhere, Category = "Network", meta = (ClampMin = "1024", ClampMax = "65535"))
    int32 ServerPort = 8080;

    /** 외부 연결 허용 여부 (보안상 기본값은 false) */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bAllowExternalConnections = false;

    /** 요청 타임아웃 (초) */
    UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float RequestTimeoutSeconds = 5.0f;

    /** 최대 동시 연결 수 */
    UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxConcurrentConnections = 10;

    /** 상세 로깅 활성화 */
    UPROPERTY(config, EditAnywhere, Category = "Debug")
    bool bEnableDetailedLogging = false;

    /** 작업 허용 에셋 경로 목록 */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    TArray<FString> AllowedAssetPaths = {TEXT("/Game/")};

    // 간단한 접근자 함수들 (복잡한 로직 없음)
    static UMCPServerSettings* Get() { return GetMutableDefault<UMCPServerSettings>(); }
    bool IsPathAllowed(const FString& AssetPath) const;
    FString GetBindAddress() const { return bAllowExternalConnections ? TEXT("0.0.0.0") : TEXT("127.0.0.1"); }
};
```

### 3. 상태 관리 시스템 (UE5 열거형 활용)

```cpp
// UE5 표준 열거형 사용 (복잡한 상태 머신 없음)
UENUM(BlueprintType)
enum class EMCPServerState : uint8
{
    Stopped     UMETA(DisplayName = "정지됨"),
    Starting    UMETA(DisplayName = "시작 중"),
    Running     UMETA(DisplayName = "실행 중"),
    Stopping    UMETA(DisplayName = "종료 중"),
    Error       UMETA(DisplayName = "오류")
};

// 단순한 상태 관리 클래스
class UNREALBLUEPRINTMCP_API FMCPServerStateManager
{
private:
    EMCPServerState CurrentState = EMCPServerState::Stopped;
    FString LastErrorMessage;
    int32 ActiveConnectionCount = 0;
    FDateTime ServerStartTime;
    
public:
    // 상태 변경 (단순한 로직)
    void SetState(EMCPServerState NewState, const FString& ErrorMessage = TEXT(""))
    {
        if (CurrentState != NewState)
        {
            UE_LOG(LogTemp, Log, TEXT("MCP Server state changed: %s -> %s"), 
                   *UEnum::GetValueAsString(CurrentState), 
                   *UEnum::GetValueAsString(NewState));
            
            CurrentState = NewState;
            LastErrorMessage = ErrorMessage;
            
            if (NewState == EMCPServerState::Running && ServerStartTime == FDateTime{})
            {
                ServerStartTime = FDateTime::Now();
            }
        }
    }
    
    // 간단한 접근자들
    EMCPServerState GetState() const { return CurrentState; }
    FString GetLastError() const { return LastErrorMessage; }
    bool IsRunning() const { return CurrentState == EMCPServerState::Running; }
    FTimespan GetUptime() const 
    { 
        return IsRunning() ? (FDateTime::Now() - ServerStartTime) : FTimespan::Zero(); 
    }
    
    // 상태 보고서 (단순한 문자열 생성)
    FString GetStatusReport() const
    {
        return FString::Printf(TEXT("State: %s, Connections: %d, Uptime: %s"), 
                              *UEnum::GetValueAsString(CurrentState),
                              ActiveConnectionCount,
                              *GetUptime().ToString());
    }
    
    void IncrementConnections() { ActiveConnectionCount++; }
    void DecrementConnections() { if (ActiveConnectionCount > 0) ActiveConnectionCount--; }
};
```

### 4. 향상된 에러 처리 (UE5 로깅 시스템 활용)

```cpp
// UE5 표준 로깅 카테고리 사용
DECLARE_LOG_CATEGORY_EXTERN(LogMCPServer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMCPSecurity, Warning, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMCPPerformance, Log, All);

// 단순한 에러 코드 열거형
UENUM()
enum class EMCPErrorCode : int32
{
    // JSON-RPC 표준 에러
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    
    // MCP 전용 에러 (1000번대)
    ThreadSafetyError = -1000,
    AssetNotFound = -1001,
    AssetCreationFailed = -1002,
    PermissionDenied = -1003,
    ServerOverloaded = -1004
};

// 단순한 에러 처리 헬퍼 클래스
class UNREALBLUEPRINTMCP_API FMCPErrorHelper
{
public:
    // 표준 에러 응답 생성 (복잡한 팩토리 패턴 없음)
    static TSharedPtr<FJsonObject> CreateErrorResponse(EMCPErrorCode Code, const FString& Message, TSharedPtr<FJsonValue> Id = nullptr)
    {
        TSharedPtr<FJsonObject> Error = MakeShareable(new FJsonObject);
        Error->SetNumberField(TEXT("code"), static_cast<int32>(Code));
        Error->SetStringField(TEXT("message"), Message);
        
        TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
        Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
        Response->SetObjectField(TEXT("error"), Error);
        
        if (Id.IsValid())
        {
            Response->SetField(TEXT("id"), Id);
        }
        else
        {
            Response->SetField(TEXT("id"), MakeShareable(new FJsonValueNull()));
        }
        
        return Response;
    }
    
    // 로깅과 에러 응답을 함께 처리
    static TSharedPtr<FJsonObject> LogAndCreateError(EMCPErrorCode Code, const FString& Message, const FString& Context = TEXT(""))
    {
        // 에러 레벨에 따른 로깅
        if (Code == EMCPErrorCode::PermissionDenied)
        {
            UE_LOG(LogMCPSecurity, Warning, TEXT("Security Error [%s]: %s"), *Context, *Message);
        }
        else if (Code == EMCPErrorCode::ServerOverloaded)
        {
            UE_LOG(LogMCPPerformance, Warning, TEXT("Performance Error [%s]: %s"), *Context, *Message);
        }
        else
        {
            UE_LOG(LogMCPServer, Error, TEXT("Error [%s]: %s"), *Context, *Message);
        }
        
        return CreateErrorResponse(Code, Message);
    }
    
    // 에러 코드를 읽기 쉬운 문자열로 변환
    static FString ErrorCodeToString(EMCPErrorCode Code)
    {
        return UEnum::GetValueAsString(Code);
    }
};
```

---

## 🚀 Phase 3: 고급 기능 (1-3개월)

### 목표
엔터프라이즈급 보안, 성능 최적화, AI 통합을 통한 혁신적 기능 제공

### 1. 인증 및 보안 시스템 (단순한 토큰 기반)

```cpp
// 단순한 API 키 정보 구조체
USTRUCT()
struct UNREALBLUEPRINTMCP_API FMCPApiKeyInfo
{
    GENERATED_BODY()

    UPROPERTY()
    FString KeyName;
    
    UPROPERTY()
    FString HashedKey;  // 실제 키는 해시로 저장
    
    UPROPERTY()
    TArray<FString> AllowedMethods;
    
    UPROPERTY()
    FDateTime ExpirationDate;
    
    UPROPERTY()
    bool bIsActive = true;
    
    UPROPERTY()
    int32 RequestCount = 0;  // 사용량 추적
};

// 단순한 인증 관리자
class UNREALBLUEPRINTMCP_API FMCPAuthManager
{
private:
    TMap<FString, FMCPApiKeyInfo> ApiKeys;  // KeyId -> KeyInfo
    TMap<FString, FDateTime> RecentRequests; // IP -> LastRequestTime (Rate limiting)
    
public:
    // API 키 생성 (단순한 GUID 사용)
    FString CreateApiKey(const FString& KeyName, const TArray<FString>& AllowedMethods, int32 ExpirationDays = 365)
    {
        FString NewKeyId = FGuid::NewGuid().ToString();
        FString ActualKey = FGuid::NewGuid().ToString(); // 실제 키
        
        FMCPApiKeyInfo KeyInfo;
        KeyInfo.KeyName = KeyName;
        KeyInfo.HashedKey = FMD5::HashAnsiString(*ActualKey); // 간단한 해시
        KeyInfo.AllowedMethods = AllowedMethods;
        KeyInfo.ExpirationDate = FDateTime::Now() + FTimespan::FromDays(ExpirationDays);
        KeyInfo.bIsActive = true;
        
        ApiKeys.Add(NewKeyId, KeyInfo);
        
        UE_LOG(LogMCPSecurity, Log, TEXT("Created API key: %s for %s"), *NewKeyId, *KeyName);
        return ActualKey; // 실제 키 반환 (한 번만)
    }
    
    // 간단한 인증 검사
    bool ValidateRequest(const FString& ProvidedKey, const FString& Method, const FString& ClientIP)
    {
        // Rate limiting 체크 (단순한 방식)
        if (IsRateLimited(ClientIP))
        {
            UE_LOG(LogMCPSecurity, Warning, TEXT("Rate limited IP: %s"), *ClientIP);
            return false;
        }
        
        // API 키 검증
        FString HashedProvidedKey = FMD5::HashAnsiString(*ProvidedKey);
        
        for (auto& KeyPair : ApiKeys)
        {
            FMCPApiKeyInfo& KeyInfo = KeyPair.Value;
            
            if (KeyInfo.HashedKey == HashedProvidedKey && 
                KeyInfo.bIsActive && 
                KeyInfo.ExpirationDate > FDateTime::Now())
            {
                // 메서드 권한 체크
                if (KeyInfo.AllowedMethods.Contains(Method) || KeyInfo.AllowedMethods.Contains(TEXT("*")))
                {
                    KeyInfo.RequestCount++;
                    RecordRequest(ClientIP);
                    return true;
                }
            }
        }
        
        UE_LOG(LogMCPSecurity, Warning, TEXT("Authentication failed for method %s from %s"), *Method, *ClientIP);
        return false;
    }

private:
    bool IsRateLimited(const FString& ClientIP)
    {
        FDateTime* LastRequest = RecentRequests.Find(ClientIP);
        if (LastRequest)
        {
            // 1초에 10요청 제한 (단순한 방식)
            return (FDateTime::Now() - *LastRequest).GetTotalSeconds() < 0.1;
        }
        return false;
    }
    
    void RecordRequest(const FString& ClientIP)
    {
        RecentRequests.Add(ClientIP, FDateTime::Now());
    }
};
```

### 2. 성능 최적화 (UE5 내장 기능 활용)

```cpp
// 단순한 객체 풀 (복잡한 템플릿 없음)
class UNREALBLUEPRINTMCP_API FMCPJsonObjectPool
{
private:
    TQueue<TSharedPtr<FJsonObject>> AvailableObjects;
    FCriticalSection PoolMutex;
    int32 MaxPoolSize = 100;
    
public:
    TSharedPtr<FJsonObject> AcquireObject()
    {
        FScopeLock Lock(&PoolMutex);
        
        TSharedPtr<FJsonObject> Object;
        if (AvailableObjects.Dequeue(Object))
        {
            Object->Values.Empty(); // 재사용을 위해 초기화
            return Object;
        }
        
        // 풀이 비어있으면 새로 생성
        return MakeShareable(new FJsonObject);
    }
    
    void ReturnObject(TSharedPtr<FJsonObject> Object)
    {
        if (!Object.IsValid()) return;
        
        FScopeLock Lock(&PoolMutex);
        
        if (AvailableObjects.Num() < MaxPoolSize)
        {
            Object->Values.Empty(); // 데이터 초기화
            AvailableObjects.Enqueue(Object);
        }
        // MaxPoolSize 초과 시 자동으로 소멸
    }
};

// 연결 풀 관리 (단순한 구조)
class UNREALBLUEPRINTMCP_API FMCPConnectionManager
{
private:
    TArray<TSharedPtr<FMCPConnection>> ActiveConnections;
    FCriticalSection ConnectionsMutex;
    UMCPServerSettings* Settings;
    
public:
    FMCPConnectionManager()
    {
        Settings = UMCPServerSettings::Get();
    }
    
    bool CanAcceptNewConnection() const
    {
        FScopeLock Lock(&ConnectionsMutex);
        return ActiveConnections.Num() < Settings->MaxConcurrentConnections;
    }
    
    void AddConnection(TSharedPtr<FMCPConnection> Connection)
    {
        FScopeLock Lock(&ConnectionsMutex);
        ActiveConnections.Add(Connection);
        
        UE_LOG(LogMCPPerformance, Log, TEXT("New connection added. Total: %d/%d"), 
               ActiveConnections.Num(), Settings->MaxConcurrentConnections);
    }
    
    void RemoveConnection(TSharedPtr<FMCPConnection> Connection)
    {
        FScopeLock Lock(&ConnectionsMutex);
        ActiveConnections.Remove(Connection);
        
        UE_LOG(LogMCPPerformance, Log, TEXT("Connection removed. Total: %d"), ActiveConnections.Num());
    }
    
    void CleanupDeadConnections()
    {
        FScopeLock Lock(&ConnectionsMutex);
        
        int32 RemovedCount = ActiveConnections.RemoveAll([](const TSharedPtr<FMCPConnection>& Conn)
        {
            return !Conn.IsValid() || !Conn->IsAlive();
        });
        
        if (RemovedCount > 0)
        {
            UE_LOG(LogMCPPerformance, Log, TEXT("Cleaned up %d dead connections"), RemovedCount);
        }
    }
};
```

### 3. 실시간 WebSocket 지원 (단순한 이벤트 시스템)

```cpp
// 간단한 WebSocket 연결 클래스
class UNREALBLUEPRINTMCP_API FMCPWebSocketConnection
{
private:
    FGuid ConnectionId;
    FString ClientIP;
    TArray<FString> SubscribedEvents;
    FSocket* Socket;
    bool bIsAlive = true;
    
public:
    FMCPWebSocketConnection(FSocket* InSocket, const FString& InClientIP)
        : ConnectionId(FGuid::NewGuid())
        , ClientIP(InClientIP)
        , Socket(InSocket)
    {
    }
    
    // 이벤트 구독 (단순한 문자열 배열)
    void SubscribeToEvent(const FString& EventType)
    {
        if (!SubscribedEvents.Contains(EventType))
        {
            SubscribedEvents.Add(EventType);
            UE_LOG(LogMCPServer, Log, TEXT("Client %s subscribed to %s"), *ConnectionId.ToString(), *EventType);
        }
    }
    
    void UnsubscribeFromEvent(const FString& EventType)
    {
        SubscribedEvents.Remove(EventType);
        UE_LOG(LogMCPServer, Log, TEXT("Client %s unsubscribed from %s"), *ConnectionId.ToString(), *EventType);
    }
    
    // 이벤트 전송 (JSON 문자열로 단순하게)
    bool SendEvent(const FString& EventType, TSharedPtr<FJsonObject> EventData)
    {
        if (!SubscribedEvents.Contains(EventType) || !bIsAlive || !Socket)
        {
            return false;
        }
        
        // 간단한 WebSocket 프레임 생성
        TSharedPtr<FJsonObject> Message = MakeShareable(new FJsonObject);
        Message->SetStringField(TEXT("event"), EventType);
        Message->SetObjectField(TEXT("data"), EventData);
        Message->SetStringField(TEXT("timestamp"), FDateTime::Now().ToIso8601());
        
        FString MessageStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MessageStr);
        FJsonSerializer::Serialize(Message.ToSharedRef(), Writer);
        
        // WebSocket 프레임으로 포장하여 전송
        return SendWebSocketFrame(MessageStr);
    }
    
    bool IsAlive() const { return bIsAlive && Socket; }
    FGuid GetId() const { return ConnectionId; }

private:
    bool SendWebSocketFrame(const FString& Data)
    {
        // 단순한 WebSocket 텍스트 프레임 생성
        // (실제 구현에서는 WebSocket 라이브러리 사용 권장)
        if (!Socket) return false;
        
        FTCHARToUTF8 DataUTF8(*Data);
        int32 BytesSent = 0;
        return Socket->Send((uint8*)DataUTF8.Get(), DataUTF8.Length(), BytesSent);
    }
};

// 이벤트 브로드캐스터 (싱글톤 없이 단순하게)
class UNREALBLUEPRINTMCP_API FMCPEventBroadcaster
{
private:
    TArray<TSharedPtr<FMCPWebSocketConnection>> Connections;
    FCriticalSection ConnectionsMutex;
    
public:
    void AddConnection(TSharedPtr<FMCPWebSocketConnection> Connection)
    {
        FScopeLock Lock(&ConnectionsMutex);
        Connections.Add(Connection);
    }
    
    void RemoveConnection(TSharedPtr<FMCPWebSocketConnection> Connection)
    {
        FScopeLock Lock(&ConnectionsMutex);
        Connections.Remove(Connection);
    }
    
    // 블루프린트 변경 이벤트 브로드캐스트
    void BroadcastBlueprintChanged(const FString& BlueprintPath)
    {
        TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject);
        EventData->SetStringField(TEXT("blueprint_path"), BlueprintPath);
        EventData->SetStringField(TEXT("change_type"), TEXT("modified"));
        
        BroadcastEvent(TEXT("blueprint_changed"), EventData);
    }
    
    // 에셋 생성 이벤트 브로드캐스트
    void BroadcastAssetCreated(const FString& AssetPath, const FString& AssetType)
    {
        TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject);
        EventData->SetStringField(TEXT("asset_path"), AssetPath);
        EventData->SetStringField(TEXT("asset_type"), AssetType);
        
        BroadcastEvent(TEXT("asset_created"), EventData);
    }

private:
    void BroadcastEvent(const FString& EventType, TSharedPtr<FJsonObject> EventData)
    {
        FScopeLock Lock(&ConnectionsMutex);
        
        // 죽은 연결 제거
        Connections.RemoveAll([](const TSharedPtr<FMCPWebSocketConnection>& Conn)
        {
            return !Conn.IsValid() || !Conn->IsAlive();
        });
        
        // 살아있는 연결들에게 이벤트 전송
        for (auto& Connection : Connections)
        {
            if (Connection.IsValid() && Connection->IsAlive())
            {
                Connection->SendEvent(EventType, EventData);
            }
        }
        
        UE_LOG(LogMCPServer, Log, TEXT("Broadcasted %s event to %d connections"), *EventType, Connections.Num());
    }
};
```

### 4. AI 통합 시스템 (단순한 HTTP 클라이언트)

```cpp
// 간단한 AI 서비스 클라이언트
class UNREALBLUEPRINTMCP_API FMCPAIServiceClient
{
private:
    FString ApiEndpoint;
    FString ApiKey;
    
public:
    FMCPAIServiceClient(const FString& InEndpoint, const FString& InApiKey)
        : ApiEndpoint(InEndpoint)
        , ApiKey(InApiKey)
    {
    }
    
    // 블루프린트 생성 요청 (비동기)
    void GenerateBlueprint(const FString& Description, const FString& TargetPath, 
                          TFunction<void(bool, const FString&)> OnComplete)
    {
        // 단순한 HTTP 요청 생성
        TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(ApiEndpoint + TEXT("/generate-blueprint"));
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
        
        // 요청 데이터 생성
        TSharedPtr<FJsonObject> RequestData = MakeShareable(new FJsonObject);
        RequestData->SetStringField(TEXT("description"), Description);
        RequestData->SetStringField(TEXT("target_path"), TargetPath);
        RequestData->SetStringField(TEXT("engine_version"), TEXT("5.6"));
        
        FString RequestBody;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
        FJsonSerializer::Serialize(RequestData.ToSharedRef(), Writer);
        Request->SetContentAsString(RequestBody);
        
        // 응답 처리
        Request->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                // 성공 응답 처리
                FString ResponseStr = Response->GetContentAsString();
                TSharedPtr<FJsonObject> ResponseJson;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
                
                if (FJsonSerializer::Deserialize(Reader, ResponseJson))
                {
                    FString GeneratedBlueprintCode = ResponseJson->GetStringField(TEXT("blueprint_code"));
                    OnComplete(true, GeneratedBlueprintCode);
                    return;
                }
            }
            
            // 실패 처리
            FString ErrorMsg = FString::Printf(TEXT("AI service error: %d"), 
                                             Response.IsValid() ? Response->GetResponseCode() : 0);
            OnComplete(false, ErrorMsg);
        });
        
        // 요청 전송
        Request->ProcessRequest();
    }
    
    // 블루프린트 최적화 제안
    void SuggestOptimizations(UBlueprint* Blueprint, TFunction<void(bool, TArray<FString>)> OnComplete)
    {
        if (!Blueprint)
        {
            OnComplete(false, {});
            return;
        }
        
        // 블루프린트 정보 수집 (단순하게)
        TSharedPtr<FJsonObject> BlueprintInfo = MakeShareable(new FJsonObject);
        BlueprintInfo->SetStringField(TEXT("name"), Blueprint->GetName());
        BlueprintInfo->SetNumberField(TEXT("variable_count"), Blueprint->NewVariables.Num());
        BlueprintInfo->SetNumberField(TEXT("function_count"), Blueprint->FunctionGraphs.Num());
        
        // HTTP 요청... (위와 유사한 패턴)
    }
};
```

---

## 🎯 코딩 스타일 가이드라인

### 핵심 원칙
1. **단순함이 최고**: 복잡한 디자인 패턴보다는 직관적이고 이해하기 쉬운 코드
2. **UE5 표준 활용**: 언리얼 엔진의 기본 기능과 컨벤션을 최대한 활용
3. **가독성 우선**: 성능보다는 코드를 읽고 이해하기 쉽게 작성
4. **명확한 네이밍**: 변수명, 함수명만 봐도 기능을 알 수 있도록

### 구체적 지침

#### 1. 클래스 설계
```cpp
// ✅ 좋은 예: 단순하고 명확한 구조
class FMCPResourcesHandler
{
public:
    bool CanHandle(const FString& Method) const;
    TSharedPtr<FJsonObject> HandleRequest(const FString& Method, TSharedPtr<FJsonObject> Params);

private:
    TSharedPtr<FJsonObject> HandleListAssets(TSharedPtr<FJsonObject> Params);
    TSharedPtr<FJsonObject> HandleGetAsset(TSharedPtr<FJsonObject> Params);
};

// ❌ 피해야 할 예: 과도하게 복잡한 구조
template<typename TRequestType, typename TResponseType>
class TMCPGenericHandlerWithFactoryAndBuilder
{
    // 복잡한 템플릿과 다중 상속 등...
};
```

#### 2. 함수 작성
```cpp
// ✅ 좋은 예: 한 가지 일만 하는 명확한 함수
bool IsValidAssetPath(const FString& AssetPath)
{
    if (AssetPath.IsEmpty()) return false;
    if (!AssetPath.StartsWith(TEXT("/Game/"))) return false;
    if (AssetPath.Contains(TEXT(".."))) return false; // 경로 탐색 방지
    return true;
}

// ❌ 피해야 할 예: 여러 일을 하는 복잡한 함수
auto ProcessRequestWithValidationAndLoggingAndCaching(auto&& request) -> decltype(auto)
{
    // 복잡한 람다와 auto, decltype 남용...
}
```

#### 3. 에러 처리
```cpp
// ✅ 좋은 예: 명확한 에러 메시지와 처리
TSharedPtr<FJsonObject> HandleGetAsset(TSharedPtr<FJsonObject> Params)
{
    if (!Params.IsValid())
    {
        UE_LOG(LogMCPServer, Warning, TEXT("HandleGetAsset: Missing parameters"));
        return FMCPErrorHelper::CreateErrorResponse(EMCPErrorCode::InvalidParams, TEXT("Parameters are required"));
    }
    
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        UE_LOG(LogMCPServer, Warning, TEXT("HandleGetAsset: Missing asset_path parameter"));
        return FMCPErrorHelper::CreateErrorResponse(EMCPErrorCode::InvalidParams, TEXT("asset_path parameter is required"));
    }
    
    // 실제 처리 로직...
}
```

#### 4. UE5 기능 활용
```cpp
// ✅ 좋은 예: UE5 내장 기능 적극 활용
UPROPERTY(config, EditAnywhere, Category = "Server Settings")
int32 ServerPort = 8080;

UENUM(BlueprintType)
enum class EMCPServerState : uint8
{
    Stopped,
    Running,
    Error
};

// UE5 로깅 시스템 사용
DECLARE_LOG_CATEGORY_EXTERN(LogMCPServer, Log, All);

// UE5 설정 시스템 사용
UMCPServerSettings* Settings = GetMutableDefault<UMCPServerSettings>();
```

#### 5. 변수 및 함수 명명
```cpp
// ✅ 좋은 예: 명확하고 설명적인 이름
bool bIsServerRunning = false;
int32 ActiveConnectionCount = 0;
TArray<FString> AllowedAssetPaths;

void StartHttpServer();
void HandleIncomingConnection(FSocket* ClientSocket);
bool ValidateApiKey(const FString& ProvidedKey);

// ❌ 피해야 할 예: 축약되거나 모호한 이름
bool bRun = false;
int32 cnt = 0;
TArray<FString> paths;

void Start();
void Handle(FSocket* s);
bool Check(const FString& k);
```

---

## 📅 구현 우선순위 및 타임라인

### Phase 2 - Week 1 (높은 우선순위)
1. **모듈형 핸들러 시스템** (3일)
   - `FMCPHandler` 베이스 클래스 생성
   - `FMCPResourcesHandler`, `FMCPToolsHandler` 구현
   - 기존 하드코딩된 로직을 핸들러로 이동

2. **설정 시스템** (2일)
   - `UMCPServerSettings` 클래스 생성
   - 에디터 UI에서 설정 가능하도록 연동
   - 기존 하드코딩된 값들을 설정으로 이동

### Phase 2 - Week 2 (중간 우선순위)
3. **상태 관리 시스템** (2일)
   - `FMCPServerStateManager` 구현
   - 서버 상태 추적 및 보고 기능

4. **에러 처리 개선** (2일)
   - `FMCPErrorHelper` 클래스 구현
   - 구조화된 에러 응답 시스템

5. **로깅 시스템** (1일)
   - UE5 로깅 카테고리 설정
   - 상세 로깅 옵션 구현

### Phase 3 - Month 1 (보안 및 성능)
6. **인증 시스템** (1주)
   - `FMCPAuthManager` 구현
   - API 키 생성/관리 기능

7. **Rate Limiting** (3일)
   - 간단한 요청 제한 시스템
   - IP 기반 제한 로직

8. **성능 최적화** (1주)
   - `FMCPJsonObjectPool` 구현
   - `FMCPConnectionManager` 구현

### Phase 3 - Month 2 (실시간 기능)
9. **WebSocket 지원** (2주)
   - `FMCPWebSocketConnection` 구현
   - `FMCPEventBroadcaster` 구현

10. **실시간 이벤트** (1주)
    - 블루프린트 변경 감지
    - 에셋 생성/수정 이벤트

### Phase 3 - Month 3 (AI 통합)
11. **AI 서비스 클라이언트** (2주)
    - `FMCPAIServiceClient` 구현
    - HTTP 기반 AI API 통신

12. **고급 블루프린트 편집** (2주)
    - 노드 그래프 조작 API
    - 블루프린트 분석 기능

---

## 🔧 개발 환경 설정

### 필요한 도구
- Unreal Engine 5.6+
- Visual Studio 2022
- Git (버전 관리)

### 권장 개발 플러그인
- Visual Studio Tools for Unreal Engine
- UnrealVS (언리얼 프로젝트 관리)

### 코드 포맷팅
- Unreal Engine 표준 코딩 컨벤션 준수
- .clang-format 파일 사용 (UE5 표준)

---

## 📝 테스트 계획

### 단위 테스트
- 각 핸들러별 기능 테스트
- 에러 처리 시나리오 테스트
- 인증 시스템 테스트

### 통합 테스트
- 전체 API 엔드포인트 테스트
- WebSocket 연결 및 이벤트 테스트
- 성능 및 메모리 사용량 테스트

### 보안 테스트
- 인증 우회 시도 테스트
- Rate Limiting 테스트
- 입력 검증 테스트

---

## 📖 문서화 계획

### 개발자 문서
- API 레퍼런스
- 아키텍처 설명서
- 코딩 가이드라인

### 사용자 문서
- 설치 및 설정 가이드
- 사용 예제
- 문제 해결 가이드

---

**이 문서는 UnrealBlueprintMCP 플러그인의 장기적 발전을 위한 로드맵입니다. 단계별로 착실히 구현하여 안정적이고 혁신적인 도구로 발전시켜 나갑시다!**