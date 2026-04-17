#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>

#define LOG_CHUNK_SIZE 4096
#define LOG_BUFFER_CAPACITY 64
#define CONTAINER_ID_LEN 32
#define LOG_DIR "logs"

/* ---------------- STRUCTS ---------------- */
typedef struct {
    char container_id[CONTAINER_ID_LEN];
    size_t length;
    char data[LOG_CHUNK_SIZE];
} log_item_t;

typedef struct {
    log_item_t items[LOG_BUFFER_CAPACITY];
    size_t head, tail, count;
    int shutting_down;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} bounded_buffer_t;

typedef struct {
    int read_fd;
    char container_id[CONTAINER_ID_LEN];
    bounded_buffer_t *log_buffer;
} reader_args_t;

/* ---------------- BUFFER ---------------- */
void bounded_buffer_init(bounded_buffer_t *b) {
    memset(b, 0, sizeof(*b));
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->not_empty, NULL);
    pthread_cond_init(&b->not_full, NULL);
}

void bounded_buffer_destroy(bounded_buffer_t *b) {
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->not_empty);
    pthread_cond_destroy(&b->not_full);
}

int bounded_buffer_push(bounded_buffer_t *b, const log_item_t *item) {
    pthread_mutex_lock(&b->mutex);

    while (b->count == LOG_BUFFER_CAPACITY && !b->shutting_down)
        pthread_cond_wait(&b->not_full, &b->mutex);

    if (b->shutting_down) {
        pthread_mutex_unlock(&b->mutex);
        return -1;
    }

    b->items[b->tail] = *item;
    b->tail = (b->tail + 1) % LOG_BUFFER_CAPACITY;
    b->count++;

    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->mutex);
    return 0;
}

int bounded_buffer_pop(bounded_buffer_t *b, log_item_t *item) {
    pthread_mutex_lock(&b->mutex);

    while (b->count == 0) {
        if (b->shutting_down) {
            pthread_mutex_unlock(&b->mutex);
            return 1;
        }
        pthread_cond_wait(&b->not_empty, &b->mutex);
    }

    *item = b->items[b->head];
    b->head = (b->head + 1) % LOG_BUFFER_CAPACITY;
    b->count--;

    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->mutex);
    return 0;
}

/* ---------------- PRODUCER ---------------- */
void *pipe_reader_thread(void *arg) {
    reader_args_t *ra = (reader_args_t *)arg;
    FILE *fp = fdopen(ra->read_fd, "r");

    char line[LOG_CHUNK_SIZE];

    while (fgets(line, sizeof(line), fp)) {
        log_item_t item;
        memset(&item, 0, sizeof(item));

        strcpy(item.container_id, ra->container_id);
        item.length = strlen(line);
        memcpy(item.data, line, item.length);

        if (bounded_buffer_push(ra->log_buffer, &item) != 0)
            break;
    }

    fclose(fp);
    free(ra);
    return NULL;
}

/* ---------------- CONSUMER ---------------- */
void *logging_thread(void *arg) {
    bounded_buffer_t *buf = (bounded_buffer_t *)arg;
    log_item_t item;

    mkdir(LOG_DIR, 0755);

    while (bounded_buffer_pop(buf, &item) == 0) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s.log", LOG_DIR, item.container_id);

        int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) continue;

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);

        char ts[32];
        strftime(ts, sizeof(ts), "[%H:%M:%S] ", tm_info);

        write(fd, ts, strlen(ts));
        write(fd, item.data, item.length);

        close(fd);
    }

    return NULL;
}

/* ---------------- MAIN ---------------- */
int main() {

    bounded_buffer_t buffer;
    bounded_buffer_init(&buffer);

    pthread_t logger;
    pthread_create(&logger, NULL, logging_thread, &buffer);

    int pipefd[2];
    pipe(pipefd);

    reader_args_t *ra = malloc(sizeof(reader_args_t));
    ra->read_fd = pipefd[0];
    strcpy(ra->container_id, "alpha");
    ra->log_buffer = &buffer;

    pthread_t reader;
    pthread_create(&reader, NULL, pipe_reader_thread, ra);

    printf("Type logs (exit to stop):\n");

    char input[256];

    while (1) {
        fgets(input, sizeof(input), stdin);

        if (strncmp(input, "exit", 4) == 0)
            break;

        write(pipefd[1], input, strlen(input));
    }

    close(pipefd[1]);

    pthread_join(reader, NULL);

    buffer.shutting_down = 1;
    pthread_cond_broadcast(&buffer.not_empty);

    pthread_join(logger, NULL);

    bounded_buffer_destroy(&buffer);

    printf("Logs saved in logs/alpha.log\n");

    return 0;
}
