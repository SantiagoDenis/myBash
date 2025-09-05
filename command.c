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
    command = malloc(sizeof *command);
    if(command == NULL) {
        perror("Error with malloc");
        return NULL;
    }
    command->args = g_queue_new();
    if (command->args == NULL) {
        perror("new queue doesnt work");
        free(command);
        return NULL;
    }
    command->r_out = NULL;
    command->r_in = NULL; 
    return command;
}

scommand scommand_destroy(scommand self) {
    assert(self != NULL);
    g_queue_free_full(self->args, (GDestroyNotify)g_free);
    g_free(self->r_out);
    g_free(self->r_in);
    free(self);
    return NULL;
}
static void scommand_destroy_wrapper(void *ptr) {
    if (ptr != NULL) {
        scommand_destroy((scommand)ptr);
    }
}
void scommand_push_back(scommand self, char *argument) {
    assert(self != NULL && argument != NULL);
    g_queue_push_tail(self->args, g_strdup(argument));
}

void scommand_pop_front(scommand self) {
    assert(self != NULL && !scommand_is_empty(self));
    char *kill = g_queue_pop_head(self->args);
    g_free(kill);
}

void scommand_set_redir_in(scommand self, char *filename){
    assert(self != NULL);
    g_free(self->r_in);
    self->r_in = (filename != NULL) ? g_strdup(filename) : NULL;
}

void scommand_set_redir_out(scommand self, char *filename) {
    assert(self != NULL);
    g_free(self->r_out);
    self->r_out = (filename != NULL) ? g_strdup(filename) : NULL;
}

bool scommand_is_empty(const scommand self) {
    assert(self != NULL);
    return g_queue_is_empty(self->args);
}

unsigned int scommand_length(const scommand s){
    assert(s != NULL);
    return g_queue_get_length(s->args);
}

char * scommand_front(const scommand self){
    assert(self != NULL && !scommand_is_empty(self));
    return (char *) g_queue_peek_head(self->args);
}

char * scommand_get_redir_in(const scommand self){
    assert(self != NULL);
    return self->r_in;
}

char * scommand_get_redir_out(const scommand self){
    assert(self != NULL);
    return self->r_out;
}

char * scommand_to_string(const scommand self){
    assert(self != NULL);
    GString *s = g_string_new(""); 
    
    for (GList *current = g_queue_peek_head_link(self->args); current != NULL; current = current->next ) {
        g_string_append(s,(char *)current->data);
        if(current->next != NULL) {
            g_string_append_c(s, ' ');
        }
        
    }

    if(self->r_in != NULL){
        if(!scommand_is_empty(self)) {
            g_string_append_c(s, ' ');
        }
        g_string_append(s, "< ");
        g_string_append(s,self->r_in);
    }

     if(self->r_out != NULL){
        if(!scommand_is_empty(self) || self->r_in != NULL) {
            g_string_append_c(s, ' ');
        }
        g_string_append(s, "> ");
        g_string_append(s, self->r_out);
     }

     char *res = g_string_free(s, FALSE);
     return res;
}

struct pipeline_s {
    GQueue * scommands;
    bool wait;
};

pipeline pipeline_new(void){
    pipeline new = NULL;
    new = malloc(sizeof(struct pipeline_s));
    if(new== NULL) {
        perror("Error with malloc");
        return NULL;
    }
    new->scommands = g_queue_new();
    if (new->scommands == NULL) {
        perror("g_queue_new");
        free(new);
        return NULL;
    }
    new->wait = true;
    return new;
}


pipeline pipeline_destroy(pipeline self){
    assert(self !=NULL);
    g_queue_free_full(self->scommands, scommand_destroy_wrapper);
    free(self);
    return NULL;
}

void pipeline_pop_front(pipeline self){
    assert(self != NULL && !pipeline_is_empty(self));
    scommand kill = g_queue_pop_head(self->scommands);
    scommand_destroy(kill);
}

bool pipeline_is_empty(const pipeline self){
    assert(self != NULL);
    return g_queue_is_empty(self->scommands);
}

unsigned int pipeline_length(const pipeline self) {
    assert(self != NULL);
    return g_queue_get_length(self->scommands);
}

scommand pipeline_front(const pipeline self) {
    assert(self != NULL && !pipeline_is_empty(self));
    return (scommand) g_queue_peek_head(self->scommands);
}

bool pipeline_get_wait(const pipeline self) {
    assert(self != NULL);
    return self->wait;
}

char * pipeline_to_string(const pipeline self) {
    assert(self != NULL);
    GString * s = g_string_new("");
    for (GList *current = g_queue_peek_head_link(self->scommands); current != NULL; current = current->next ) {
        char * tmp = scommand_to_string(current->data);
        g_string_append(s,tmp);
        g_free(tmp);

        if(current->next != NULL) {
            g_string_append(s, " | ");
        }
        
    }
    if(!self->wait && !g_queue_is_empty(self->scommands)) {
        g_string_append(s, " &");
    }
    char * res = g_string_free(s, FALSE);
    return res;
}

void pipeline_push_back(pipeline self, scommand sc){
    assert(self != NULL && sc != NULL);
    g_queue_push_tail(self->scommands, sc);
}


void pipeline_set_wait(pipeline self, const bool w){
    assert (self != NULL);
    self->wait = w;   
}

