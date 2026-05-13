#ifndef PARSER_H
#define PARSER_H

#include "transaction.h"

int parse_args(int argc, char *argv[],
               char **accounts_file,
               char **trace_file,
               int *tick_ms);

int load_accounts(const char *path);
int load_trace(const char *path,
               Transaction *txs,
               int *tx_count,
               int max_tx);
int parse_operation(const char *line, Operation *op);

#endif /* PARSER_H */
