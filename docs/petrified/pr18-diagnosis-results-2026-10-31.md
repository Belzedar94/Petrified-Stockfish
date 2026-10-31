# Diagnóstico ejecutado de PR18: evaluación, escala y búsqueda

**Decisión: no entrenar todavía otra red.** La señal más sólida de este banco es
la dependencia del horizonte de búsqueda, especialmente al acercarse a una
liquidación. Hay una asimetría real en la escala heredada y carencias visibles de
la evaluación estática, pero este experimento no demuestra que la capacidad del
transformador NNUE sea el límite principal. Tampoco demuestra que retirar podas
o modificar la escala mejore el juego. PR18 sigue siendo la campeona; ninguna
intervención de diagnóstico se convierte en candidata.

El [protocolo previo](pr18-diagnosis-v1.md) se ejecutó sobre **256 historiales,
2.048 posiciones y 7.680 consultas**, con 656.978.472 nodos reportados en total.
No se generaron partidas ni se entrenó, ajustó o promovió ningún modelo.
Los [resultados numéricos](pr18-diagnosis-results-2026-10-31.json) conservan las
métricas, intervalos, casos, auditorías y hashes; el expediente local contiene
las historias completas, salidas originales y variantes independientes.

**Qué población se midió.** Son 128 partidas de desarrollo v1 y 128 de v2,
con familias distintas, ocho estados por partida y selección por hash fijada
antes de buscar. Se excluyeron familias reservadas y las aperturas del panel.
La estratificación sobrerrepresenta situaciones de interés: no estima su
frecuencia natural. Hay 68 estados con reloj ≥80, 716 con al menos ocho muros,
413 con seis piezas vivas o menos y 205 con diferencia absoluta de puntos ≥15;
estos grupos se solapan. Ninguna raíz está en jaque. Dos decisiones forzadas y
14 reclamaciones exactas se separan: quedan **2.032 decisiones de elección**.

La base real fue PR18 `fb04cbca1a7584edd7a29fd5f0a19b28d551c217`, exportada
aisladamente; no se incorporó código de PR19–24. Las búsquedas baseline usaron
el ejecutable campeón original. Los controles usaron instrumental Debug/Release
con la misma red, T1/Hash128 y cuatro motores como máximo, con historia completa
y TT/historiales de búsqueda vaciados antes de cada consulta. El piloto de ocho
estados verificó extracción idéntica Debug/Release, evaluación original idéntica
y búsqueda baseline instrumental idéntica al campeón a 32.768 nodos.

**La búsqueda aún cambia decisiones importantes.**

| Presupuesto PR18 | Brier frente al resultado de origen¹ | Profundidad mediana² |
|---|---:|---:|
| Evaluación estática | 0,10696 | — |
| 4.096 nodos | 0,09198 | 10 |
| 32.768 nodos | 0,08696 | 14 |
| 262.144 nodos | 0,08041 | 18 |

Entre 4.096 y 32.768 nodos cambian 920/2.032 jugadas (45,3%); entre 32.768 y
262.144 cambian 687/2.032 (33,8%). Sólo 865/2.032 (42,6%) coinciden en los tres
presupuestos. Un cambio no es un acierto ni una pérdida de Elo: puede haber
alternativas equivalentes. La variación media de la estimación entre los dos
presupuestos mayores es 0,026 en la transformación utilizada; la mediana es
0,0095. Hay casos extremos, pero no son el caso típico.

¹ Brier es error cuadrático respecto a victoria=1, tablas=0,5, derrota=0 de la
partida que produjo el historial. Se usa la transformación preexistente
`sigmoid(valor_interno/600)`, sin recalibrarla. Los resultados de esa política
no son etiquetas minimax; este banco de desarrollo tampoco es un test externo
de generalización. Los valores de la tabla son los de la última iteración
completa sin cota, no una probabilidad calibrada del movimiento final.

² La media resulta engañosa: muchas posiciones resueltas alcanzan profundidad
nominal 245 con variantes muy cortas. No son 245 plies efectivos ni evidencia
de fuerza. Se informa la mediana y se conservan profundidad selectiva y PV.

