# Trampolín 5 / JumpStart 3rd Grade: parche portable para Windows moderno

## Resultado validado

El mismo parche sin contenido propietario reconstruye y ejecuta estas dos
ediciones desde su CD original:

- *Trampolín Educación Primaria 5.º Curso* (español);
- *JumpStart 3rd Grade* (inglés).

El 13 de agosto de 2026 se validaron manualmente en Windows 10 x64 tanto la
imagen como música, voces y efectos. En ambas se reprodujo también la salida
desde la pantalla inicial, que antes dejaba el proceso en negro y terminaba en
`AppHangB1`. Con el arreglo definitivo ambas muestran solamente su breve negro
normal y se cierran solas.

La reconstrucción pública parte exclusivamente de los archivos aportados por
el usuario. No distribuye `3G.EXE`, recursos, `Sound.bal`, la imagen de disco ni
la DLL WinG derivada.

## Ediciones identificadas

| Edición | `3G.EXE` SHA-256 | `WSOUND32.DLL` SHA-256 |
| --- | --- | --- |
| Trampolín 5 | `6C7FF278C39ACFADD5611F7EE996F8336F53A9EE52DDE34A4DAEC00CFB8936DD` | `48C5CCDF926246C27700BC6F2E31654204525FBAF99A65D8E13EC60B9299F4BD` |
| JumpStart 3rd Grade | `63F72788226CA073F6C813008FF3FF889E2C8D93F10179E8CCF994C316CFDC03` | `D1867FB71B62DAAC6A1FB5A9A50864074279375FB64D10DB602F33C3E1EE41FF` |

La WinG original es idéntica en los dos CD y tiene SHA-256
`BB1F552E2525E784B61D2FE0CA23F3402ADEC05AA5F92F4C1DFBEA3966A84CBB`.

## Reconstrucción desde CD

### Español

Se conserva la semántica del instalador mediante dos raíces:

- `hd`: capas `SUPPORT/W32`, `SUPPORT/CMN` y `SUPPORT/WGM32`, seguidas por los
  archivos de `3G` que todavía no existan;
- `cd/3G`: copia de los datos del directorio `3G` del CD.

No se sobrescriben los homónimos instalados con versiones del CD. Esto conserva
marcadores como `WGM32.TXT` y las variantes correctas de los INI. Las primeras
pruebas que fusionaban todo indiscriminadamente mantenían imagen pero rompían
el arranque del subsistema de sonido.

### Inglés

El aplicador construye una raíz `game` sin depender de ninguna instalación
previa:

1. copia `3G/32BIT`;
2. copia los archivos planos de `3G`, que sustituyen sus homónimos;
3. añade únicamente los archivos ausentes de `SUPPORT/W32`, `SUPPORT/CMN` y
   `SUPPORT/WGM32`;
4. añade `3G.CNT` y `3G.HLP` si faltan.

Esta receta reprodujo byte a byte los 106 archivos propietarios de la antigua
copia funcional inglesa. Eliminó la dependencia circular de usar esa misma
copia como supuesta entrada “instalada”.

## WinG local y presentación

En una copia local de WinG se sustituyen solamente los bytes `75 11` del
offset `0xA55` por `90 90`. Se elimina así la comprobación que exigía cargar
WinG desde el directorio del sistema. El resultado se guarda como
`WING32.legacy.dll` y tiene SHA-256
`EDD26762E7DFD37C5A4306698C77D1A0C4C1F7E734946B3B82C534FAC13065F6`.

El proxy local `WING32.DLL`:

- reenvía las diez funciones originales al backend local;
- detecta el lienzo 640×480 y lo presenta a pantalla completa sin bordes;
- conserva la relación 4:3 y rellena el resto en negro;
- traduce las coordenadas del ratón al lienzo original;
- mantiene una ventana utilizable con Alt+Tab;
- no escribe trazas en producción.

Su binario reproducible definitivo tiene SHA-256
`3B85879F38108E49B93A414CCA78D7F91575D3A7C692DC4D995B2662974AE4DE`.

## Ruta corta y audio español

La `WSOUND32.DLL` española construye la ruta de `Sound.bal` en un búfer fijo.
Con rutas modernas largas falla antes de abrir el dispositivo de audio. El
lanzador crea temporalmente una letra libre entre `V:` y `Z:` y ejecuta el
juego desde esa ruta corta. La letra se elimina al terminar.

Las trazas comparativas conservaron exactamente los mismos 156 archivos:

- con ruta larga, el subsistema devolvía error antes de `waveOutOpen`;
- con el alias corto, abría el dispositivo y reproducía música, voces y
  efectos.

## Cuelgue al salir: causa y arreglo

El fallo de cierre dependía del estado. Después de jugar podía cerrar bien,
pero al salir desde la pantalla inicial destruía la ventana y dejaba vivo el
proceso. Windows registraba `AppHangB1`, tipo `Cross-thread`, firma `44b3`, con
el título interno “A Broadway Application”.

Una captura de las pilas del proceso negro aisló el hilo principal en:

```text
WSOUND32.DLL+0x10392
winmmbase.dll!waveOutClose
wdmaud.drv!wodMessage
KERNELBASE.dll!WaitForSingleObject
ntdll.dll!NtWaitForSingleObject
```

