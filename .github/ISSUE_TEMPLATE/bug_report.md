---
name: Bug report
about: Reporta un fallo, comportamiento inesperado o pánico del kernel (triple fault,
  etc.)
title: "[BUG] "
labels: bug
assignees: ''

---

**Descripción del error**
Una descripción clara y concisa de lo que ocurre de forma inesperada en el sistema.

**Pasos para reproducir**
1. Ejecutar el comando `make`...
2. Iniciar QEMU con `make run`...
3. Realizar la acción...
4. Ver el fallo en la pantalla/consola.

**Comportamiento esperado**
Qué es lo que debería haber hecho el sistema operativo en lugar de fallar.

**Entorno de desarrollo**
- Arquitectura/Procesador (ej. x86_64)
- Emulador o hardware real (ej. QEMU vX.X)
- Sistema operativo anfitrión (ej. Ubuntu 24.04, WSL2)

**Información adicional y Logs**
Añade capturas de pantalla, volcados de registros de la CPU o cualquier salida relevante del archivo `qemu.log`.
