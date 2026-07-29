// metrics.c — Implementação da biblioteca de métricas thread-safe
#define _POSIX_C_SOURCE 200809L

#include "metrics.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Tipo opaco: a struct só existe aqui, o usuário da biblioteca
// enxerga apenas o ponteiro declarado no header.
struct Metrics {
    MetricaInfo *metricas;   // array com capacidade fixa
    int count;               // quantas métricas registradas
    int capacidade;          // max_metricas passado no create
    pthread_mutex_t mutex;   // protege valor/incrementos
    struct timespec criacao; // marco zero do uptime
};

// Busca o índice de uma métrica pelo nome. Retorna -1 se não existir.
//
// Roda FORA do mutex de propósito: nome e count só são escritos por
// metrics_register, que a API exige que seja chamado na fase de setup,
// antes de qualquer thread subir. O pthread_create posterior estabelece
// o happens-before, então essa leitura nunca corre com uma escrita —
// e o lock fica reservado só para a operação em si.
static int indice_de(const Metrics *m, const char *nome) {
    int total = m->count;
    for (int i = 0; i < total; i++)
        if (strcmp(m->metricas[i].nome, nome) == 0)
            return i;
    return -1;
}

static double intervalo_em_segundos(const struct timespec *inicio,
                                    const struct timespec *fim) {
    return (double)(fim->tv_sec - inicio->tv_sec)
         + (double)(fim->tv_nsec - inicio->tv_nsec) / 1e9;
}

Metrics* metrics_create(int max_metricas) {
    if (max_metricas <= 0) return NULL;

    Metrics *m = malloc(sizeof *m);
    if (!m) return NULL;

    m->metricas = calloc((size_t)max_metricas, sizeof *m->metricas);
    if (!m->metricas) {
        free(m);
        return NULL;
    }

    if (pthread_mutex_init(&m->mutex, NULL) != 0) {
        free(m->metricas);
        free(m);
        return NULL;
    }

    m->count = 0;
    m->capacidade = max_metricas;
    clock_gettime(CLOCK_MONOTONIC, &m->criacao);
    return m;
}

void metrics_destroy(Metrics* m) {
    if (!m) return;
    pthread_mutex_destroy(&m->mutex);
    free(m->metricas);
    free(m);
}

int metrics_register(Metrics* m, const char* nome) {
    if (!m || !nome) return -1;

    // Nome precisa caber no campo com o '\0'; truncar criaria duas
    // métricas distintas com o mesmo nome guardado.
    size_t tamanho = strlen(nome);
    if (tamanho == 0 || tamanho >= sizeof m->metricas[0].nome) return -1;

    pthread_mutex_lock(&m->mutex);

    if (m->count >= m->capacidade) {
        pthread_mutex_unlock(&m->mutex);
        return -1;
    }
    for (int i = 0; i < m->count; i++) {
        if (strcmp(m->metricas[i].nome, nome) == 0) {
            pthread_mutex_unlock(&m->mutex);
            return -1;
        }
    }

    MetricaInfo *nova = &m->metricas[m->count];
    memcpy(nova->nome, nome, tamanho + 1);
    nova->valor = 0;
    nova->incrementos = 0;
    m->count++;

    pthread_mutex_unlock(&m->mutex);
    return 0;
}

int metrics_increment(Metrics* m, const char* nome, long delta) {
    if (!m || !nome) return -1;

    int i = indice_de(m, nome);  // busca fora da seção crítica
    if (i < 0) return -1;

    pthread_mutex_lock(&m->mutex);
    m->metricas[i].valor += delta;
    m->metricas[i].incrementos++;
    pthread_mutex_unlock(&m->mutex);
    return 0;
}

Snapshot metrics_snapshot(Metrics* m) {
    Snapshot snap = { .total = 0, .metricas = NULL, .uptime_seconds = 0.0 };
    if (!m) return snap;

    struct timespec agora;
    clock_gettime(CLOCK_MONOTONIC, &agora);

    pthread_mutex_lock(&m->mutex);

    int total = m->count;
    MetricaInfo *copia = NULL;
    if (total > 0) {
        copia = malloc((size_t)total * sizeof *copia);
        if (copia)
            memcpy(copia, m->metricas, (size_t)total * sizeof *copia);
        else
            total = 0;  // sem memória: devolve snapshot vazio
    }

    pthread_mutex_unlock(&m->mutex);

    snap.total = total;
    snap.metricas = copia;
    snap.uptime_seconds = intervalo_em_segundos(&m->criacao, &agora);
    return snap;
}

void snapshot_free(Snapshot* snap) {
    if (!snap) return;
    free(snap->metricas);
    snap->metricas = NULL;
    snap->total = 0;
}

void metrics_reset(Metrics* m) {
    if (!m) return;

    pthread_mutex_lock(&m->mutex);
    for (int i = 0; i < m->count; i++) {
        m->metricas[i].valor = 0;
        m->metricas[i].incrementos = 0;
    }
    pthread_mutex_unlock(&m->mutex);
}
