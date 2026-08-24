# OS - Custom Operating System

¡Bienvenido a **OS**! Este es un proyecto de desarrollo de un sistema operativo de código abierto desarrollado desde cero. El sistema está estructurado de forma modular para soportar arquitecturas específicas, gestión de memoria, controladores de hardware y un sistema de archivos básico.

## 📁 Estructura del Proyecto

El repositorio está organizado con las siguientes carpetas y componentes principales:

*   **`arch/`**: Código específico de la arquitectura del procesador (actualmente enfocado en `x86_64`).
*   **`boot/`**: Código del cargador de arranque (bootloader) encargado de inicializar el sistema.
*   **`kernel/`**: El núcleo del sistema operativo que gestiona los recursos del sistema.
*   **`drivers/`**: Controladores de hardware para interactuar con componentes físicos (teclado, pantalla, etc.).
*   **`hal/`**: Capa de Abstracción de Hardware (Hardware Abstraction Layer).
*   **`mm/`**: Administrador de memoria (Memory Management) física y virtual.
*   **`fs/`**: Sistema de archivos (File System) para la persistencia de datos.
*   **`lib/`**: Bibliotecas personalizadas internas utilizadas por el kernel.
*   **`user/`**: Espacio de usuario y programas de prueba.
*   **`tests/`**: Suite de pruebas unitarias y de integración para el sistema.

## 🛠️ Requisitos Previos

Para compilar y emular este sistema operativo, necesitarás las siguientes herramientas en tu entorno de desarrollo (Linux recomendado):

*   **Compilador GCC / Clang**: Configurado para compilación cruzada (`x86_64-elf-gcc`).
*   **GNU Make**: Para automatizar el proceso de construcción.
*   **NASM**: Ensamblador para las secciones de código en Assembly.
*   **QEMU**: Emulador para ejecutar el sistema operativo sin salir de tu máquina anfitriona.
*   **Grub / Xorriso**: Herramientas necesarias si deseas generar la imagen ISO ejecutable.

## 🚀 Compilación y Ejecución

El proyecto incluye un archivo `Makefile` automatizado para facilitar la construcción:

1.  **Clonar el repositorio:**
    ```bash
    git clone https://github.com
    cd OS
    ```

2.  **Compilar el proyecto:**
    Genera los binarios del kernel ejecutando:
    ```bash
    make
    ```

3.  **Ejecutar en el emulador (QEMU):**
    Para iniciar el sistema operativo directamente en la máquina virtual:
    ```bash
    make run
    ```

4.  **Limpiar los archivos generados:**
    Si deseas borrar los binarios compilados y temporales:
    ```bash
    make clean
    ```

## 📜 Licencia

Este proyecto está bajo la Licencia Pública General de GNU v2.0 (**GPL-2.0**). Consulta el archivo `LICENSE` incluido en este repositorio para obtener más detalles.
