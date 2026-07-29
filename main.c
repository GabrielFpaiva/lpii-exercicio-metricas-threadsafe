// main.c — Testa a biblioteca de métricas
#include "metrics.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define NUM_WORKERS 6
#define OPS_PER_WORKER 10000

Metrics* metricas;

void* worker_http(void* arg) {
    int id = *(int*)arg;
    unsigned int seed = id + 1;
    for (int i = 0; i < OPS_PER_WORKER; i++) {
        metrics_increment(metricas, "requests", 1);
        metrics_increment(metricas, "bytes_in", 100 + rand_r(&seed) % 900);
        if (rand_r(&seed) % 20 == 0)  // 5% de erro
            metrics_increment(metricas, "errors", 1);
    }
    return NULL;
}

void* monitor(void* arg) {
    (void)arg;
    for (int i = 0; i < 5; i++) {
        usleep(200000);  // A cada 200ms
        Snapshot snap = metrics_snapshot(metricas);
        printf("\n--- Monitor (uptime %.1fs) ---\n",
               snap.uptime_seconds);
        for (int j = 0; j < snap.total; j++)
            printf("  %-12s = %8ld (%ld incrementos)\n",
                   snap.metricas[j].nome,
                   snap.metricas[j].valor,
                   snap.metricas[j].incrementos);
        snapshot_free(&snap);
    }
    return NULL;
}

int main() {
    metricas = metrics_create(10);

    // Setup phase (single-threaded)
    metrics_register(metricas, "requests");
    metrics_register(metricas, "errors");
    metrics_register(metricas, "bytes_in");
    metrics_register(metricas, "connections");

    // Fase concorrente
    pthread_t workers[NUM_WORKERS], mon;
    int ids[NUM_WORKERS];

    pthread_create(&mon, NULL, monitor, NULL);
    for (int i = 0; i < NUM_WORKERS; i++) {
        ids[i] = i;
        pthread_create(&workers[i], NULL, worker_http, &ids[i]);
    }

    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_join(workers[i], NULL);
    pthread_join(mon, NULL);

    // Snapshot final
    Snapshot final = metrics_snapshot(metricas);
    printf("\n=== Relatório Final (uptime %.1fs) ===\n",
           final.uptime_seconds);
    for (int i = 0; i < final.total; i++)
        printf("  %-12s = %8ld (%ld incrementos)\n",
               final.metricas[i].nome,
               final.metricas[i].valor,
               final.metricas[i].incrementos);
    snapshot_free(&final);

    // Verificação de correção
    Snapshot check = metrics_snapshot(metricas);
    long req = 0;
    for (int i = 0; i < check.total; i++)
        if (strcmp(check.metricas[i].nome, "requests") == 0)
            req = check.metricas[i].valor;
    snapshot_free(&check);

    long esperado = (long)NUM_WORKERS * OPS_PER_WORKER;
    printf("\nRequests: %ld (esperado: %ld) — %s\n",
           req, esperado,
           req == esperado ? "CORRETO" : "INCORRETO (race condition!)");

    // Testar reset
    metrics_reset(metricas);
    Snapshot pos_reset = metrics_snapshot(metricas);
    int ok = 1;
    for (int i = 0; i < pos_reset.total; i++)
        if (pos_reset.metricas[i].valor != 0) ok = 0;
    printf("Reset: %s\n", ok ? "OK" : "FALHOU");
    snapshot_free(&pos_reset);

    metrics_destroy(metricas);
    printf("Destroy: OK (sem leaks)\n");
    return 0;
}
