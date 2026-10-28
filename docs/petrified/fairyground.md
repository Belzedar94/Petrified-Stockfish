# Fairyground local con Petrified-Stockfish

Abrir: <http://127.0.0.1:5185/public/index.html?variant=petrified>.

La etiqueta inferior debe mostrar **Petrified-Stockfish + run4test · Game of Points
activo**. El servidor carga automáticamente el mismo motor para blancas, negras
y análisis, con `Threads=1`, `Hash=128`, `Ponder=false` y la red run4test congelada.

- Para analizar, activa **Engine analysis** en **Moves**. Vuelve a desactivarlo
  para parar la búsqueda.
- **Engine** permite cambiar Lines (MultiPV), Threads, Memory y los límites
  Depth/Time/Nodes del motor nativo. Desactiva y reactiva **Engine analysis**
  después de cambiar los valores. Inicialmente usa un hilo y 128 MB de hash.
- El perfil actual se usa para **análisis**. El marcador resaltado junto a cada
  jugador sigue las jugadas y la navegación; el panel muestra el resultado por
  puntos y separa el bonus final. Dar mate no implica ganar la partida.
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
  engine** o el motor de su color. La salida debe contener `NNUE Legacy Walls
  run4test loaded` y el SHA-256 `345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933`.
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
python scripts/build_wasm.py --target cjs --out build/wasm-points
.\scripts\setup_fairyground.ps1 -Engine .\src\petrified-points-rules-r1-bmi2.exe `
  -Network '..\..\..\Match script\petrified_run4test_e19_l085.nnue'
.\scripts\start_fairyground.ps1 -OpenBrowser
```

El instalador fija la revisión y el lockfile, verifica el hash de la red,
conserva las modificaciones de un checkout incompatible y escribe
`.local/fairyground/petrified-profile.json` con hashes de los componentes.
`-InstallDependencies` vuelve a instalar exactamente el lockfile.
`-BoardDirectory` permite seleccionar otro directorio CJS compilado. El motor
y el Board deben incluir la misma corrección de reglas; la instalación actual
usa `build/wasm-adapter-v1-r1/cjs` y conserva run4test como evaluación.

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
