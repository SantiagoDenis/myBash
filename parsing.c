#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>


#include "parsing.h"
#include "parser.h"
#include "command.h"

static scommand parse_scommand(Parser p) {
    assert(p != NULL);
    scommand sc = scommand_new();
    if (sc == NULL) return NULL;
    parser_skip_blanks(p);
    bool end = false;
    arg_kind_t kind;
    char * arg;
    while(!end) {
        arg = parser_next_argument(p, &kind);
        if (arg != NULL) {
            if(kind == ARG_NORMAL) {
                scommand_push_back(sc, arg);
            } else if (kind == ARG_INPUT) {
                 if(scommand_get_redir_in(sc) != NULL) {
                    free(arg);
                    scommand_destroy(sc);
                    return NULL;
                 } 
                 scommand_set_redir_in(sc, arg);
            } else {
                if(scommand_get_redir_out(sc) != NULL) {
                    free(arg);
                    scommand_destroy(sc);
                    return NULL;
                }
                scommand_set_redir_out(sc, arg);
            }
        } else {
            end = true;
        }
        
    }
    if (scommand_length(sc) == 0) {
        scommand_destroy(sc);
        return NULL;
    }
    return sc;

}

pipeline parse_pipeline(Parser parser) {
    assert(parser != NULL && !parser_at_eof(parser));

    pipeline result = pipeline_new();
    if (result == NULL) return NULL;

    bool error = false;
    bool another_pipe = false;
    bool set_wait = false;

    scommand cmd = parse_scommand(parser);
    if (cmd == NULL) {
        pipeline_destroy(result);
        return NULL;
    }
    pipeline_push_back(result, cmd);

    parser_skip_blanks(parser);
    parser_op_pipe(parser, &another_pipe);
    while (another_pipe && !error) {
        parser_skip_blanks(parser);
        cmd = parse_scommand(parser);
        if (cmd == NULL) {
            error = true;
            break;
        }
        pipeline_push_back(result, cmd);

        parser_skip_blanks(parser);
        parser_op_pipe(parser, &another_pipe);
    }

    parser_skip_blanks(parser);
    parser_op_background(parser, &set_wait);
    pipeline_set_wait(result, !set_wait);

    bool garbage = false;
    if (!parser_at_eof(parser)) {
        parser_garbage(parser, &garbage); /* consume el '\n' */
        if (garbage) {
            pipeline_destroy(result);
            return NULL;
        }
    }

    if (error) {
        pipeline_destroy(result);
        return NULL;
    }
    return result;
}