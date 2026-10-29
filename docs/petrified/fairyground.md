# Fairyground local con Petrified-Stockfish

Abrir: <http://127.0.0.1:5185/public/index.html?variant=petrified>.

La etiqueta inferior debe mostrar **Petrified-Stockfish + run4test PointsHead v1 + LegalQuery v1 (PR #11) ·
Game of Points activo**. Recarga la página tras actualizar el perfil. El servidor
carga automáticamente el mismo motor para blancas, negras y análisis, con
`Threads=1`, `Hash=128` y `Ponder=false`. La cabeza recibe el marcador; los
acumuladores y PSQT de run4test siguen congelados.

- Para analizar, activa **Engine analysis** en **Moves**. Vuelve a desactivarlo
  para parar la búsqueda.
- **Engine** permite cambiar Lines (MultiPV), Threads, Memory y los límites
  Depth/Time/Nodes del motor nativo. Desactiva y reactiva **Engine analysis**
  después de cambiar los valores. Inicialmente usa un hilo y 128 MB de hash.
- El perfil actual se usa para **análisis**. El marcador resaltado junto a cada
  jugador sigue las jugadas y la navegación; el panel muestra el resultado por
  puntos y separa el bonus final. Dar mate no implica ganar la partida.
- El análisis muestra **Mate in N** y `#N` cuando encuentra un mate que gana por puntos.
  Con 19 puntos de desventaja el mate gana, con 20 empata y con 21 pierde;
  estos dos últimos casos no se anuncian como un mate ganador.
- Para posiciones propias, usa **Setup**, pega el FEN de seis campos (muros `*`)
  y las jugadas UCI opcionales. Introduce los puntos de cada jugador **al inicio
  de ese FEN**, antes de cualquier bonus final, y pulsa **Aplicar marcador** y
  **Done**. El historial suma las capturas automáticamente. Si no confirmas los
  puntos de un FEN nuevo, el marcador queda pendiente y se bloquea su búsqueda.
- Una partida desde la posición inicial empieza en 0–0. En un FEN intermedio
  no se pueden inferir puntos a partir del material. Para el mate del ejemplo,
  `points=25,4` en la API significa 25–24 final; no introduzcas 25–24 como puntos
  previos o sumarías el mate dos veces.
- Para comprobar la red, activa **Engine debug** y selecciona **Analysis binary
  engine** o el motor de su color. La salida debe contener `NNUE PointsHead v1
  loaded` y el SHA-256
  `ab8bc1e4bb57be21eab8af061259c2bbcf065b6dfcd13c848e2a62b52739941c`.
  El mismo anuncio identifica la base run4test con SHA-256
  `345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933`.
- La red se carga desde el perfil nativo; no hay que subirla por el navegador.
  Las opciones adicionales del ejecutable están en **Engine management**.

Desde PowerShell, en la raíz del proyecto:

```powershell
.\scripts\start_fairyground.ps1 -OpenBrowser
.\scripts\stop_fairyground.ps1
```

El arranque reutiliza la instancia existente y comprueba los puertos. La parada
cierra únicamente este servidor y sus motores. El servidor escucha sólo en
`127.0.0.1:5185` y `127.0.0.1:5186`, admite el origen de esta página local y tiene
un límite de vida de 24 horas. Después se puede volver a arrancar. No modifica
ni detiene OpenBench.

## Reproducir la instalación

Origen: <https://github.com/ianfab/fairyground>, commit
`5f7392c3416b3949a3352d729f4e32368f03bade`. Node y npm deben estar disponibles.
Primero compila un ejecutable nativo del motor. La instalación utilizada aquí
está en `.local/fairyground`; pesos y ejecutables no se publican en el parche.

```powershell
# Con el entorno Emscripten activado:
python scripts/build_wasm.py --target cjs --out build/wasm-points-legal-query-v1
.\scripts\setup_fairyground.ps1
.\scripts\start_fairyground.ps1 -OpenBrowser
```

El instalador toma las rutas predeterminadas de `points-champion.json`; los
artefactos congelados deben existir localmente. `-Engine` y `-Network` permiten
indicar otras rutas a esos mismos archivos. Fija la revisión y el lockfile, verifica el hash de la red,
conserva las modificaciones de un checkout incompatible y escribe
`.local/fairyground/petrified-profile.json` con hashes de los componentes.
`-InstallDependencies` vuelve a instalar exactamente el lockfile.
`-BoardDirectory` permite seleccionar otro directorio CJS compilado. El motor
y el Board deben incluir la misma corrección de reglas; la instalación actual
usa `build/wasm-points-legal-query-v1/cjs` y la cabeza con marcador cualificada.
El perfil instalado añade `LegalQuery v1 (PR #11)` al nombre para distinguir
la nueva ganadora; el instalador puede mostrar sólo el nombre de la red v1.
La identidad definitiva es el hash del ejecutable y la red del perfil.

El parche y el perfil están en `integrations/fairyground/`. Usan el backend
nativo de Fairyground: la representación del tablero sigue usando ffish-es6,
sus jugadas legales y su resultado se consultan al módulo Board de Petrified-Stockfish, y la búsqueda
usa el motor nativo para Windows. Los puntos del marcador llegan de la misma
implementación C++. El árbitro independiente de las campañas de puntos sigue
siendo el cliente público de Chess.com, no este módulo Board.
Los historiales pegados se validan antes de enviarlos al motor. Un peón clavado
no puede moverse lateralmente, aunque continúe tapando la línea hacia su rey;
este caso tiene regresiones Python, UCI y CJS/ESM.

## Validación y aprendizaje

El anuncio UCI es `option name UCI_Variant type combo default petrified var
petrified var petrifiedpoints`. El perfil traduce la selección del tablero
`petrified` a `petrifiedpoints` para el motor. Fairyground lee la lista `var`.
El parser de opciones también debe aceptar que el predeterminado aparezca en
esa lista. Ambas condiciones tienen cobertura de regresión.

Se comprobó análisis nativo con la red real, interrupción de búsqueda y una
partida desde `4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1`: `d1d5` produjo un muro y el
motor respondió `e8d7`. El binario de la campaña se conserva; el ejecutable de
GUI incorporaba las correcciones de protocolo y conservaba su bench de 179335.

La actualización de puntos se comprobó con la partida real 111037628: antes del
último movimiento, 25–4; después de `f1g1`, 25–24 y victoria de blancas. Retroceder
y avanzar restaura ambos marcadores, y el análisis nativo arranca y se detiene
con los puntos del FEN. Captura local: `build/screenshots/fairyground-points.jpg`.

La revisión de reglas r1 rechazó el historial ilegal `f7e7` sin abrir un aviso
modal ni enviarlo al motor. El historial visible usa la API común de reglas
cuando el visor PGN experimental pierde su contenedor al alternar Setup/Moves.
Se comprobó de nuevo la partida completa y la navegación 25–4 / 25–24, incluido
el resultado `1-0` en la lista. El panel del resultado ocupa su fila bajo el
tablero y no se superpone al marcador. Captura actual:
`build/screenshots/fairyground-points-rules-r1.jpg`.

La revisión r2 muestra **Mate in 1** y `Qh7` en la partida del propietario,
con marcador previo 20–4. Jugar `22.Qh7#` produce 40–4 y `1-0`; retroceder
restaura 20–4 y permite volver a analizar. El motor es
`src/petrified-points-mate-r2-bmi2.exe`; los ejecutables anteriores se conservan.
Capturas: `build/screenshots/fairyground-winning-mate-r2.png` y
`build/screenshots/fairyground-winning-mate-result-r2.png`.

El 2026-10-29 se actualizó el perfil a points-head-v1 tras sus tres victorias
locales. La prueba HTTP/WebSocket verificó los archivos servidos, el hash completo
de la red cargada, `h2h7` con `score mate 1` y el final 25–24/`1-0` del mate que
pierde. El control visual del navegador no estuvo disponible en esa sesión;
las capturas anteriores documentan la revisión r2, no la nueva red.

El 2026-10-29 se instaló después la ganadora de PR #11, LegalQuery v1, tras
70/70/88 partidas y tres victorias auditadas frente a PR #7. Conserva exactamente
su red PointsHead v1 y todas las correcciones anteriores de puntos y mates;
acelera las consultas de existencia y pertenencia de jugadas legales.
El ejecutable es `.local/points-legal-query-v1-inputs/petrified-points-legal-query-v1-bmi2.exe`,
SHA256 `03de806f94fa12d92b5a35c3574518ed9f4433d404d06abb15b8597b169cbb45`.
La red es `.local/points-legal-query-v1-inputs/champion-v1.pnnue`, con el mismo
SHA256 `ab8bc1e4bb57be21eab8af061259c2bbcf065b6dfcd13c848e2a62b52739941c`.
El Board servido corresponde a la misma fuente `74ea5095`.

Pasaron HTTP/WebSocket, hashes de los archivos servidos, `h2h7` con mate ganador
y el final 25–24/`1-0`. El navegador mostró Petrified Chess, marcador 0–0 y el
perfil LegalQuery v1 con Game of Points activo. Se dejó el servidor abierto
por petición expresa del dueño, con el análisis desactivado inicialmente.
La evidencia está en `.local/points-legal-query-v1/fairyground-*-audit.json`;
el perfil anterior está preservado en el directorio del experimento.
