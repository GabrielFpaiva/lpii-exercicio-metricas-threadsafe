// metrics.h — Interface pública da biblioteca
#ifndef METRICS_H
#define METRICS_H

// Tipo opaco: o usuário não acessa campos internos
typedef struct Metrics Metrics;

// Snapshot imutável de um instante (retornado por metrics_snapshot)
typedef struct {
    char nome[32];
    long valor;
    long incrementos;  // Quantas vezes foi incrementado
} MetricaInfo;

typedef struct {
    int total;
    MetricaInfo *metricas;  // Array alocado por metrics_snapshot
    double uptime_seconds;  // Tempo desde metrics_create
} Snapshot;

// ═══ API ═══

// Criar coletor com capacidade para até max_metricas contadores
Metrics* metrics_create(int max_metricas);

// Destruir coletor e liberar toda memória
void metrics_destroy(Metrics* m);

// Registrar uma nova métrica pelo nome. Retorna 0=ok, -1=erro
// (nome duplicado ou capacidade excedida).
// Deve ser chamado ANTES de iniciar as threads (setup phase).
int metrics_register(Metrics* m, const char* nome);

// Incrementar uma métrica pelo nome. Retorna 0=ok, -1=não encontrada.
// Thread-safe: múltiplas threads podem chamar simultaneamente.
int metrics_increment(Metrics* m, const char* nome, long delta);

// Obter snapshot atômico de todas as métricas.
// Retorna uma cópia (o chamador deve liberar com snapshot_free).
// Thread-safe: pode ser chamado a qualquer momento.
Snapshot metrics_snapshot(Metrics* m);

// Liberar memória de um snapshot
void snapshot_free(Snapshot* snap);

// Resetar todas as métricas para zero.
// Thread-safe.
void metrics_reset(Metrics* m);

#endif
