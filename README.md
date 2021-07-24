
## C++ Console Chatting Program
이 프로젝트는 Windows에서 Visual Studio 2019로 개발하였습니다
Visual Studio 2019에서 빌드됩니다.

Server는 콘솔 채팅 서버
BClient는 봇 클라이언트
Client는 콘솔 클라이언트

### Server 빌드
https://github.com/Microsoft/hiredis 의 코드를 받아
hiredis, win32_interop 프로젝트를 열어
구성형식 :	정적 라이브러리(.lib)
Windows SDK 버전 : 10.0.XX
플랫폼 도구 집합 : Visual Studio 2019 (v142)로 빌드

Server프로젝트를 Server.sln을 클릭해 열고
포함 디렉토리에 hiredis경로를
라이브러리 디렉토리 hiredis\lib\x64경로를 셋팅하세요
구성형식 : 애플리케이션(.exe)
Windows SDK 버전 : 10.0.XX
플랫폼 도구 집합 : Visual Studio 2019 (v142)로 맞추고
Server프로젝트 빌드

### BClient, Client 빌드
각각 BClient.sln, Client.sln
구성형식 : 애플리케이션(.exe)
Windows SDK 버전 : 10.0.XX
플랫폼 도구 집합 : Visual Studio 2019 (v142)로 맞추고
프로젝트 빌드

Visual Studio 2019 프로젝트 구성 속성에서
구성	: Release
플랫폼 : x64
경고 수준 : 수준3(/W3)
SDL 검사 : 예(/sdl)
