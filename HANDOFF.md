# D2Max Launcher handoff

## Cuenta activa y `steam_api.ini`

El launcher guarda varios perfiles en la configuración local y `MainWindow::play`
selecciona el perfil cuyo nombre coincide con `CurrentUsername`. Antes del fix,
`IniGenerator` escribía `game\SKYNET\steam_api.ini`, pero el shim cargado por
`dota2.exe` usa `Process.MainModule.FileName` y lee
`game\bin\win64\D2MAX\steam_api.ini`. En el primer arranque el shim podía mover
`SKYNET` a `D2MAX`; después, al cambiar de cuenta, el launcher seguía actualizando
`SKYNET` mientras el shim continuaba leyendo el primer `D2MAX`. Ese era el dato
aparentemente hardcodeado.

El launcher ahora escribe atómicamente en `<dota2.exe dir>\D2MAX\steam_api.ini`,
con `FallbackPersonaName` y `FallbackAccountId` del perfil activo. No crea una
segunda ruta legacy ni sobrescribe datos de otra cuenta. `QSaveFile` evita que el
shim lea un INI parcial durante un cambio.

## Segundo arranque y procesos residuales

Antes de crear/iniciar una nueva instancia, `GameLauncher` inspecciona los
procesos `dota2.exe`, compara su ruta completa con la ruta seleccionada y termina
solo los procesos residuales de esa misma instalación. Espera hasta cinco segundos
para que desaparezcan; si alguno queda vivo, cancela el nuevo lanzamiento y muestra
el error. Esto cubre el caso en que la primera instancia queda en segundo plano y
Source 2 rechaza la siguiente.

## Verificación

- `git diff --check` y revisión estática de las rutas pasan en este entorno.
- El build requiere Qt 6 y CMake/compilador de Windows; no están instalados en
  este entorno Linux, por lo que debe ejecutarse en Windows/CI.
- Prueba manual: iniciar con cuenta A, cerrar el juego, iniciar con cuenta B y
  comprobar `game\bin\win64\D2MAX\steam_api.ini`; el `FallbackAccountId` debe
  cambiar antes de cada lanzamiento. Confirmar también que no quede
  `dota2.exe` con esa ruta en Task Manager.
