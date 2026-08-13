# Parche moderno para Trampolín 5 / JumpStart 3rd Grade

[English README](README.md)

No hacen falta conocimientos técnicos. Este parche gratuito y no oficial crea
una copia portable para Windows moderno usando tu propio CD original. El
paquete no contiene el juego, sus recursos ni una imagen de disco.

## Descargar y crear el juego portable

1. Abre la [última versión estable](https://github.com/Miduin2/jumpstart-3rd-grade-1996-windows-fix/releases/latest).
2. Despliega **Assets** y descarga
   `Trampolin5-JumpStart3rdGrade-WindowsFix_1.0.2.zip`. No descargues los ZIP
   automáticos llamados `Source code`.
3. Descomprime el parche en una carpeta normal.
4. Inserta tu CD original o haz clic derecho sobre tu archivo `.iso` y elige
   **Montar**. Windows mostrará una unidad nueva, por ejemplo `F:`.
5. Haz doble clic en `Build portable game.cmd`.
6. Cuando pregunte por `CD root`, escribe la unidad montada, por ejemplo
   `F:\`. Esa ruta debe contener directamente las carpetas `3G` y `SUPPORT`;
   no es la carpeta donde guardaste el archivo ISO.
7. Pulsa Intro para aceptar la carpeta de salida propuesta o escribe una
   carpeta nueva que todavía no exista.

Si todo ha ido bien, la ventana terminará mostrando:

```text
Construction completed successfully.
```

Abre la carpeta indicada después de `Output:` y ejecuta:

- `Play Trampolin 5.exe` para la edición española;
- `Play JumpStart 3rd Grade.exe` para la edición inglesa.

El CD ya no será necesario para jugar.

## Ediciones verificadas

- *Trampolín Educación Primaria 5.º Curso* en español;
- *JumpStart 3rd Grade* en inglés.

Ambas se probaron manualmente en Windows 10 x64: imagen, música, voces y
efectos, ratón, menús y cierre limpio. Windows 11 todavía no se ha validado
formalmente.

El núcleo técnico es común y puede adaptarse a otras ediciones, pero el
constructor automático solo acepta las dos versiones comprobadas. Consulta
la [guía para portar otras ediciones](PORTING_OTHER_EDITIONS.md) si tienes otra
versión.

## Si aparece un error

La ventana permanece abierta para que puedas leerlo. Al informar del problema,
indica la edición, versión de Windows, resolución/escalado, dispositivo de
audio y el texto exacto del error. No subas archivos del juego ni imágenes de
disco.

Este proyecto independiente no está afiliado con Knowledge Adventure,
Davidson & Associates, Havas, Microsoft ni ningún titular actual de derechos.