La mejora descriptiva de Brier de 32.768 a 262.144 es 0,00656, con intervalo
bootstrap por historial [0,00500; 0,00825]. La diferencia sigue positiva en el
subconjunto donde **ambas consultas terminaron con valor sin cota**: 1.135 estados
de 251 historiales, mejora por historial 0,00122 [0,00046; 0,00201]. La diferencia
de magnitud importa: no atribuir toda la ganancia de la primera tabla a una
mejora de la decisión final.

Hay 2.338/7.680 consultas cuya última iteración devuelve una cota. Si se mantienen
esas cotas como intervalos, el Brier final agregado queda entre [0,05644; 0,18391]
a 32.768 y [0,05631; 0,16670] a 262.144; se solapan ampliamente. El efecto sobre
todos los valores finales no queda identificado por ese cálculo conservador.
El subconjunto sin cotas tampoco es una muestra aleatoria. No hay un Elo nuevo.

**Retirar selectividad de forma amplia no resuelve el problema.** En 256 estados
se desactivaron las podas/reducciones enumeradas en el protocolo, conservando
alpha-beta, TT, quiescencia y reglas. A profundidad 4 cambió la jugada en 133/256;
el coste mediano fue **3,84 veces** los nodos de PR18 (253 consultas con nodos;
tres eran reclamaciones). A igual presupuesto cambió 124/256 jugadas a 32.768
y 141/256 a 262.144. No hay una mejora de Brier demostrada: con últimas
iteraciones completas el control empeora 0,00497 y 0,00769, respectivamente;
al restringir a finales sin cota, ambos intervalos incluyen cero. Esto descarta
recomendar ese control como solución; no identifica qué poda individual sería
responsable ni prueba que toda selectividad de PR18 sea correcta.

**La escala tiene un defecto de simetría, pero no aparece como explicación
dominante.** Al invertir filas, colores, turno y marcador, la salida NNUE sin
ajuste es idéntica en las 2.048 raíces. El ajuste interno heredado difiere en
96; el valor final difiere en 1.155. La diferencia final absoluta es de **3,50
cp de media**, 0,48 de mediana, 13,94 en el percentil 95 y 90,38 como máximo
(208 unidades internas = 100 cp UCI). No hay evidencia aquí de un error global
de unidades o un factor de escala grande; hay una dependencia de orientación
concreta, trazada al contrato heredado.

| Control aislado, 256 estados a 32.768 nodos | Jugadas distintas | Cambio de Brier¹ frente a PR18, positivo mejora |
|---|---:|---:|
| Escala material externa simétrica | 90 (35,2%) | −0,00111 [−0,00412; +0,00186] |
| Sin atenuación externa por reloj | 111 (43,4%) | −0,00012 [−0,00574; +0,00446] |

Ambos intervalos incluyen cero y la sensibilidad con finales sin cota mantiene
esa conclusión. Un efecto pequeño sobre el valor puede alterar la trayectoria
selectiva de búsqueda; el número de jugadas cambiadas no demuestra una mejora.
La escala simétrica sólo promedia la contribución externa de peones: no modifica
el ajuste `entertainment`. La intervención del reloj conserva la entrada de
reloj de la red y las reglas exactas; no elimina todas sus dependencias del reloj.

Retirar sólo la atenuación sí reduce el Brier **estático** en 0,00366
[0,00208; 0,00509] sobre todo el banco, pero esa mejora no se transmite de forma
demostrada al control de búsqueda. Por eso no basta optimizar una pérdida
offline y entrenar más datos bajo el mismo supuesto.

**Qué puede y qué no puede atribuirse a la evaluación.** El ranking de hijos
puramente estático coincide con PR18 a 262.144 nodos en 412/1.776 elecciones
comparables (23,2%); se excluyen hijos no terminales en jaque del ranking estático.
No es una tasa de error NNUE: la búsqueda considera respuestas y liquidaciones
que una sola evaluación no resuelve. Incluso en 99 controles donde ambas
búsquedas profundas coinciden en jugada y difieren como máximo 0,1 en la
transformación del valor, queda una diferencia estática/búsqueda. Esa selección
condicionada tampoco certifica el valor verdadero.

