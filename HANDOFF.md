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
- El primer build con Qt 6.11.1 MinGW detectó que `TlHelp32.h` se incluía antes
  de `windows.h`; el orden de los headers queda corregido en el commit de
  seguimiento para que MinGW conozca `HANDLE`, `DWORD` y `PROCESSENTRY32W`.
- El build requiere Qt 6 y CMake/compilador de Windows; no están instalados en
  este entorno Linux, por lo que debe ejecutarse en Windows/CI.
- Prueba manual: iniciar con cuenta A, cerrar el juego, iniciar con cuenta B y
  comprobar `game\bin\win64\D2MAX\steam_api.ini`; el `FallbackAccountId` debe
  cambiar antes de cada lanzamiento. Confirmar también que no quede
  `dota2.exe` con esa ruta en Task Manager.

## Tienda integrada

El botón `TIENDA` cambia a una vista Qt nativa dentro del launcher. La vista
consume catálogo, filtros, saldo, inventario, historial y compra directamente
con el bearer token del perfil activo; no abre navegador, no usa cookies y no
crea una segunda sesión. Las respuestas HTTP 401 muestran una puerta de sesión
expirada que permite volver al login del launcher.

La prueba manual en Windows debe cubrir: iniciar sesión con A, pulsar `TIENDA`,
comprar un producto activo, confirmar el artículo en el inventario de Dota,
cerrar/reabrir el juego y repetir con la cuenta B para verificar que el código y
la identidad no se mezclan.

## Sesión 22 — tienda, estado del servidor y UI principal

Se corrigieron los pendientes de la revisión del launcher:

- El botón `Agregar cuenta` ya no se muestra en el dashboard. El login inicial
  sigue funcionando desde `main.cpp` y el cierre de sesión vuelve a abrir el
  diálogo existente.
- El indicador que aparecía como `--` ahora está conectado a
  `ServerClient::pingFinished` y muestra `COMPROBANDO`, `SERVIDOR EN LÍNEA`
  con versión o `SERVIDOR SIN RESPUESTA`, con una pastilla de estado visible.
- `MainWindow` usa un dashboard más amplio con tarjetas, gradientes, sombras,
  bordes suaves, botones de acción y un tema oscuro glassmorphism. La lógica de
  cuenta, tienda y lanzamiento conserva sus contratos existentes.
- La autodetección ya no depende de cinco rutas fijas. Valida el archivo real,
  acepta un ejecutable o una raíz de Dota/Steam, revisa el path guardado, VDF de
  bibliotecas Steam, registro de Windows, variables de entorno y candidatos
  comunes de las unidades. `play()` vuelve a resolver la ruta antes de lanzar.
- Cuando el launcher llama a `/api/presence/offline`, el servidor revoca el
  bearer activo; cualquier llamada posterior a la tienda nativa recibe 401.

`git diff --check` pasa. La configuración CMake fue intentada en Linux, pero
este entorno no tiene Qt6 instalado; la compilación final debe ejecutarse en la
máquina Windows/CI con Qt6 y el payload `steam_api64.dll` disponible.

## Sesión 23 — rediseño total del dashboard

`MainWindow` conserva las conexiones de login, estado del servidor, avatar,
rank, navegación de tienda, autodetección, configuración de argumentos y
lanzamiento, pero ahora presenta una UI completamente nueva:

- barra lateral con marca, navegación a tienda, estado de sesión y cierre de
  sesión;
- portada principal con acción `JUGAR DOTA 2` y tarjeta de cuenta activa;
- métricas separadas para rango, nivel y entorno;
- panel único de configuración para ruta de Dota, servidor y opciones de
  lanzamiento;
- tema visual nuevo con jerarquía glassmorphism, gradientes, sombras, estados y
  tarjetas responsive para el dashboard.

Verificación: `git diff --check` pasa. CMake se configuró hasta el chequeo de
dependencia, pero este entorno Linux no tiene Qt6; el build MinGW/Qt6 y la
prueba visual de la nueva UI deben ejecutarse en Windows/CI. La validación
pendiente también debe comprar un item desde la tienda y comprobar que Dota lo
recibe sin reinicio.

## Sesión 24 — tienda nativa y refinamiento visual

Se reemplazó la navegación web por una experiencia integrada:

- `StoreView` incorpora encabezado, saldo, filtros, catálogo paginado, precios
  Steam/locales, compra, inventario y actividad en tarjetas con scroll;
- el token del perfil se pasa directamente a `ServerClient` para todas las
  llamadas `/api/store/*`, eliminando el problema de sesión desalineada entre
  launcher y navegador;
- el dashboard ahora usa `QStackedWidget`, navegación `INICIO`/`TIENDA`, scroll
  vertical y geometría más amplia para evitar cortes de contenido;
- el tema define bordes, espacios, alturas, estados, botones y tarjetas de la
  tienda con una escala consistente;
- sin sesión, la tienda cambia a una pantalla informativa con acceso directo al
  diálogo de login, oculta filtros y controles de compra y no deja que una
  respuesta atrasada vuelva a mostrar datos de la cuenta anterior.

La compilación Qt6 y la validación visual/funcional con Windows siguen pendientes
porque Qt6 y el cliente Dota objetivo no están disponibles en este entorno. La
configuración CMake llega correctamente al chequeo de Qt6; la prueba pendiente
debe abrir la tienda sin sesión, iniciar sesión desde la propia vista y comprar
un artículo sin reiniciar Dota.

## Sesión 25 — ranking nativo del launcher

La UI modificada en `origin/main` quedó sincronizada primero y se conserva como
base. Sobre ese diseño compacto se agregó una tercera sección nativa:

- `MainWindow` añade `RANKING` al rail lateral y un `QStackedWidget` con
  `RankingView`; el login, logout y cambio de perfil actualizan la tienda y el
  ranking con el mismo bearer token.
- `ServerClient` consume `GET /api/ranking?page=1&pageSize=50` y conserva el
  SteamId como texto para no perder precisión al parsear ids de 64 bits.
- La vista muestra posición, icono de medalla, nombre, presencia, MMR, nombre
  de medalla/estrellas y partidas, victorias, derrotas y porcentaje de victoria.
  Sus tarjetas mantienen los mismos márgenes, radios, bordes, gradientes y
  densidad visual de la UI de tienda actual.
- El endpoint del servidor lista solo cuentas calibradas y ordena por MMR
  descendente, victorias, partidas y AccountId. La respuesta paginada permite
  que la tabla no dependa de cargar todos los usuarios en el launcher.
- Sesión requerida, carga, error y `Aún no hay jugadores clasificados` son
  estados visuales independientes; el ranking vacío no se confunde con una
  caída del servidor.

Fuente de medallas verificada: el tracking público de los archivos de Dota 2
identifica `dota/pak01_dir.vpk` y las rutas internas
`panorama/images/rank_tier_icons/rank0_psd.vtex_c` hasta
`rank8_psd.vtex_c`. Son recursos Valve compilados (`.vtex_c`), no PNG que Qt
pueda abrir directamente. Por eso `RankingView` usa un badge vectorial local
por tier, sin CDN ni sesión web; el comentario del código conserva la ruta
oficial para sustituirlo por una extracción del VPK cuando se distribuya esa
herramienta/asset en el entorno Windows.

Evidencia: `git diff --check` pasa. CMake alcanza el chequeo de Qt6 pero este
entorno Linux no tiene Qt6 instalado; falta compilar/visualizar en Windows o
CI con Qt6 y comprobar el ranking contra una base con partidas calibradas.
