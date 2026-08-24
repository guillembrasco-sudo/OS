# Política de Seguridad (Security Policy)

¡Gracias por ayudar a mantener **OS** seguro! Nos tomamos muy en serio la seguridad de nuestro sistema operativo, pero al ser un proyecto educativo y de desarrollo personal, gestionamos las vulnerabilidades de forma abierta.

## Versiones Soportadas

Actualmente, solo realizamos correcciones de seguridad en la rama principal de desarrollo:

| Versión | Soportada |
| ------- | --------- |
| Rama `main` / `master` |  Sí |
| Versiones anteriores  |  No |

## Cómo reportar una vulnerabilidad

Si encuentras un fallo de seguridad, un desbordamiento de búfer (buffer overflow), un pánico del kernel provocado o cualquier otra vulnerabilidad en el código:

1. **Abre un Issue directamente:** No es necesario que envíes correos electrónicos privados. Puedes registrar el problema abriendo un nuevo reporte en la pestaña de **Issues** de este repositorio.
2. **Describe el fallo:** Incluye los pasos necesarios para reproducir el error, el comportamiento esperado y qué componente (`arch`, `mm`, `kernel`, `drivers`, etc.) se ve afectado.
3. **Añade logs si es posible:** Si el error ocurre al emular en QEMU, adjunta la salida de la consola o el contenido relevante de tu archivo `qemu.log`.