Los demás hilos ya estaban dentro de `LdrShutdownThread` o terminando COM. La
hipótesis anterior de una DLL WinG descargada quedó descartada: tanto el proxy
como `WING32.legacy.dll` seguían cargados en el informe WER.

El proxy intercepta la importación `waveOutClose` de `WSOUND32.DLL` cuando el
sonido ya está cargado. Ejecuta primero `waveOutReset` y realiza el cierre en
un hilo auxiliar. Los cierres normales conservan su resultado real; si el
controlador no retorna en dos segundos, el hilo principal recibe éxito y
continúa el apagado. No se termina por la fuerza el hilo auxiliar mientras
pueda poseer bloqueos del controlador.

La traza de validación española mostró dos cierres normales y un tercero que
agotó el límite; a continuación se recibieron `WM_DESTROY`, `WM_NCDESTROY`,
dos `PostQuitMessage` y `ExitProcess(0)`. No quedó proceso ni evento WER. La
edición inglesa, con una `WSOUND32.DLL` distinta, cerró también correctamente
desde la salida reconstruida por el aplicador público.

## Lanzador y estado reversible

El lanzador x64 reproducible tiene SHA-256
`9F6E951B296B1D192BF6967D24B6D5FFFFF912581513DA95EAFB9DA165A525A5`.
Detecta el diseño español o inglés, escribe temporalmente las rutas necesarias
en `KA.INI` y `3G.INI`, espera al proceso hijo y restaura ambos archivos aunque
el juego termine con error. También restaura los colores del sistema y elimina
la letra temporal.

Después de las pruebas finales los manifiestos no mostraron ningún archivo
modificado ni extra, y `KA.INI` volvió a quedar ausente cuando no existía antes.

## Reproducibilidad

`src/trampolin5launcher/rebuild-clean-tests.ps1` generó
`workspace/repro-tests/trampolin-jumpstart-clean-rebuild-v011` desde los dos
árboles de CD originales. Frente a v007, la única diferencia de manifiesto fue
el nuevo proxy de cierre.

`src/trampolin5launcher/build-portable-from-cd.ps1` generó después
`workspace/repro-tests/trampolin-jumpstart-public-applicator-v002`. Sus dos
salidas coincidieron byte a byte con v011:

- español: 213 archivos totales, 0 diferencias;
- inglés: 111 archivos totales, 0 diferencias.

El aplicador rechaza hashes desconocidos, una salida preexistente y una salida
dentro de la fuente. Si ocurre un error, solo elimina la carpeta nueva parcial
que él mismo haya creado.

La primera simulación manual realizada directamente contra la ISO española
montada detectó una diferencia que las extracciones de laboratorio ocultaban:
el CD marca `SUPPORT\CMN\3G.GID` como oculto y todos sus archivos como solo
lectura. La versión 1.0.1 enumera archivos con `-Force`, limpia en la salida los
atributos propios del medio óptico y verifica de nuevo los 213 archivos contra
el manifiesto. La reconstrucción directa desde `F:\` pasó completa, con cero
archivos ocultos, de sistema o de solo lectura en la portable resultante.

## Publicación

El proxy, el lanzador, el parche local de WinG y la mitigación de cierre de
audio forman un único núcleo compartido por las dos ediciones verificadas. No
son dos arreglos técnicos independientes. Lo específico de cada edición es la
receta que reconstruye los archivos seleccionados por su instalador y, cuando
corresponde, la detección que decide sus rutas y contenido INI. Por eso un hash
de `3G.EXE` desconocido se rechaza en el constructor estable aunque el núcleo
pueda ser reutilizable.

`PORTING_OTHER_EDITIONS.md` documenta cómo evaluar otras variantes sin rebajar
esa frontera de seguridad. Una edición adicional solo debe anunciarse como
compatible después de reconstruirla de forma determinista y probar imagen,
entrada, audio, persistencia y cierre.

El paquete publicable debe contener únicamente:

- el aplicador PowerShell;
- el proxy y el lanzador originales del proyecto;
- los dos manifiestos de hashes;
- código fuente, instrucciones y licencia.

El usuario apunta `-SourceRoot` a la unidad del CD montado (no a la carpeta que
contiene la ISO) o a una extracción completa, y elige un `-OutputRoot`
inexistente. El aplicador identifica automáticamente la edición y verifica
todos los archivos generados antes de anunciar éxito. La versión 1.0.1 incluye
además `Build portable game.cmd` para guiar estas dos elecciones sin exigir que
el usuario componga el comando PowerShell.

Fuentes internas principales:

- proxy: `src/trampolin5wing`;
- lanzador y aplicador: `src/trampolin5launcher`;
- reconstrucción final: `workspace/repro-tests/trampolin-jumpstart-clean-rebuild-v011`;
- salida pública simulada: `workspace/repro-tests/trampolin-jumpstart-public-applicator-v002`.

## Actualización documental 1.0.2

La versión 1.0.2 conserva sin cambios el constructor, el proxy y el lanzador
validados en 1.0.1. Su finalidad es sincronizar el ZIP descargable con la
documentación posterior de `main`: recorrido inicial simplificado, guía breve
en español y adaptación experimental de otras ediciones. La 1.0.1 permanece
publicada e inmutable como referencia histórica del artefacto probado
manualmente.