Los ejemplos concretos evitan confundir diagnósticos:

* `h045-p0207`: la evaluación estática da −1.051 unidades al negro; `a1a4`
  produce inmediatamente una victoria por mate/liquidación comprobada por el
  árbitro. PR18 la encuentra ya a 4.096 nodos. Es una carencia estática que la
  búsqueda corrige, no un fallo demostrado de la decisión del motor.
* `h144-p0222`: con ventaja de 17 puntos, el valor estático es +1.241, pero las
  tres búsquedas anuncian derrota en tres y la PV termina con resultado adverso
  verificado. El marcador no expresa por sí solo capacidad de conservar la
  victoria. Esa PV no certifica por sí sola todas las alternativas.
* `h065-p0081`: el último valor completo era +676 cp a los dos presupuestos
  menores, pero a 32.768 ya había una **cota superior −1.563 cp**. A 262.144 se
  informa derrota en cinco; la jugada `g3h3` no cambia. Es un ejemplo de valor
  y horizonte, y también de por qué no convertir una iteración antigua en el
  valor exacto de la jugada final.
* Los dos casos donde PR18 elige una derrota terminal inmediata
  (`h129-p0431`, `h241-p0307`) tienen **todas** sus jugadas legales perdedoras por
  reloj: siete y cinco alternativas comprobadas. No son dos errores de búsqueda.

Hay 51 raíces con alguna victoria terminal inmediata. En 50 el motor elige una
de ellas o una reclamación ganadora; en la restante (`h001-p0338`) escoge una
línea anunciada ganadora por mate en 11, cuya PV es legal y acaba en victoria,
en vez de cobrar por reloj de inmediato. No se clasifica como derrota ni se
deduce una superioridad de esa elección. Se verificaron 170 hijos terminales,
sin excluir ninguno.

**Prioridad que deja este diagnóstico.** Primero investigaría cómo representar
y alcanzar la liquidación favorable dentro del horizonte disponible: qué
decisiones cambian el resultado, qué eventos irreversibles importan y dónde
se pierde profundidad útil. Mantendría por ahora la red y el contrato de escala
para aislar ese trabajo. No aplicaría una desactivación general de podas ni
supondría que simetrizar la escala aporta fuerza por sí solo.

Antes de invertir en un transformador mayor o entrenable, hace falta una
referencia de decisiones mejor que la predicción de resultados de la política
de origen: terminales y familias resolubles por reglas, alternativas con historia
y estimaciones profundas explícitamente inciertas. Sólo después una comparación
con los mismos datos, presupuesto, escala y búsqueda entre cabeza congelada/
entrenable y transformador entrenable podría identificar capacidad de
representación. Este banco no hace esa comparación. **La hipótesis de límite
NNUE sigue abierta; no ha quedado confirmada ni refutada.**

**Verificación y preservación.** El árbitro independiente pasó 256 historias,
2.048 listas legales, 51.417 hijos, 170 liquidaciones y 256 comprobaciones de
clonación frente a replay completo. La revisión de 7.680 consultas pasó 74.386
plies de PV; se conservaron también las variantes completas anteriores cuando
diferían del movimiento final. Hashes, conteos y estadísticas se conciliaron.

Los fallos de instrumental se conservan: el primer build usó un enumerador
inexistente; tres adaptaciones del auditor corrigieron representación de muros,
marcas de piezas después de la liquidación y reloj/turno de presentación. El
cuarto rechazo de auditoría detectó la asociación incorrecta de PV completa y
bestmove final en 188 consultas. Se normalizaron las salidas guardadas en archivos
separados y se auditaron ambas variantes. No se retocaron motores, banco,
presupuestos ni resultados originales; no se repitieron las búsquedas.

Los procesos se registraron por PID y CreationDate con handles retenidos para
hijos directos. Los compiladores descendientes de CMake/Ninja no tienen un diario
individual: su salida es transitiva mediante las esperas síncronas y los códigos
de CMake, limitación conservada en la auditoría. El cierre final incluye prueba
de salida de los auxiliares registrados, inventario, archivo verificado y
liberación de lease. No se inicia automáticamente ninguna fase posterior.
