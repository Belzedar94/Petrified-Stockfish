# Fairyground local con Petrified-Stockfish

Abrir: <http://127.0.0.1:5185/public/index.html?variant=petrified>.

La etiqueta inferior debe mostrar **Petrified-Stockfish + run4test · Motor nativo
listo**. El servidor carga automáticamente el mismo motor para blancas, negras
y análisis, con `Threads=1`, `Hash=128`, `Ponder=false` y la red run4test congelada.

- Para analizar, activa **Engine analysis** en **Moves**. Vuelve a desactivarlo
  para parar la búsqueda.
- **Engine** permite cambiar Lines (MultiPV), Threads, Memory y los límites
  Depth/Time/Nodes del motor nativo. Desactiva y reactiva **Engine analysis**
  después de cambiar los valores. Inicialmente usa un hilo y 128 MB de hash.
- Para jugar, pulsa **New game**. Blancas: **You**; negras: **External engine**
  (preseleccionado). El tiempo inicial del rival es 0,5 segundos por jugada.
- Para posiciones propias, usa **Setup**, pega el FEN y pulsa **Set position**
  y **Done**. Los muros se escriben como `*`.
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
.\scripts\setup_fairyground.ps1 -Engine .\src\petrified-bmi2-gui.exe `
  -Network '..\..\..\Match script\petrified_run4test_e19_l085.nnue'
.\scripts\start_fairyground.ps1 -OpenBrowser
```

El instalador fija la revisión y el lockfile, verifica el hash de la red,
conserva las modificaciones de un checkout incompatible y escribe
`.local/fairyground/petrified-profile.json` con hashes de los componentes.
`-InstallDependencies` vuelve a instalar exactamente el lockfile.

El parche y el perfil están en `integrations/fairyground/`. Usan el backend
nativo de Fairyground: el tablero sigue usando ffish-es6 y el motor que busca es
Petrified-Stockfish para Windows. Esto no es un port del motor de búsqueda al
navegador y tampoco sustituye al árbitro Walls de las campañas oficiales.

## Validación y aprendizaje

El anuncio UCI debe ser `option name UCI_Variant type combo default petrified var
petrified`. Fairyground lee la lista `var`; sin ella no reconoce la variante.
El parser de opciones también debe aceptar que el predeterminado aparezca en
esa lista. Ambas condiciones tienen cobertura de regresión.

Se comprobó análisis nativo con la red real, interrupción de búsqueda y una
partida desde `4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1`: `d1d5` produjo un muro y el
motor respondió `e8d7`. El binario de la campaña se conserva; el ejecutable de
GUI incorpora las correcciones de protocolo y conserva su bench de 179335.
