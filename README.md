# Biblioteca de métricas thread-safe

Exercício prático de LPII na UFPB.
Contadores nomeados que várias threads incrementam ao mesmo tempo, com um thread
de monitoramento lendo snapshots.

`metrics.h` e `main.c` vieram prontos no enunciado. O que eu escrevi é o
`metrics.c`.

## Compilar

```sh
make run
```

Ou direto:

```sh
gcc -Wall -Wextra -pthread -o metrics_test metrics.c main.c
./metrics_test
```

Mudei uma linha do `main.c` fornecido: `#include <string.h>`. Sem ela o arquivo
chama `strcmp` sem declaração, o que dá erro no clang e warning no gcc.

## Decisões

`struct Metrics` fica só no `metrics.c`. O header expõe o typedef, então quem usa
a biblioteca não alcança os campos.

Um `pthread_mutex_t` protege `valor` e `incrementos`. A busca pelo nome roda fora
dele, porque `nome` e `count` só são escritos pelo `metrics_register`, que a API
exige na fase de setup, antes de qualquer thread existir. O `pthread_create`
posterior garante a visibilidade, então essa leitura não corre com nenhuma
escrita.

`metrics_snapshot` faz `malloc` e `memcpy` do array com o lock preso e devolve a
cópia por valor. O chamador nunca recebe ponteiro para dado interno.
`snapshot_free` libera o array e zera o struct.

## Verificações

| Verificação | Resultado |
|---|---|
| `-Wall -Wextra` | zero warnings |
| `requests` no final | 60000 de 60000 |
| Reset | OK |
| `make tsan` | nenhuma data race |
| `leaks --atExit` | 0 leaks |

Valgrind não roda em macOS arm64, então usei o ThreadSanitizer e o `leaks` da
Apple no lugar do Helgrind. Em Linux tem `make helgrind` e `make memcheck`.
