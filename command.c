#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <glib.h>
#include "command.h"

struct scommand_s {
    GQueue* args;
    char* r_out;
    char* r_in;
};

scommand scommand_new(void) {
    scommand command = NULL;
    command = malloc(sizeof(struct scommand_s));
    if(command == NULL) {
        perror("Error with malloc");
        return NULL;
    }
    command->args = g_queue_new();
    if (command->args == NULL) {
        free(command);
        perror("new queue doesnt work");
        return NULL;
    }
    command->r_out = NULL;
    command->r_in = NULL; 
    return command;
}

scommand scommand_destroy(scommand self) {
    assert(self != NULL);
    g_queue_free_full(self->args, free);
    free(self->r_out);
    free(self->r_in);
    free(self);
    self = NULL;
    return self;
}

void scommand_push_back(scommand self, char* argument) {
    assert(self != NULL || argument != NULL);
    g_queue_push_tail(self->args, argument);
}



struct pipeline_s {
    GQueue* scommands;
    bool wait;
};

pipeline pipeline_new(void) {
    pipeline new_pipeline = NULL;
    new_pipeline = malloc(sizeof(struct pipeline_s));
    if(new_pipeline == NULL) {
        perror("Error with malloc");
        return NULL;
    }
    new_pipeline->scommands = g_queue_new();
    if(new_pipeline->scommands == NULL) {
        free(new_pipeline);
        perror("new queue doesnt work");
        return NULL;
    }
    new_pipeline->wait = true;
    return new_pipeline;
}

static void scommand_destroy_wrapper(gpointer data) {
    scommand_destroy((scommand)data);
}

pipeline pipeline_destroy(pipeline self) {
    if(self == NULL) return self;
    g_queue_free_full(self->scommands, scommand_destroy_wrapper);
    free(self);
    self = NULL;
    return self;
}

void pipeline_push_back(pipeline self, scommand sc) {
    assert(self != NULL && sc != NULL);
    g_queue_push_tail(self->scommands, sc);
}

void pipeline_pop_front(pipeline self) {
    assert(self!=NULL && !pipeline_is_empty(self));
    scommand kill = g_queue_pop_head(self->scommands);
    scommand_destroy(kill);
}

void pipeline_set_wait(pipeline self, const bool w) {
    assert(self != NULL);
    self->wait = w;
}

bool pipeline_is_empty(const pipeline self) {
    assert(self != NULL);
    return g_queue_is_empty(self->scommands);
}

unsigned int pipeline_length(const pipeline self) {
    assert(self != NULL);
    return g_queue_get_length(self->scommands);
}

scommand pipeline_front(const pipeline self) {
    assert(self != NULL && !pipeline_is_empty(self));
    return g_queue_peek_head(self->scommands);
}

