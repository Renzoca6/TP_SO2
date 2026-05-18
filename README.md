#TPE_SO

Como correrlo 
$ docker stop tp-so && docker rm tp-so
docker run --name tp-so -v "${PWD}:/root" --privileged -d agodio/itba-so-multiarch:3.1 tail -f /dev/null
sudo chown $USER:$USER "/home/renzo/SO/TP 2/TP_SO2/x64BareBones/Image/x64BareBonesImage.qcow2"
./compile.sh 
./run.sh 

# TP2 — Kernel de Sistema Operativo

**Materia:** Sistemas Operativos (72.11) — ITBA  
**Grupo:**  
**Integrantes:**

---

## Compilación y ejecución

---

## Comandos y tests

---

## Caracteres especiales

---

## Atajos de teclado

---

## Ejemplos de uso

---

## Limitaciones

---

## Requerimientos faltantes o parcialmente implementados

---

## Citas y uso de IA

Se utilizó inteligencia artificial (OpenCode / GPT) como herramienta de asistencia para el
desarrollo de los tests del administrador de memoria. La IA colaboró en:

- **test_generics.c** — tests portables a cualquier memory manager
  (alloc/free básico, no solapamiento, alineación, estrés, double free, punteros
  inválidos, out of memory, etc.).

- **test_buddy.c** — tests específicos del *buddy system* (reutilización exacta
  del bloque liberado, coalescencia de buddies, coalescencia parcial).

- **test_mm.c** — test de tipo *fuzz* indicado por la cátedra: ciclo infinito de
  alloc/free con tamaños aleatorios y verificación de no solapamiento entre
  bloques vivos.

Toda la lógica de estos tests fue revisada y validada por los integrantes del grupo.