#ifndef __AUDPIPE_CLI
#define __AUDPIPE_CLI

// Parse the CLI arguments. This is mostly just to keep main very empty, like I
// prefer. Plus the CLI code is a tad ugly, and we might want some other ways to
// interact with the application down the line(?)
int parse_cli(int argc, char **argv);

#endif