# Diagnóstico PR18: evaluación, escala y búsqueda

Autorizado por el dueño el 31 de octubre de 2026. Banco fijado antes de examinar
errores: 256 historiales y 2.048 estados de decisión como máximo. No es una
candidata, entrenamiento, panel de fuerza ni autorización de promoción.

Base: PR18 `fb04cbca1a7584edd7a29fd5f0a19b28d551c217`, binario y red campeones
con sus SHA256 publicados. PR24 y todos los paneles anteriores siguen cerrados.

## Muestreo previo

Se toman 128 partidas de cada corpus de desarrollo PointsHead v1/v2, sólo de
familias cuyo SHA256 modulo diez pertenece a entrenamiento (2..9), con resultado
completo y al menos ocho muestras distintas. No se usan los conjuntos reservados
ni aperturas del panel. Una familia sólo puede aparecer una vez en el banco.
En cada corpus se eligen, por hash fijo, 16 partidas con reloj >=80, 16 con >=8
muros, 16 con <=6 piezas vivas, 16 con diferencia absoluta de puntos >=15 y 64
partidas restantes. Las categorías se asignan en ese orden, sin duplicar partidas.
Los ocho estados por partida son: reloj máximo, muros máximos, material vivo
mínimo, marcador más próximo a +/-20 y cuatro cuantiles temporales (10/35/60/85%).
Empates y selección dentro de categorías se resuelven por SHA256 con semilla
`pr18-diagnosis-v1-20261031`. No se selecciona por error, evaluación ni resultado.
Los estados de jugada forzada o reclamación exacta se conservan y se separan de
las métricas de elección. No son ocho observaciones independientes por partida.

## Comparaciones fijadas

T1, Hash128, cuatro motores como máximo; TT e historiales de búsqueda se vacían
antes de cada consulta. Siempre se envía la historia desde el inicio, nunca sólo
la FEN. Se conservarán todos los outputs, cotas, profundidad, nodos, tiempo y PV.

* Todo el banco: componentes de evaluación, todos los hijos legales y terminales
  inmediatos; PR18 a 4.096, 32.768 y 262.144 nodos.
* Un estado por historial, seleccionado por hash: PR18 y control menos selectivo
  a profundidad 4; control menos selectivo a 32.768 y 262.144 nodos; dos controles
  de escala a 32.768 nodos.
* Control menos selectivo: desactivar razoring, reverse futility, null move,
  IIR, ProbCut, pruning de movimientos, extensiones/reducciones singulares y LMR.
  Conserva alpha-beta/TT, quiescencia y reglas de PR18. No es minimax exacto.
* Control de escala simétrica: sólo en el factor material externo, sustituir
  214*peones blancos por 107*(peones blancos+negros), promedio de las dos
  orientaciones. No cambia red ni el ajuste interno entertainment.
* Control sin atenuación por reloj: retirar sólo el multiplicador externo
  (100-rule50)/100. Es una intervención diagnóstica fuera del contrato entrenado,
  no una mejora presumida ni una búsqueda de coeficientes.
* Simetría: invertir filas y colores, intercambiar marcador y turno, sólo para
  evaluación estática. No inventar historia para comparar búsquedas repetidas.

Los presupuestos se fijan aquí. Un piloto de ocho estados mide sólo viabilidad y
paridad del instrumental con el binario original. No cambia los presupuestos en
función de aciertos. Las consultas tienen límite de tiempo para detectar fallos;
una consulta que lo exceda se conserva como incompleta, nunca se rellena.

## Evidencia y atribución

El árbitro Chess.com congelado verificará las historias, estados, listas legales,
hijos terminales y PV. Las pruebas exactas se limitan a reglas/terminales y
reclamaciones comprobadas. Una PV ganadora no certifica todas las alternativas.
Los scores de búsqueda profunda y los resultados de la política que generó las
partidas son referencias provisionales, no etiquetas minimax.

Se medirán estabilidad de elección entre presupuestos, magnitud de cambios de
valor, ranking estático de alternativas, sensibilidad a escala y selectividad,
asimetría de color y calibración frente al resultado de la partida de origen.
Se separarán resultados por régimen, corpus y familia. Las diferencias de pérdida
y tasas se resumirán por historial con bootstrap emparejado, no por filas IID.
No se elegirá un control ganador por los mismos datos que explican sus efectos.
Un cambio de jugada aislado no prueba mejora; desacuerdo persistente no identifica
por sí solo la evaluación como causa. Se informarán límites y casos inconclusos.

Primero se exige paridad del modo baseline instrumental con PR18 y consistencia
Debug/Release de la nueva extracción. No se repiten los paneles ni las validaciones
históricas. Inputs, fuente instrumental, ejecutables y selección se sellarán antes
de las búsquedas del banco. Reserva de máquina hasta salida comprobada de todos
los procesos propios. T8 pausado, Fairyground cerrado, sin cargas remotas ni OB.
