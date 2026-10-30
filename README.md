동아리 학생 정보 관리 프로그램

## 로컬 환경 설정

이 프로그램은 C++ 콘솔에서 Supabase와 디스코드 웹훅에 연결합니다.

1. 저장소 루트의 .env.example을 .env로 복사합니다.
2. .env에 다음 세 설정을 채웁니다.
   - SUPABASE_ANON_KEY: Supabase anon 키
   - DISCORD_WEBHOOK_PROFILE_URL: 학생 조회 결과 전송용 디스코드 웹훅 URL
   - DISCORD_WEBHOOK_TIER_URL: 승급 알림 전송용 디스코드 웹훅 URL
3. Visual Studio에서 기존 studentsinf.sln을 빌드합니다.

프로그램은 OS 환경 변수 값을 우선하며, 없는 경우 프로젝트 루트의 .env를 사용합니다.
.env 파일은 .gitignore 대상이므로 Git에 추가하거나 채팅에 값을 공개하지 마세요.
기존 공개 Git 커밋에 포함되었던 디스코드 웹훅 URL은 별도로 폐기·재발급해야 합니다.
Supabase는 anon 키의 공개 여부와 별개로 RLS 정책으로 학생 테이블 접근을 제한해야 합니다.

주의: 프로그램을 실행하면 DB에 저장된 학생 정보를 불러와 플랫폼 티어를 갱신할 수 있습니다.
