# D2Max Launcher (Qt6 / C++)

Launcher de escritorio para **D2STServer**. Dota 2, con **multi-cuenta**: cada cuenta es un usuario del
propio servidor D2 (el primer login crea el usuario en el server), se cambia de
cuenta con un clic y el juego se lanza con la identidad de la cuenta activa.

Inspirado en el launcher WPF de SKYNET escrito en **Qt 6 / C++** (Widgets + Network).

## Funcionalidades

- **Login con usuario y contraseña** contra `POST /api/auth/login` del servidor
  D2. Los usuarios se crean automáticamente en el server al primer login.
- **Multi-cuenta**: perfiles guardados localmente; selector de cuenta en la
  ventana principal; cada cuenta conserva su token y su identidad
  (AccountId/SteamId) para el lanzamiento.
- **Indicador de servidor**: comprobación periódica de `GET /api/version`
  (en línea / fuera de línea + versión).
- **Tienda web sin segundo login**: el botón `TIENDA` solicita un código
  temporal al servidor y abre `/store` en el navegador; la cuenta es siempre el
  perfil activo y el token permanente no se expone en la URL.
- **Perfil de la cuenta activa**: nombre, AccountId, SteamId y nivel desde
  `GET /api/users/me`.
- **Detección de Dota 2**: auto-detección por rutas comunes y registro de Steam
  (Windows), o selección manual de `dota2.exe`.
- **Lanzamiento con inyección**: `CreateProcessW` suspendido, inyección de
  `steam_api64.dll` (`CreateRemoteThread`/`LoadLibraryW`) o redirección del
  import estático, sin tocar la carpeta del juego. El payload se copia a un
  shadow path en `%TEMP%`.
- **Opciones de lanzamiento**: `-console`, `-novid`, `-insecure`.
- **steam_api.ini por partida**: se escribe en la carpeta que realmente lee el
  shim: `<dota2.exe dir>\D2MAX\steam_api.ini`
  con AppId 570, ServerUrl, identidad de la cuenta activa
  (`FallbackAccountId` / `FallbackPersonaName`), `SecureNetworking=false`, etc.
- **Modo headless de prueba** (pensado para CI): `D2MaxLauncher.exe --launch <ruta a dota2.exe>`.

## Requisitos

- Windows 10/11 (el lanzamiento con inyección usa Win32; la UI y el cliente de
  red son multiplataforma).
- Qt 6.x (Widgets + Network) y CMake 3.16+.
- Compilador MSVC (Visual Studio) o MinGW.
- `steam_api64.dll` (el shim de Dota 2 7.22g compilado) **junto al ejecutable
  del launcher**, o en `payload/x64/steam_api64.dll`. No se versiona en el repo.

## Compilar

```powershell
# Con Qt instalado y en el PATH (p. ej. C:\Qt\6.x.x\msvc2022_64\bin)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

El ejecutable queda en `build/Release/D2MaxLauncher.exe`. Copia
`steam_api64.dll` junto a él.

## Cómo funciona

1. En el login se llama a `POST /api/auth/login` con `{Username, Password}`; el
   server responde `{SteamId, AccountId, Token}` (los usuarios se registran al
   primer login).
2. Cada login crea/actualiza un **perfil** local. Al jugar, el launcher escribe
   `D2MAX\steam_api.ini` con el `FallbackAccountId` del perfil activo; el shim
   inyectado hace el logon del juego contra ese usuario del server D2.
3. El juego arranca suspendido, se inyecta `steam_api64.dll` (o se redirige el
   import estático si `dota2.exe` lo importa directamente) y se reanuda.

## Estructura

```
src/
  main.cpp
  config/    AppConfig + ConfigStore (config.json en %APPDATA%\D2MaxLauncher)
  net/       ServerClient (login, version, me, store handoff — JSON PascalCase)
  launch/    GameLauncher, DllInjector, PeUtils, IniGenerator, DotaPathDetector
  ui/        LoginDialog + MainWindow (tema oscuro QSS)
  util/      Log
resources/   theme.qss
```

## Hoja de ruta

- Validación end-to-end con el cliente real 7.22g + shim compilado en Windows.
- Persistir el token con caducidad y refresco (`/api/users/me`) en vez de
  revalidar solo al iniciar.
- Vistas de usuarios en línea y estado de partidas usando `GET /api/users`.
- Icono/avatares de cuenta desde `GET /api/users/{steamId}/avatar`.
- Tienda web same-origin desde el launcher con compra e inventario local.
- Instalador (windeployqt) y actualización automática del payload.
